// SPDX-FileCopyrightText: 2015 Jolla Ltd.
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "ddbusextendedpendingcallwatcher_p.h"
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(logUtil, "dtk.core.util")

DDBusExtendedPendingCallWatcher::DDBusExtendedPendingCallWatcher(const QDBusPendingCall &call,
                                                                 const QString &asyncProperty,
                                                                 const QVariant &previousValue,
                                                                 QObject *parent)
    : QDBusPendingCallWatcher(call, parent)
    , m_asyncProperty(asyncProperty)
    , m_previousValue(previousValue)
{
    qCDebug(logUtil) << "DDBusExtendedPendingCallWatcher created with property:" << asyncProperty;
}

DDBusExtendedPendingCallWatcher::~DDBusExtendedPendingCallWatcher() {
    qCDebug(logUtil) << "DDBusExtendedPendingCallWatcher destroyed";
}

DCORE_END_NAMESPACE
