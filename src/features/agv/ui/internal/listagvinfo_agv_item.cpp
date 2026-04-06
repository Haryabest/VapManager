#include "listagvinfo.h"

AgvItem::AgvItem(const AgvInfo &info, std::function<int(int)> scale, QWidget *parent)
    : QFrame(parent), agv(info), s(scale)
{
    setObjectName("agvItem");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    setStyleSheet(
        "#agvItem{background:white;border-radius:10px;border:1px solid #E0E0E0;}"
        "#agvItem:hover{background:#F7F7F7;}"
    );

    QHBoxLayout *root = new QHBoxLayout(this);
    root->setContentsMargins(s(12), s(10), s(12), s(10));
    root->setSpacing(s(12));

    QWidget *leftCol = new QWidget(this);
    QVBoxLayout *left = new QVBoxLayout(leftCol);
    left->setContentsMargins(0, 0, 0, 0);
    left->setSpacing(s(6));

    header = new QWidget(leftCol);
    QHBoxLayout *h = new QHBoxLayout(header);
    h->setContentsMargins(0,0,0,0);
    h->setSpacing(s(10));

    QLabel *icon = new QLabel(header);
    icon->setPixmap(QPixmap(":/new/mainWindowIcons/noback/agvIcon.png")
                    .scaled(s(32), s(32), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    QLabel *title = new QLabel(agv.id, header);
    title->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:900;color:#1A1A1A;"
    ).arg(s(16)));

    arrowLabel = new QLabel(header);
    arrowLabel->setPixmap(QPixmap(":/new/mainWindowIcons/noback/arrow_down.png")
                          .scaled(s(18), s(18)));

    h->addWidget(icon);
    h->addWidget(title);
    h->addStretch();
    h->addWidget(arrowLabel, 0, Qt::AlignVCenter);

    left->addWidget(header);

    QWidget *sub = new QWidget(leftCol);
    QHBoxLayout *subL = new QHBoxLayout(sub);
    subL->setContentsMargins(0,0,0,0);
    subL->setSpacing(s(15));

    QString subStyle = QString(
        "font-family:Inter;font-size:%1px;font-weight:600;color:#333;"
    ).arg(s(14));

    QLabel *serial = new QLabel("SN: " + agv.serial, sub);
    serial->setStyleSheet(subStyle);

    QLabel *km = new QLabel(QString("Пробег: %1 км").arg(agv.kilometers), sub);
    km->setStyleSheet(subStyle);

    auto makeMaintenanceDot = [&](const QString &state){
        QLabel *dot = new QLabel(sub);
        dot->setFixedSize(s(12), s(12));
        dot->setStyleSheet(QString(
            "background:%1;border-radius:%2px;"
        ).arg(maintenanceColor(state)).arg(s(6)));
        return dot;
    };

    subL->addWidget(serial);
    subL->addWidget(km);
    subL->addSpacing(s(2));
    if (agv.hasOverdueMaintenance) {
        subL->addWidget(makeMaintenanceDot("red"));
        if (agv.hasSoonMaintenance)
            subL->addWidget(makeMaintenanceDot("orange"));
    } else if (agv.hasSoonMaintenance) {
        subL->addWidget(makeMaintenanceDot("orange"));
    } else {
        subL->addWidget(makeMaintenanceDot("green"));
    }
    subL->addStretch();

    left->addWidget(sub);

    details = new QWidget(leftCol);
    details->setVisible(false);

    QVBoxLayout *d = new QVBoxLayout(details);
    d->setContentsMargins(s(5), s(5), s(5), s(5));
    d->setSpacing(s(6));

    QString currentTaskText = agv.task.trimmed();
    if (currentTaskText.isEmpty() || currentTaskText == "—")
        currentTaskText = "—";
    QLabel *task = new QLabel("Текущая задача: " + currentTaskText, details);
    task->setStyleSheet(QString(
        "font-family:Inter;font-size:%1px;font-weight:600;color:#1A1A1A;"
    ).arg(s(14)));

    QLabel *bp = new QLabel(details);
    QPixmap bpPix(agv.blueprintPath);
    if (!bpPix.isNull())
        bp->setPixmap(bpPix.scaled(s(200), s(150), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    d->addWidget(task);
    d->addWidget(bp);

    left->addWidget(details);

    QWidget *rightCol = new QWidget(this);
    rightCol->setMinimumWidth(s(180));
    rightCol->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    QVBoxLayout *right = new QVBoxLayout(rightCol);
    right->setContentsMargins(s(8), s(4), s(2), s(4));
    right->setSpacing(s(10));

    QLabel *onlineStatus = new QLabel(rightCol);
    onlineStatus->setFixedSize(s(28), s(28));
    onlineStatus->setStyleSheet(QString(
        "background:%1;border-radius:%2px;"
    ).arg(statusColor(agv.status)).arg(s(14)));

    detailsButton = new QPushButton("Подробнее", rightCol);
    detailsButton->setStyleSheet(QString(
        "QPushButton{background:#0F00DB;color:white;font-family:Inter;"
        "font-size:%1px;font-weight:700;border-radius:%2px;padding:%3px %4px;}"
        "QPushButton:hover{background:#1A4ACD;}"
    ).arg(s(14)).arg(s(8)).arg(s(5)).arg(s(12)));
    detailsButton->setMinimumHeight(s(38));
    detailsButton->setCursor(Qt::PointingHandCursor);

    connect(detailsButton, &QPushButton::clicked, this, [this](){
        emit openDetailsRequested(agv.id);
    });

    QWidget *controlRow = new QWidget(rightCol);
    QHBoxLayout *controlLay = new QHBoxLayout(controlRow);
    controlLay->setContentsMargins(0, 0, 0, 0);
    controlLay->setSpacing(s(22));

    QWidget *statusWrap = new QWidget(controlRow);
    QHBoxLayout *statusLay = new QHBoxLayout(statusWrap);
    statusLay->setContentsMargins(0, 0, 0, 0);
    statusLay->setSpacing(0);
    statusLay->addWidget(onlineStatus);

    controlLay->addWidget(statusWrap, 0, Qt::AlignVCenter);
    controlLay->addWidget(detailsButton, 0, Qt::AlignVCenter);

    right->addStretch();
    right->addWidget(controlRow, 0, Qt::AlignRight | Qt::AlignVCenter);
    right->addStretch();

    root->addWidget(leftCol, 1);
    root->addWidget(rightCol, 0, Qt::AlignRight);
}

QString AgvItem::statusColor(const QString &st)
{
    const QString state = st.trimmed().toLower();
    if (state == "online" || state == "working")
        return "#00C8FF";
    if (state == "offline" || state == "disabled" || state == "off")
        return "#999999";
    return "#999999";
}

QString AgvItem::maintenanceColor(const QString &state)
{
    const QString st = state.trimmed().toLower();
    if (st == "red")
        return "#FF0000";
    if (st == "orange")
        return "#FF8800";
    return "#18CF00";
}

void AgvItem::mouseReleaseEvent(QMouseEvent *event)
{
    QFrame::mouseReleaseEvent(event);

    const bool vis = details->isVisible();
    details->setVisible(!vis);

    arrowLabel->setPixmap(QPixmap(
        vis ?
        ":/new/mainWindowIcons/noback/arrow_down.png" :
        ":/new/mainWindowIcons/noback/arrow_up.png"
    ).scaled(s(18), s(18)));
}
