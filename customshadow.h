#ifndef CUSTOM_SHADOW_H
#define CUSTOM_SHADOW_H

#include <QObject>
#include <QWidget>
#include <QPainter>
#include <QEvent> // Важно включить QEvent!

class CustomShadow : public QObject
{
    Q_OBJECT
public:
    explicit CustomShadow(QWidget* widget, bool hasRightShadow = true, bool hasBottomShadow = true, QObject *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *);

    // Объявляем обработчик фильтрации событий
    bool eventFilter(QObject*, QEvent*) override;

private:
    QWidget* m_targetWidget;
    bool m_hasRightShadow;
    bool m_hasBottomShadow;
};

#endif // CUSTOM_SHADOW_H
