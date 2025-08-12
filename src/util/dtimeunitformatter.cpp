// SPDX-FileCopyrightText: 2017 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dtimeunitformatter.h"

#include <QString>
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(logUtil, "dtk.core.util")

DTimeUnitFormatter::DTimeUnitFormatter()
    : DAbstractUnitFormatter()
{
    qCDebug(logUtil) << "DTimeUnitFormatter constructor called";
}

uint DTimeUnitFormatter::unitConvertRate(int unitId) const
{
    qCDebug(logUtil) << "Getting unit convert rate for unit ID:" << unitId;
    
    switch (unitId) {
        case Seconds:
            qCDebug(logUtil) << "Returning convert rate for Seconds: 60";
            return 60;
        case Minute:
            qCDebug(logUtil) << "Returning convert rate for Minute: 60";
            return 60;
        case Hour:
            qCDebug(logUtil) << "Returning convert rate for Hour: 24";
            return 24;
        default:
            qCWarning(logUtil) << "Unknown unit ID:" << unitId << "returning 0";
            ;
    }

    return 0;
}

QString DTimeUnitFormatter::unitStr(int unitId) const
{
    qCDebug(logUtil) << "Getting unit string for unit ID:" << unitId;
    
    switch (unitId) {
        case Seconds:
            qCDebug(logUtil) << "Returning unit: s";
            return QStringLiteral("s");
        case Minute:
            qCDebug(logUtil) << "Returning unit: m";
            return QStringLiteral("m");
        case Hour:
            qCDebug(logUtil) << "Returning unit: h";
            return QStringLiteral("h");
        case Day:
            qCDebug(logUtil) << "Returning unit: d";
            return QStringLiteral("d");
        default:
            qCWarning(logUtil) << "Unknown unit ID:" << unitId << "returning empty string";
            ;
    }

    return QString();
}

DCORE_END_NAMESPACE
