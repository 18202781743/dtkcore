// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "ddbusinterface.h"
#include "ddbusinterface_p.h"

#include <QMetaObject>
#include <qmetaobject.h>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusPendingReply>
#include <QDebug>
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(logUtil, "dtk.core.util")

static const QString &FreedesktopService = QStringLiteral("org.freedesktop.DBus");
static const QString &FreedesktopPath = QStringLiteral("/org/freedesktop/DBus");
static const QString &FreedesktopInterface = QStringLiteral("org.freedesktop.DBus");
static const QString &NameOwnerChanged = QStringLiteral("NameOwnerChanged");

static const QString &PropertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");
static const QString &PropertiesChanged = QStringLiteral("PropertiesChanged");
static const char *PropertyName = "propname";

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    #define PropType(metaProperty) metaProperty.metaType()
#else
    #define PropType(metaProperty) metaProperty.userType()
#endif

static QVariant demarshall(const QMetaProperty &metaProperty, const QVariant &value)
{
    qCDebug(logUtil) << "Demarshalling property:" << metaProperty.name();
    // if the value is the same with parent one, return value
    if (value.userType() == metaProperty.userType()) {
        qCDebug(logUtil) << "Value type matches property type, returning as-is";
        return value;
    }

    // unwrap the value with parent one
    QVariant result = QVariant(PropType(metaProperty), nullptr);

    if (value.userType() == qMetaTypeId<QDBusArgument>()) {
        qCDebug(logUtil) << "Converting QDBusArgument to property type";
        QDBusArgument dbusArg = value.value<QDBusArgument>();
        QDBusMetaType::demarshall(dbusArg, PropType(metaProperty), result.data());
    } else {
        qCDebug(logUtil) << "Value type:" << value.typeName() << "property type:" << metaProperty.typeName();
    }

    return result;
}

