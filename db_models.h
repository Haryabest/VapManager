#ifndef DB_MODELS_H
#define DB_MODELS_H

#include <QString>
#include <QStringList>
#include <QVector>

struct ModelInfo {
    QString name;
    QString category;
    int capacityKg;
    int maxSpeed; // км/ч
};

QStringList loadModelNames();
QVector<ModelInfo> loadModelList();

bool insertModelToDb(const ModelInfo &m);
bool deleteModelByName(const QString &name);

#endif
