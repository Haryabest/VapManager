#pragma once

#include <QString>

namespace AppVersion {

inline QString string()
{
    return QStringLiteral("1.0.0");
}

inline int build()
{
    return 105;
}

inline QString label()
{
    return string() + QStringLiteral(" (build ") + QString::number(build()) + QLatin1Char(')');
}

} // namespace AppVersion