DDBusInterfacePrivate::DDBusInterfacePrivate(DDBusInterface *interface, QObject *parent)
    : QObject(interface)
    , m_parent(parent)
    , m_serviceValid(false)
    , q_ptr(interface)
{
    qCDebug(logUtil) << "DDBusInterfacePrivate created for service:" << interface->service();
    QDBusMessage message = QDBusMessage::createMethodCall(FreedesktopService, FreedesktopPath, FreedesktopInterface, "NameHasOwner");
    message << interface->service();
    interface->connection().callWithCallback(message, this, SLOT(onDBusNameHasOwner(bool)));

    interface->connection().connect(interface->service(),
                                    interface->path(),
                                    PropertiesInterface,
                                    PropertiesChanged,
                                    {interface->interface()},
                                    QString(),
                                    this,
                                    SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
}

DDBusInterfacePrivate::~DDBusInterfacePrivate()
{
    qCDebug(logUtil) << "DDBusInterfacePrivate destructor called";
}

void DDBusInterfacePrivate::updateProp(const char *propName, const QVariant &value)
{
    qCDebug(logUtil) << "Updating property:" << propName;
    if (!m_parent) {
        qCWarning(logUtil, "No parent object, cannot update property");
        return;
    }

    const QMetaObject *metaObj = m_parent->metaObject();
    const char *typeName(value.typeName());
    void *data = const_cast<void *>(value.data());
    int propertyIndex = metaObj->indexOfProperty(propName);
    QVariant result = value;

    // TODO: it now cannot convert right, Like QMap
    // if there is property, then try to convert with property
    if (propertyIndex != -1) {
        qCDebug(logUtil) << "Property found at index:" << propertyIndex;
        QMetaProperty metaProperty = metaObj->property(propertyIndex);
        result = demarshall(metaProperty, value);
        data = const_cast<void *>(result.data());
        typeName = result.typeName();
    } else {
        qCDebug(logUtil) << "Property not found:" << propName;
    }

    if (data) {
        qCDebug(logUtil) << "Setting property value with type:" << typeName;
        metaObj->property(propertyIndex).write(m_parent, result);
    } else {
        qCWarning(logUtil, "No data available for property: %s", propName);
    }
}

void DDBusInterfacePrivate::initDBusConnection()
{
    qCDebug(logUtil) << "Initializing DBus connection";
    if (!m_parent) {
        qCWarning(logUtil, "No parent object, cannot initialize DBus connection");
        return;
    }

    Q_Q(DDBusInterface);
    QDBusConnection connection = q->connection();
    QStringList signalList;
    QDBusInterface inter(q->service(), q->path(), q->interface(), connection);
    const QMetaObject *meta = inter.metaObject();
    qCDebug(logUtil) << "Processing signals for service:" << q->service() << "path:" << q->path() << "interface:" << q->interface();
    
    for (int i = meta->methodOffset(); i < meta->methodCount(); ++i) {
        const QMetaMethod &method = meta->method(i);
        if (method.methodType() == QMetaMethod::Signal) {
            signalList << method.methodSignature();
            qCDebug(logUtil) << "Found signal:" << method.methodSignature().constData();
        }
    }
    
    qCDebug(logUtil) << "Total signals found:" << signalList.size();
    const QMetaObject *parentMeta = m_parent->metaObject();
    for (const QString &signal : signalList) {
        int i = parentMeta->indexOfSignal(QMetaObject::normalizedSignature(signal.toLatin1()));
        if (i != -1) {
            const QMetaMethod &parentMethod = parentMeta->method(i);
            qCDebug(logUtil) << "Connecting signal:" << signal;
            connection.connect(q->service(),
                               q->path(),
                               q->interface(),
                               parentMethod.name(),
                               m_parent,
                               QT_STRINGIFY(QSIGNAL_CODE) + parentMethod.methodSignature());
        } else {
            qCDebug(logUtil) << "Signal not found in parent:" << signal;
        }
    }
}

void DDBusInterfacePrivate::onPropertiesChanged(const QString &interface, const QVariantMap &changedProperties, const QStringList &invalidatedProperties)
{
    qCDebug(logUtil) << "Properties changed for interface:" << interface;
    qCDebug(logUtil) << "Changed properties count:" << changedProperties.size();
    qCDebug(logUtil) << "Invalidated properties count:" << invalidatedProperties.size();
    
    for (auto it = changedProperties.begin(); it != changedProperties.end(); ++it) {
        qCDebug(logUtil) << "Processing changed property:" << it.key();
        updateProp(it.key().toLocal8Bit().constData(), it.value());
    }
    
    for (const QString &propName : invalidatedProperties) {
        qCDebug(logUtil) << "Processing invalidated property:" << propName;
        // Handle invalidated properties if needed
    }
}

void DDBusInterfacePrivate::onAsyncPropertyFinished(QDBusPendingCallWatcher *w)
{
    qCDebug(logUtil) << "Async property finished";
    QDBusPendingReply<QVariant> reply = *w;
    if (!reply.isError()) {
        QString propName = w->property(PropertyName).toString();
        qCDebug(logUtil) << "Property reply successful for:" << propName;
        updateProp(propName.toLatin1(), reply.value());
    } else {
        qCWarning(logUtil, "Property reply error: %s", qPrintable(reply.error().message()));
    }
    w->deleteLater();
}

void DDBusInterfacePrivate::setServiceValid(bool valid)
{
    qCDebug(logUtil) << "Setting service valid:" << (valid ? "true" : "false");
    if (m_serviceValid != valid) {
        m_serviceValid = valid;
        Q_EMIT q_ptr->serviceValidChanged(m_serviceValid);
        qCDebug(logUtil) << "Service valid changed to:" << (valid ? "true" : "false");
    }
}

void DDBusInterfacePrivate::onDBusNameHasOwner(bool valid)
{
    qCDebug(logUtil) << "DBus name has owner callback:" << (valid ? "true" : "false");
    setServiceValid(valid);
}

void DDBusInterfacePrivate::onDBusNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner)
{
    Q_Q(DDBusInterface);
    qCDebug(logUtil) << "DBus name owner changed - name:" << name << "oldOwner:" << oldOwner << "newOwner:" << newOwner;
    
    if (name == q->service() && oldOwner.isEmpty()) {
        qCDebug(logUtil) << "Service appeared, initializing connection";
        initDBusConnection();
        q->connection().disconnect(FreedesktopService,
                                   FreedesktopPath,
                                   FreedesktopInterface,
                                   NameOwnerChanged,
                                   this,
                                   SLOT(onDBusNameOwnerChanged(QString, QString, QString)));
        setServiceValid(true);
    } else if (name == q->service() && newOwner.isEmpty()) {
        qCDebug(logUtil) << "Service disappeared";
        setServiceValid(false);
    }
}

