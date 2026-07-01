#include "leftmenu.h"

#include "app_session.h"
#include "db_users.h"
#include "databus.h"

#include <QBuffer>
#include <QClipboard>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QIcon>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTimer>
#include <QVBoxLayout>

namespace {

bool saveUserAvatarToDb(const QString &username, const QPixmap &pm)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    pm.save(&buffer, "PNG");

    QSqlQuery q(QSqlDatabase::database("main_connection"));
    q.prepare("UPDATE users SET avatar = :av WHERE username = :u");
    q.bindValue(":av", bytes);
    q.bindValue(":u", username);
    return q.exec();
}

} // namespace

void leftMenu::showProfile()
{
    activePage_ = ActivePage::Profile;
    hideAllPages();
    clearSearch();

    if (!profilePage)
        buildProfilePage();

    profilePage->setVisible(true);
    stressSuiteLogPageEntered(QStringLiteral("profile"));
}


void leftMenu::buildProfilePage()
{
    if (profilePage) {
        delete profilePage;
        profilePage = nullptr;
    }
    if (profileKeyTimer) {
        profileKeyTimer->stop();
        delete profileKeyTimer;
        profileKeyTimer = nullptr;
    }

    const QString currentUsername = AppSession::currentUsername();

    QString userRole = "viewer";
    {
        QSqlDatabase db = QSqlDatabase::database("main_connection");
        if (db.isOpen()) {
            QSqlQuery q(db);
            q.prepare("SELECT role FROM users WHERE username = :u");
            q.bindValue(":u", currentUsername);
            if (q.exec() && q.next()) {
                userRole = q.value(0).toString();
            }
        }
    }
    bool isAdmin = (userRole == "admin");
    bool isTech = (userRole == "tech");

    const QString userKey = currentUsername.trimmed().isEmpty()
                                ? QString("unknown")
                                : currentUsername.trimmed();

    QString savedFio, savedEmployeeId, savedMobile, savedEmail;
    QString savedPosition, savedDepartment, savedExtPhone, savedTelegram;

    UserInfo dbProfile;
    if (loadUserProfile(currentUsername, dbProfile)) {
        savedFio        = dbProfile.fullName;
        savedEmployeeId = dbProfile.employeeId;
        savedMobile     = dbProfile.mobile;
        savedEmail      = dbProfile.email;
        savedPosition   = dbProfile.position;
        savedDepartment = dbProfile.department;
        savedExtPhone   = dbProfile.extPhone;
        savedTelegram   = dbProfile.telegram;
    } else {
        QSettings settings("VapManager", "VapManager");
    settings.beginGroup(QString("profiles/%1").arg(userKey));
        savedFio        = settings.value("fio").toString();
        savedEmployeeId = settings.value("employee_id").toString();
        savedMobile     = settings.value("mobile").toString();
        savedEmail      = settings.value("email").toString();
        savedPosition   = settings.value("position").toString();
        savedDepartment = settings.value("department").toString();
        savedExtPhone   = settings.value("ext_phone").toString();
        savedTelegram   = settings.value("telegram").toString();
    settings.endGroup();
    }

    QWidget *profileParent = rightCalendarFrame ? rightCalendarFrame->parentWidget() : this;

    profilePage = new QWidget(profileParent);
    profilePage->setStyleSheet("background:#F5F7FB;");

    QVBoxLayout *mainLayout = new QVBoxLayout(profilePage);
    mainLayout->setContentsMargins(s(20), s(15), s(20), s(15));
    mainLayout->setSpacing(s(12));

    //
    // ====== ШАПКА ======
    //
    //
    // ====== ШАПКА ПРОФИЛЯ (идеально выровненная) ======
    //
    QWidget *header = new QWidget(profilePage);
    QHBoxLayout *headerLayout = new QHBoxLayout(header);

    // Отступы как у твоей кнопки
    headerLayout->setContentsMargins(s(10), s(10), s(10), s(5));
    headerLayout->setSpacing(s(10));

    QPushButton *backBtn = new QPushButton("   Назад", header);
    backBtn->setIcon(QIcon(":/new/mainWindowIcons/noback/arrow_left.png"));
    backBtn->setIconSize(QSize(s(24), s(24)));
    backBtn->setFixedSize(s(150), s(50));

    backBtn->setStyleSheet(QString(
        "QPushButton {"
        "   background-color:#E6E6E6;"
        "   border-radius:%1px;"
        "   border:1px solid #C8C8C8;"
        "   font-family:Inter;"
        "   font-size:%2px;"
        "   font-weight:800;"
        "   color:black;"
        "   text-align:left;"
        "   padding-left:%3px;"
        "}"
        "QPushButton:hover { background-color:#D5D5D5; }"
    ).arg(s(10)).arg(s(16)).arg(s(10)));

    connect(backBtn, &QPushButton::clicked, this, [this]() {
        showCalendar();
    });

    headerLayout->addWidget(backBtn, 0, Qt::AlignLeft);
    headerLayout->addStretch();

    // Добавляем header в профиль
    mainLayout->addWidget(header);
    mainLayout->addSpacing(s(5));

    //
    // ====== СКРОЛЛ ======
    //
    QScrollArea *scroll = new QScrollArea(profilePage);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea{background:transparent;}");

    QWidget *content = new QWidget();
    content->setStyleSheet("background:transparent;");
    QVBoxLayout *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(s(10), 0, s(10), 0);
    contentLayout->setSpacing(s(20));

    //
    // ====== КАРТОЧКА ПРОФИЛЯ ======
    //
    QWidget *profileCard = new QWidget(content);
    profileCard->setStyleSheet("background:transparent;");
    QVBoxLayout *cardLayout = new QVBoxLayout(profileCard);
    cardLayout->setContentsMargins(s(4), 0, 0, 0);   // ← единый левый отступ
    cardLayout->setSpacing(s(16));

    QHBoxLayout *avatarRow = new QHBoxLayout();
    avatarRow->setSpacing(s(16));

    QLabel *avatarLabel = new QLabel(profileCard);
    QPixmap avatarPm = loadUserAvatarFromDb(currentUsername);
    if (avatarPm.isNull()) avatarPm = QPixmap(":/new/mainWindowIcons/noback/user-icon.png");
    QPixmap toRound = avatarPm.isNull() ? QPixmap() : avatarPm.scaled(s(80), s(80), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    QPixmap roundAvatar = makeRoundPixmap(toRound, s(80));
    avatarLabel->setPixmap(roundAvatar);
    avatarLabel->setFixedSize(s(80), s(80));
    avatarRow->addWidget(avatarLabel);

    QVBoxLayout *nameCol = new QVBoxLayout();
    nameCol->setSpacing(s(4));

    QLabel *nameLabel = new QLabel(currentUsername, profileCard);
    nameLabel->setStyleSheet(
        "font-family:Inter;font-size:24px;font-weight:900;color:#0F172A;background:transparent;"
    );
    nameCol->addWidget(nameLabel);

    QString roleText =
        isAdmin ? "Роль: Администратор"
        : (userRole == "tech" ? "Роль: Техник" : "Роль: Пользователь");

    QLabel *roleLabel = new QLabel(roleText, profileCard);
    roleLabel->setStyleSheet(
        "font-family:Inter;font-size:14px;font-weight:700;color:#475569;background:transparent;"
    );
    nameCol->addWidget(roleLabel);

    avatarRow->addLayout(nameCol);
    avatarRow->addStretch();
    cardLayout->addLayout(avatarRow);

    //
    // ====== АДМИНСКИЙ КЛЮЧ ======
    //
    //
    // ====== АДМИНСКИЙ КЛЮЧ (идеально выровненный) ======
    //
    //
    // ====== АДМИНСКИЙ КЛЮЧ — ИДЕАЛЬНОЕ ВЫРАВНИВАНИЕ ======
    //
    if (isAdmin || isTech) {

        QWidget *adminBlock = new QWidget(profileCard);
        QVBoxLayout *adminLayout = new QVBoxLayout(adminBlock);
        adminLayout->setContentsMargins(0, 0, 0, 0);
        adminLayout->setSpacing(s(10));

        //
        // Создаём сетку: 2 колонки
        //
        QGridLayout *grid = new QGridLayout();
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(s(6));
        grid->setVerticalSpacing(s(4));

        // Фиксируем ширину первой колонки (как у меток "ФИО", "Телефон" и т.д.)
        int labelWidth = s(20);   // ширина под иконку
        grid->setColumnMinimumWidth(0, labelWidth);
        grid->setColumnStretch(1, 1);

        //
        // Иконка
        //
        QLabel *keyIcon = new QLabel(adminBlock);
        keyIcon->setPixmap(
            QPixmap(":/new/mainWindowIcons/noback/key.png")
                .scaled(s(16), s(16), Qt::KeepAspectRatio, Qt::SmoothTransformation)
        );
        keyIcon->setFixedSize(s(16), s(16));
        keyIcon->setStyleSheet("background:transparent;");

        //
        // Заголовок
        //
        QString keyBlockTitle = isTech ? "Ключ техника" : "Ключ администратора";
        QLabel *keyTitle = new QLabel(keyBlockTitle, adminBlock);
        keyTitle->setStyleSheet(
            "font-family:Inter;"
            "font-size:14px;"
            "font-weight:800;"
            "color:#0369A1;"
            "background:transparent;"
        );

        grid->addWidget(keyIcon, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
        grid->addWidget(keyTitle, 0, 1, Qt::AlignLeft | Qt::AlignVCenter);

        adminLayout->addLayout(grid);

        //
        // Сам ключ
        //
        QString inviteKey;
        if (isAdmin) {
        refreshAdminInviteKeyIfNeeded(currentUsername);
            inviteKey = getAdminInviteKey(currentUsername);
        } else {
            refreshTechInviteKeyIfNeeded(currentUsername);
            inviteKey = getTechInviteKey(currentUsername);
        }

        QLabel *keyValue = new QLabel(inviteKey.isEmpty() ? "Генерация..." : inviteKey, adminBlock);
        keyValue->setAlignment(Qt::AlignCenter);
        keyValue->setStyleSheet(
            "background:#E0F2FE;"
            "border:1px solid #BAE6FD;"
            "border-radius:10px;"
            "font-family:Consolas,monospace;"
            "font-size:20px;"
            "font-weight:700;"
            "color:#0C4A6E;"
            "padding:12px;"
        );
        adminLayout->addWidget(keyValue);

        //
        // Подсказка
        //
        QString keyHintText = isTech
            ? "Действует 10 минут. Передайте новому технику для регистрации."
            : "Действует 10 минут. Передайте новому админу для регистрации.";
        QLabel *keyHint = new QLabel(keyHintText, adminBlock);
        keyHint->setWordWrap(true);
        keyHint->setStyleSheet(
            "font-family:Inter;font-size:11px;color:#0369A1;background:transparent;"
        );
        adminLayout->addWidget(keyHint);

        //
        // Кнопка копирования
        //
        QPushButton *copyKeyBtn = new QPushButton("Копировать ключ", adminBlock);
        copyKeyBtn->setStyleSheet(
            "QPushButton{background:#0EA5E9;color:white;font-family:Inter;font-size:13px;"
            "font-weight:700;border:none;border-radius:8px;padding:10px 16px;}"
            "QPushButton:hover{background:#0284C7;}"
        );
        connect(copyKeyBtn, &QPushButton::clicked, this, [keyValue, copyKeyBtn]() {
            QApplication::clipboard()->setText(keyValue->text());
            copyKeyBtn->setText("Скопировано!");
            QTimer::singleShot(2000, copyKeyBtn, [copyKeyBtn]() {
                copyKeyBtn->setText("Копировать ключ");
            });
        });
        adminLayout->addWidget(copyKeyBtn);

        //
        // Таймер
        //
        profileKeyTimer = new QTimer(this);
        profileKeyTimer->setInterval(30000);
        connect(profileKeyTimer, &QTimer::timeout, this, [this, keyValue, currentUsername, isTech]() {
            if (isTech) {
                refreshTechInviteKeyIfNeeded(currentUsername);
                QString newKey = getTechInviteKey(currentUsername);
                keyValue->setText(newKey.isEmpty() ? "Ошибка" : newKey);
            } else {
            refreshAdminInviteKeyIfNeeded(currentUsername);
            QString newKey = getAdminInviteKey(currentUsername);
            keyValue->setText(newKey.isEmpty() ? "Ошибка" : newKey);
            }
        });
        profileKeyTimer->start();

        cardLayout->addWidget(adminBlock);
    }


    contentLayout->addWidget(profileCard);

    //
    // ====== ЛИЧНЫЕ ДАННЫЕ ======
    //
    QWidget *editCard = new QWidget(content);
    editCard->setStyleSheet("background:transparent;");
    QVBoxLayout *editLayout = new QVBoxLayout(editCard);
    editLayout->setContentsMargins(s(4), 0, 0, 0);   // ← тот же левый отступ
    editLayout->setSpacing(s(10));

    QLabel *editTitle = new QLabel("Личные данные", editCard);
    editTitle->setStyleSheet(
        "font-family:Inter;font-size:18px;font-weight:900;color:#0F172A;background:transparent;"
    );
    editLayout->addWidget(editTitle);

    auto addField = [&](const QString &label,
                        const QString &placeholder,
                        const QString &value,
                        const QString &validatorPattern,
                        int maxLen = 0) -> QLineEdit*
    {
        QLabel *lbl = new QLabel(label, editCard);
        lbl->setStyleSheet(
            "font-family:Inter;font-size:13px;font-weight:700;color:#334155;background:transparent;"
        );
        editLayout->addWidget(lbl);

        QLineEdit *edit = new QLineEdit(editCard);
        edit->setPlaceholderText(placeholder);
        edit->setText(value);
        edit->setStyleSheet(
            "QLineEdit{background:#FFFFFF;border:1px solid #E2E8F0;border-radius:10px;"
            "padding:12px 14px;font-family:Inter;font-size:14px;color:#0F172A;}"
            "QLineEdit:focus{border:1px solid #3B82F6;background:white;}"
        );
        if (maxLen > 0)
            edit->setMaxLength(maxLen);
        if (!validatorPattern.isEmpty()) {
            edit->setValidator(
                new QRegularExpressionValidator(QRegularExpression(validatorPattern), edit)
            );
        }
        editLayout->addWidget(edit);
        return edit;
    };

    QLineEdit *fioEdit        = addField("ФИО", "Введите ФИО", savedFio, "", 128);
    QLineEdit *employeeIdEdit = addField("Табельный номер", "До 6 цифр", savedEmployeeId, "^[0-9]{0,6}$");
    QLineEdit *positionEdit   = addField("Должность", "Введите должность", savedPosition, "", 128);
    QLineEdit *departmentEdit = addField("Подразделение", "Введите подразделение", savedDepartment, "", 128);
    QLineEdit *mobileEdit     = addField("Телефон", "+7 (XXX) XXX-XX-XX", savedMobile, "^[\\+0-9\\-\\s\\(\\)]{0,20}$");
    QLineEdit *extPhoneEdit   = addField("Внутренний номер", "До 5 цифр", savedExtPhone, "^[0-9]{0,5}$");
    QLineEdit *emailEdit      = addField("Email", "example@mail.com", savedEmail, "");
    QLineEdit *telegramEdit   = addField("Telegram", "@username", savedTelegram, "^@?[A-Za-z0-9_]{0,32}$");

    connect(mobileEdit, &QLineEdit::editingFinished, mobileEdit, [mobileEdit](){
        QString t = mobileEdit->text().trimmed();
        if (t.isEmpty()) return;
        QString digits = t;
        digits.remove(QRegularExpression("[^0-9+]"));
        if (digits.isEmpty()) return;
        QString result;
        if (digits.startsWith("+7")) result = digits;
        else if (digits.startsWith("8") && digits.length() > 1) result = "+7" + digits.mid(1);
        else if (digits.startsWith("7")) result = "+" + digits;
        else result = "+7" + digits;
        if (result != mobileEdit->text()) {
            mobileEdit->blockSignals(true);
            mobileEdit->setText(result);
            mobileEdit->blockSignals(false);
        }
    });

    connect(telegramEdit, &QLineEdit::textChanged, telegramEdit, [telegramEdit](){
        QString t = telegramEdit->text();
        QString out;
        for (int i = 0; i < t.length(); ++i) {
            QChar c = t[i];
            if (c == '@' && out.isEmpty()) out += c;
            else if (c.isLetterOrNumber() || c == '_') out += c;
        }
        if (!out.isEmpty() && !out.startsWith('@')) out.prepend('@');
        if (out != t) {
            telegramEdit->blockSignals(true);
            telegramEdit->setText(out);
            telegramEdit->blockSignals(false);
        }
    });

    QLabel *errorLbl = new QLabel(editCard);
    errorLbl->setStyleSheet(
        "font-family:Inter;font-size:12px;font-weight:600;color:#DC2626;background:transparent;"
    );
    errorLbl->setWordWrap(true);
    editLayout->addWidget(errorLbl);

    editLayout->addSpacing(s(10));

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(s(12));

    QPushButton *saveBtn = new QPushButton("Сохранить изменения", editCard);
    saveBtn->setStyleSheet(
        "QPushButton{background:#0F00DB;color:white;font-family:Inter;font-size:14px;"
        "font-weight:800;border:none;border-radius:10px;padding:14px 28px;}"
        "QPushButton:hover{background:#1A4ACD;}"
    );

    connect(saveBtn, &QPushButton::clicked, this, [=]() {
        QString mobile = mobileEdit->text().trimmed();
        if (!mobile.isEmpty()) {
            QString digits = mobile;
            digits.remove(QRegularExpression("[^0-9+]"));
            if (!digits.startsWith("+") && !digits.startsWith("7") && !digits.startsWith("8")) {
                mobile = "+7" + digits;
            } else if (digits.startsWith("8") && digits.length() > 1) {
                mobile = "+7" + digits.mid(1);
            } else if (digits.startsWith("7") && !digits.startsWith("+")) {
                mobile = "+" + digits;
            }
            mobileEdit->setText(mobile);
        }

        QString email = emailEdit->text().trimmed();
        if (!email.isEmpty()) {
            if (!email.contains('@') || !QRegularExpression("\\.[a-zA-Z]{2,}$").match(email).hasMatch()) {
                errorLbl->setText("Email должен содержать @ и домен (.com, .ru и т.п.).");
                return;
            }
        }

        QString telegram = telegramEdit->text().trimmed();
        if (!telegram.isEmpty()) {
            QString out;
            for (int i = 0; i < telegram.length(); ++i) {
                QChar c = telegram[i];
                if (c == '@' && out.isEmpty()) out += c;
                else if (c.isLetterOrNumber() || c == '_') out += c;
            }
            if (!out.isEmpty() && !out.startsWith('@')) out.prepend('@');
            telegramEdit->setText(out);
            telegram = out;
        }

        QSettings s("VapManager", "VapManager");
        s.beginGroup(QString("profiles/%1").arg(userKey));
        s.setValue("fio",          fioEdit->text().trimmed());
        s.setValue("employee_id",  employeeIdEdit->text().trimmed());
        s.setValue("mobile",       mobile);
        s.setValue("email",        email);
        s.setValue("position",     positionEdit->text().trimmed());
        s.setValue("department",   departmentEdit->text().trimmed());
        s.setValue("ext_phone",    extPhoneEdit->text().trimmed());
        s.setValue("telegram",     telegram);
        s.endGroup();

        UserInfo profileData;
        profileData.username   = currentUsername;
        profileData.fullName   = fioEdit->text().trimmed();
        profileData.employeeId = employeeIdEdit->text().trimmed();
        profileData.position   = positionEdit->text().trimmed();
        profileData.department = departmentEdit->text().trimmed();
        profileData.mobile     = mobile;
        profileData.extPhone   = extPhoneEdit->text().trimmed();
        profileData.email      = email;
        profileData.telegram   = telegram;

        QString dbError;
        if (!saveUserProfile(profileData, dbError)) {
            errorLbl->setStyleSheet(
                "font-family:Inter;font-size:12px;font-weight:600;color:#DC2626;background:transparent;"
            );
            errorLbl->setText("Ошибка сохранения: " + (dbError.isEmpty() ? "не удалось записать в БД" : dbError));
            return;
        }

        emit DataBus::instance().userDataChanged();

        logAction(currentUsername, "profile_saved", "Профиль сохранён");
        errorLbl->setStyleSheet(
            "font-family:Inter;font-size:12px;font-weight:600;color:#10B981;background:transparent;"
        );
        errorLbl->setText("Изменения сохранены!");
        QTimer::singleShot(2000, errorLbl, [errorLbl]() {
            errorLbl->setStyleSheet(
                "font-family:Inter;font-size:12px;font-weight:600;color:#DC2626;background:transparent;"
            );
            errorLbl->clear();
        });
    });

    btnRow->addWidget(saveBtn);
    btnRow->addStretch();
    editLayout->addLayout(btnRow);

    contentLayout->addWidget(editCard);
    contentLayout->addStretch();

    scroll->setWidget(content);
    mainLayout->addWidget(scroll, 1);

    // Добавляем страницу в правую панель
    if (rightCalendarFrame) {
        QWidget *rightBodyFrame = rightCalendarFrame->parentWidget();
        if (rightBodyFrame) {
            if (QVBoxLayout *rightBodyLayout = qobject_cast<QVBoxLayout*>(rightBodyFrame->layout())) {
                rightBodyLayout->addWidget(profilePage, 3);
            }
        }
    }
}

void leftMenu::changeAvatar()
{
        QString username = AppSession::currentUsername();
        if (username.isEmpty())
            return;

        QString file = QFileDialog::getOpenFileName(
            this,
            "Выберите изображение",
            "",
            "Изображения (*.png *.jpg *.jpeg *.bmp)"
        );

        if (file.isEmpty())
            return;

        QPixmap pm(file);
        if (pm.isNull()) {
            QMessageBox::warning(this, "Ошибка", "Не удалось загрузить изображение.");
            return;
        }

        // Сохраняем в БД
        if (!saveUserAvatarToDb(username, pm)) {
            QMessageBox::warning(this, "Ошибка", "Не удалось сохранить аватар в базу данных.");
            return;
        }

        // Обновляем кнопку
        QPixmap round = makeRoundPixmap(pm, s(55));
        userButton->setIcon(QIcon(round));
        userButton->setIconSize(QSize(s(55), s(55)));

        // Если страница профиля уже создана — пересобираем её, чтобы аватар обновился сразу
        if (profilePage) {
            bool wasVisible = profilePage->isVisible();
            buildProfilePage();
            profilePage->setVisible(wasVisible);
        }

        QMessageBox::information(this, "Готово", "Аватар успешно обновлён.");
}
