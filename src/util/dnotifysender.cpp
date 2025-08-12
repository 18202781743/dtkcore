// SPDX-FileCopyrightText: 2017 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dnotifysender.h"
#include "ddbussender.h"
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(logUtil, "dtk.core.util")

namespace DUtil {

struct DNotifyData {
    uint        m_replaceId;
    int         m_timeOut;
    QString     m_body;
    QString     m_summary;
    QString     m_appIcon;
    QString     m_appName;
    QStringList m_actions;
    QVariantMap m_hints;
};

DNotifySender::DNotifySender(const QString &summary) : m_dbusData(std::make_shared<DNotifyData>())
{
    qCDebug(logUtil) << "DNotifySender created with summary:" << summary;
    m_dbusData->m_summary = summary;
}

DNotifySender DNotifySender::appName(const QString &appName)
{
    qCDebug(logUtil) << "Setting app name:" << appName;
    m_dbusData->m_appName = appName;

    return *this;
}

DNotifySender DNotifySender::appIcon(const QString &appIcon)
{
    qCDebug(logUtil) << "Setting app icon:" << appIcon;
    m_dbusData->m_appIcon = appIcon;

    return *this;
}

DNotifySender DNotifySender::appBody(const QString &appBody)
{
    qCDebug(logUtil) << "Setting app body:" << appBody;
    m_dbusData->m_body = appBody;

    return *this;
}

DNotifySender DNotifySender::replaceId(const uint replaceId)
{
    qCDebug(logUtil) << "Setting replace ID:" << replaceId;
    m_dbusData->m_replaceId = replaceId;

    return *this;
}

DNotifySender DNotifySender::timeOut(const int timeOut)
{
    qCDebug(logUtil) << "Setting timeout:" << timeOut;
    m_dbusData->m_timeOut = timeOut;

    return *this;
}

DNotifySender DNotifySender::actions(const QStringList &actions)
{
    qCDebug(logUtil) << "Setting actions:" << actions.size() << "items";
    m_dbusData->m_actions = actions;

    return *this;
}

DNotifySender DNotifySender::hints(const QVariantMap &hints)
{
    qCDebug(logUtil) << "Setting hints:" << hints.size() << "items";
    m_dbusData->m_hints = hints;

    return *this;
}

QDBusPendingCall DNotifySender::call()
{
    qCDebug(logUtil) << "Sending notification: app=" << m_dbusData->m_appName << "summary=" << m_dbusData->m_summary << "body=" << m_dbusData->m_body;
    qCDebug(logUtil) << "Notification details: replaceId=" << m_dbusData->m_replaceId << "timeout=" << m_dbusData->m_timeOut << "actions=" << m_dbusData->m_actions.size() << "hints=" << m_dbusData->m_hints.size();
    
    QDBusPendingCall result = DDBusSender()
        .service("org.freedesktop.Notifications")
        .path("/org/freedesktop/Notifications")
        .interface("org.freedesktop.Notifications")
        .method(QString("Notify"))
        .arg(m_dbusData->m_appName)
        .arg(m_dbusData->m_replaceId)
        .arg(m_dbusData->m_appIcon)
        .arg(m_dbusData->m_summary)
        .arg(m_dbusData->m_body)
        .arg(m_dbusData->m_actions)
        .arg(m_dbusData->m_hints)
        .arg(m_dbusData->m_timeOut)
        .call();
    
    qCDebug(logUtil) << "Notification sent successfully";
    return result;
}

}  // namespace DUtil

DCORE_END_NAMESPACE