//////////////////////////////////////////////////////////
// class DDBusInterface
//////////////////////////////////////////////////////////

DDBusInterface::DDBusInterface(const QString &service, const QString &path, const QString &interface, const QDBusConnection &connection, QObject *parent)
    : QDBusAbstractInterface(service, path, interface.toLatin1(), connection, parent)
    , d_ptr(new DDBusInterfacePrivate(this, parent))
{
    qCDebug(logUtil) << "DDBusInterface created - service:" << service << "path:" << path << "interface:" << interface;
}

DDBusInterface::~DDBusInterface() 
{
    qCDebug(logUtil) << "DDBusInterface destructor called";
}

bool DDBusInterface::serviceValid() const
{
    Q_D(const DDBusInterface);
    qCDebug(logUtil) << "Service valid check:" << (d->m_serviceValid ? "true" : "false");
    return d->m_serviceValid;
}

QString DDBusInterface::suffix() const
{
    Q_D(const DDBusInterface);
    qCDebug(logUtil) << "Getting suffix:" << d->m_suffix;
    return d->m_suffix;
}

void DDBusInterface::setSuffix(const QString &suffix)
{
    Q_D(DDBusInterface);
    qCDebug(logUtil) << "Setting suffix:" << suffix;
    d->m_suffix = suffix;
}

inline QString originalPropname(const char *propname, QString suffix)
{
    QString propStr(propname);
    QString result = propStr.left(propStr.length() - suffix.length());
    qCDebug(logUtil) << "Original propname:" << propname << "->" << result;
    return result;
}

QVariant DDBusInterface::property(const char *propName)
{
    Q_D(DDBusInterface);
    qCDebug(logUtil) << "Getting property:" << propName;

    QDBusMessage msg = QDBusMessage::createMethodCall(service(), path(), PropertiesInterface, QStringLiteral("Get"));
    msg << interface() << originalPropname(propName, d->m_suffix);
    QDBusPendingReply<QVariant> prop = connection().asyncCall(msg);
    if (prop.value().isValid()) {
        qCDebug(logUtil) << "Property value is valid";
        // if there is no parent, return value
        if (!parent()) {
            qCWarning(logUtil, "No parent object, property value may be invalid");
            return prop.value();
        }
        auto metaObject = parent()->metaObject();
        QVariant propresult = prop.value();
        int i = metaObject->indexOfProperty(propName);
        if (i != -1) {
            qCDebug(logUtil) << "Property found in parent at index:" << i;
            QMetaProperty metaProperty = metaObject->property(i);
            // try to use property in parent to unwrap the value
            propresult = demarshall(metaProperty, propresult);
        } else {
            qCDebug(logUtil) << "Property not found in parent:" << propName;
        }
        return propresult;
    }

    qCDebug(logUtil) << "Property value not valid, setting up async call";
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(prop, this);
    watcher->setProperty(PropertyName, propName);
    connect(watcher, &QDBusPendingCallWatcher::finished, d, &DDBusInterfacePrivate::onAsyncPropertyFinished);

    return QVariant();
}

void DDBusInterface::setProperty(const char *propName, const QVariant &value)
{
    Q_D(const DDBusInterface);
    qCDebug(logUtil) << "Setting property:" << propName << "with value type:" << value.typeName();
    
    QDBusMessage msg = QDBusMessage::createMethodCall(service(), path(), PropertiesInterface, QStringLiteral("Set"));
    msg << interface() << originalPropname(propName, d->m_suffix) << QVariant::fromValue(QDBusVariant(value));

    QDBusPendingReply<void> reply = connection().asyncCall(msg);
    reply.waitForFinished();
    if (!reply.isValid()) {
        qCWarning(logUtil, "Set property failed: %s", qPrintable(reply.error().message()));
    } else {
        qCDebug(logUtil) << "Property set successfully";
    }
}
DCORE_END_NAMESPACE
