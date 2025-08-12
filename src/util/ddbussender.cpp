// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ddbussender.h"

#include <QDBusInterface>
#include <QDebug>
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(logUtil, "dtk.core.util")

DDBusSender::DDBusSender()
    : m_dbusData(std::make_shared<DDBusData>())
{
    qCDebug(logUtil) << "DDBusSender created";
}

DDBusSender DDBusSender::service(const QString &service)
{
    qCDebug(logUtil) << "Setting service:" << service;
    m_dbusData->service = service;

    return *this;
}

DDBusSender DDBusSender::interface(const QString &interface)
{
    qCDebug(logUtil) << "Setting interface:" << interface;
    m_dbusData->interface = interface;

    return *this;
}

DDBusCaller DDBusSender::method(const QString &method)
{
    qCDebug(logUtil) << "Creating caller for method:" << method;
    return DDBusCaller(method, m_dbusData);
}

DDBusProperty DDBusSender::property(const QString &property)
{
    qCDebug(logUtil) << "Creating property for:" << property;
    return DDBusProperty(property, m_dbusData);
}

DDBusSender DDBusSender::system()
{
    qCDebug(logUtil) << "Creating system DDBusSender";
    DDBusSender self;
    self.type(QDBusConnection::SystemBus);
    return self;
}

DDBusSender DDBusSender::path(const QString &path)
{
    qCDebug(logUtil) << "Setting path:" << path;
    m_dbusData->path = path;

    return *this;
}

DDBusSender DDBusSender::type(const QDBusConnection::BusType busType)
{
    qCDebug(logUtil) << "Setting bus type:" << busType;
    switch (busType)
    {
    case QDBusConnection::SessionBus:
        qCDebug(logUtil) << "Using session bus";
        m_dbusData->connection = QDBusConnection::sessionBus();
        break;

    case QDBusConnection::SystemBus:
        qCDebug(logUtil) << "Using system bus";
        m_dbusData->connection = QDBusConnection::systemBus();
        break;

    default:
        qCWarning(logUtil) << "Unreachable bus type:" << busType;
        Q_UNREACHABLE_IMPL();
    }

    return *this;
}

DDBusData::DDBusData()
    : connection(QDBusConnection::sessionBus())
{
    qCDebug(logUtil) << "DDBusData created with session bus";
}

QDBusPendingCall DDBusData::asyncCallWithArguments(const QString &method, const QVariantList &arguments, const QString &iface)
{
    qCDebug(logUtil) << "Making async call: method=" << method << "arguments=" << arguments.size() << "interface=" << iface;
    // When creating a QDBusAbstractInterface, Qt will try to invoke introspection into this dbus path;
    // This is costing in some cases when introspection is not ready;
    // Cause this is an asynchronous method, it'd be better not to wait for anything, just leave this to caller;
    QDBusMessage message = QDBusMessage::createMethodCall(service, path, iface, method);
    message.setArguments(arguments);
    qCDebug(logUtil) << "DBus message created successfully";
    return connection.asyncCall(message);
}

QDBusMessage DDBusData::call(const QString &method, const QVariantList &arguments, const QString &iface)
{
    qCDebug(logUtil) << "Making sync call: method=" << method << "arguments=" << arguments.size() << "interface=" << iface;
    QDBusMessage message = QDBusMessage::createMethodCall(service, path, iface, method);
    message.setArguments(arguments);
    QDBusMessage reply = connection.call(message);
    qCDebug(logUtil) << "DBus call completed, reply type:" << reply.type();
    return reply;
}

QVariant DDBusData::get(const QString &property, const QString &iface)
{
    qCDebug(logUtil) << "Getting property:" << property << "interface:" << iface;
    QDBusMessage message = QDBusMessage::createMethodCall(service, path, iface, "Get");
    message << iface << property;
    QDBusMessage reply = connection.call(message);
    qCDebug(logUtil) << "Property get completed, reply type:" << reply.type();
    return reply.arguments().value(0);
}

DDBusCaller::DDBusCaller(const QString &method, std::shared_ptr<DDBusData> data)
    : m_method(method)
    , m_data(data)
{
    qCDebug(logUtil) << "DDBusCaller created for method:" << method;
}

QDBusPendingCall DDBusProperty::get()
{
    qCDebug(logUtil) << "Getting property:" << m_propertyName;
    QVariantList args{QVariant::fromValue(m_dbusData->interface), QVariant::fromValue(m_propertyName)};
    qCDebug(logUtil) << "Creating async call for property get";
    return m_dbusData->asyncCallWithArguments(QStringLiteral("Get"), args, QStringLiteral("org.freedesktop.DBus.Properties"));
}

DDBusProperty::DDBusProperty(const QString &property, std::shared_ptr<DDBusData> data)
    : m_property(property)
    , m_data(data)
{
    qCDebug(logUtil) << "DDBusProperty created for property:" << property;
}

DCORE_END_NAMESPACE
