// SPDX-FileCopyrightText: 2017 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ddisksizeformatter.h"

#include <QString>
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(logUtil, "dtk.core.util")

DDiskSizeFormatter::DDiskSizeFormatter()
    : DAbstractUnitFormatter()
{
    qCDebug(logUtil) << "DDiskSizeFormatter constructor called";
}

QString DDiskSizeFormatter::unitStr(int unitId) const
{
    qCDebug(logUtil) << "Getting unit string for unit ID:" << unitId;
    
    switch (unitId) {
        case B:
            qCDebug(logUtil) << "Returning unit: B";
            return QStringLiteral("B");
        case K:
            qCDebug(logUtil) << "Returning unit: KB";
            return QStringLiteral("KB");
        case M:
            qCDebug(logUtil) << "Returning unit: MB";
            return QStringLiteral("MB");
        case G:
            qCDebug(logUtil) << "Returning unit: GB";
            return QStringLiteral("GB");
        case T:
            qCDebug(logUtil) << "Returning unit: TB";
            return QStringLiteral("TB");
    }

    qCWarning(logUtil) << "Unknown unit ID:" << unitId << "returning empty string";
    return QString();
}

DDiskSizeFormatter DDiskSizeFormatter::rate(int rate)
{
    qCDebug(logUtil) << "Setting disk size formatter rate:" << rate;
    m_rate = rate;

    return *this;
}

DCORE_END_NAMESPACE
