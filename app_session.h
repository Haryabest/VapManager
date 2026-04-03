#ifndef APP_SESSION_H
#define APP_SESSION_H

#include <QString>

namespace AppSession {
void setCurrentUsername(const QString &username);
QString currentUsername();
}

#endif // APP_SESSION_H
