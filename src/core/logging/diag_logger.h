#pragma once

#include <QString>
#include <QVector>

class QTextEdit;

// Путь к каталогу скрытых расширенных логов пользователей (viewer) — не в «Документах», для выгрузки техником.
QString viewerSecureLogDirPath();

// Полный путь к защищённому файлу расширенного аудита для логина (viewer).
QString viewerSecureLogFilePath(const QString &username);

// Расширенная строка только для роли viewer — пишется в скрытый каталог (техник забирает через «Скачать логи»).
void viewerSecureExtendedLog(const QString &username,
                             const QString &action,
                             const QString &details);

// Подробный тех-лог (только role == "tech"): файл + опционально виджет в UI.
void techDiagLog(const QString &tag, const QString &message);

// Привязать QTextEdit на экране «Logs» (только техник); nullptr — отвязать.
void setTechDiagLogSink(QTextEdit *w);

// Последние строки тех-лога (для отладки / экспорта).
QVector<QString> techDiagRecentLines(int maxLines = 2000);

void clearTechDiagRecentLines();

// --- Полный автотест UI (отчёт для техника) ---
QString stressAutotestReportPath();
void stressAutotestBeginSession(const QString &headline);
void stressAutotestLogLine(const QString &message);
