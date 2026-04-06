#ifndef DB_MODELS_H
#define DB_MODELS_H

#include <QString>
#include <QStringList>
#include <QVector>

struct ModelInfo
{
    QString name;
    QString versionPo;      // версия по: англ буквы + цифры, 10 символов
    QString versionEplan;   // версия eplan: те же фильтры
    QString category;       // в Подробнее
    int capacityKg = 0;
    int maxSpeed = 0;       // км/ч, в Подробнее
    QString dimensions;     // габариты: 4x4x4 м
    int couplingCount = 0;  // сцепные устройства 1-4
    QString direction;      // направление: 1/2/4
};

// Read
QStringList loadModelNames();
QVector<ModelInfo> loadModelList();

// Write
bool insertModelToDb(const ModelInfo &m);
bool deleteModelByName(const QString &name);

#endif
