#include <QApplication>
#include <QPluginLoader>
#include <QSqlDatabase>
#include <QSqlError>
#include <QDebug>
#include <QFileInfo>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    const QString dir = app.applicationDirPath();
    app.addLibraryPath(dir);
    app.addLibraryPath(dir + "/sqldrivers");
    app.addLibraryPath(dir + "/plugins");

    const QString plugin = dir + "/sqldrivers/qsqlodbc.dll";
    QPluginLoader loader(plugin);
    qDebug() << "plugin exists:" << QFileInfo::exists(plugin);
    qDebug() << "plugin load:" << loader.load() << loader.errorString();
    qDebug() << "drivers:" << QSqlDatabase::drivers();

    QSqlDatabase db = QSqlDatabase::addDatabase("QODBC");
    db.setDatabaseName(
        "DRIVER={PostgreSQL ODBC Driver(UNICODE)};SERVER=127.0.0.1;PORT=5432;"
        "DATABASE=agv_manager_db;UID=vapmanager;PWD=vapmanager2026;");
    qDebug() << "open:" << db.open() << db.lastError().text();
    return db.isOpen() ? 0 : 1;
}
