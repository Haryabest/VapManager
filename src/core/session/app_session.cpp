#include "app_session.h"

namespace {
QString g_username = "system";
}

namespace AppSession {
void setCurrentUsername(const QString &username)
{
    const QString trimmed = username.trimmed();
    g_username = trimmed.isEmpty() ? QString("system") : trimmed;
}

    QString currentUsername()
    {
        return g_username;
    }
}
