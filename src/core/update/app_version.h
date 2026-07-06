#pragma once

#include <QString>

namespace AppVersion {

inline QString string()
{
    return QStringLiteral("1.0.2");
}

inline int build()
{
    return 162;
}

inline QString label()
{
    return string() + QStringLiteral(" (build ") + QString::number(build()) + QLatin1Char(')');
}

} // namespace AppVersion
