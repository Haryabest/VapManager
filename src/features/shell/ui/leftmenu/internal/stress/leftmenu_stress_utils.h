#pragma once

#include <QApplication>
#include <QEventLoop>
#include <QRandomGenerator>
#include <QTimer>
#include <QVector>

constexpr qint64 kLeftMenuStressWallCapMs = 600000;

template <typename T>
inline void leftMenuShuffleVector(QVector<T> &values)
{
    for (int i = values.size() - 1; i > 0; --i) {
        const int j = QRandomGenerator::global()->bounded(i + 1);
        if (i != j)
            values.swapItemsAt(i, j);
    }
}

inline void leftMenuWaitUiMs(int ms)
{
    if (ms <= 0) {
        QApplication::processEvents();
        return;
    }
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

