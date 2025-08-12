// SPDX-FileCopyrightText: 2017 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dexportedinterface.h"
#include "base/private/dobject_p.h"

#include <QHash>
#include <QPair>
#include <QVector>
#include <QVariant>
#include <QDBusConnection>
#include <QDBusVariant>
#include <QDBusContext>
#include <QDBusMessage>
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

#ifdef QT_DEBUG
Q_LOGGING_CATEGORY(logUtil, "dtk.core.util")
#else
Q_LOGGING_CATEGORY(logUtil, "dtk.core.util", QtInfoMsg)
#endif

namespace DUtil {

class DExportedInterfacePrivate;
class DExportedInterfaceDBusInterface : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.deepin.ExportedInterface")

public:
    DExportedInterfaceDBusInterface(DExportedInterfacePrivate *priv);

public Q_SLOTS:
    QStringList list();
    QString help(const QString &action);
    QDBusVariant invoke(QString action, QString parameters);

private:
    DExportedInterfacePrivate *p;
};

class DExportedInterfacePrivate : public DObjectPrivate
{
public:
    DExportedInterfacePrivate(DExportedInterface *q) : DObjectPrivate(q) {
        qCDebug(logUtil) << "DExportedInterfacePrivate created";
    }

private:
    QStringList actionHelp(QString action, int indent);

    QHash<QString, QPair<std::function<QVariant(QString)>, QString>> actions;
    QScopedPointer<DExportedInterfaceDBusInterface> dbusif;
    D_DECLARE_PUBLIC(DExportedInterface)

    friend class DExportedInterfaceDBusInterface;
};

DExportedInterface::DExportedInterface(QObject *parent)
    : QObject(parent),
      DObject(*new DExportedInterfacePrivate(this))
{
    qCDebug(logUtil) << "DExportedInterface created";
    D_D(DExportedInterface);
    QDBusConnection::sessionBus().registerObject("/", d->dbusif.data(), QDBusConnection::RegisterOption::ExportAllSlots);
}

DExportedInterface::~DExportedInterface()
{
    qCDebug(logUtil) << "DExportedInterface destroyed";
    QDBusConnection::sessionBus().unregisterObject("/");
}

void DExportedInterface::registerAction(const QString &action, const QString &description, const std::function<QVariant (QString)> handler)
{
    qCDebug(logUtil) << "Registering action:" << action;
    D_D(DExportedInterface);
    d->actions[action] = {handler, description};
}

QVariant DExportedInterface::invoke(const QString &action, const QString &parameters) const
{
    qCDebug(logUtil) << "Invoking action:" << action << "with parameters:" << parameters;
    D_DC(DExportedInterface);
    if (auto func = d->actions.value(action).first) {
        QVariant result = func(parameters);
        qCDebug(logUtil) << "Action result:" << result.toString();
        return result;
    }
    qCWarning(logUtil) << "Action not found:" << action;
    return QVariant();
}

DExportedInterfacePrivate::DExportedInterfacePrivate(DExportedInterface *q)
    : DObjectPrivate(q)
    , dbusif(new DExportedInterfaceDBusInterface(this))
{}

QStringList DExportedInterfacePrivate::actionHelp(QString action, int indent)
{
    qCDebug(logUtil) << "Getting action help for:" << action;
    QStringList ret;
    if (actions.contains(action)) {
        ret << QString(indent * 2, ' ') + QString("%1: %2").arg(action).arg(actions[action].second);
    }
    return ret;
}

DExportedInterfaceDBusInterface::DExportedInterfaceDBusInterface(DExportedInterfacePrivate *priv)
    : QObject(nullptr)
    , p(priv)
{
    qCDebug(logUtil) << "DExportedInterfaceDBusInterface created";
}

QStringList DExportedInterfaceDBusInterface::list()
{
    qCDebug(logUtil) << "Listing actions";
    QStringList ret;
    for (auto it = p->actions.begin(); it != p->actions.end(); ++it) {
        ret << it.key();
    }
    qCDebug(logUtil) << "Found" << ret.size() << "actions";
    return ret;
}

QString DExportedInterfaceDBusInterface::help(const QString &action)
{
    qCDebug(logUtil) << "Getting help for action:" << action;
    if (action.length()) {
        qCDebug(logUtil) << "Getting specific action help";
        QString result = p->actionHelp(action, 0).join('\n');
        qCDebug(logUtil) << "Help result length:" << result.length();
        return result;
    } else {
        qCDebug(logUtil) << "Getting general help for all actions";
        QString ret = "Available actions:";
        QStringList actions = p->actions.keys();
        actions.sort();
        qCDebug(logUtil) << "Found" << actions.size() << "actions for help";
        for (auto action : actions) {
            ret += QString("\n\n") + p->actionHelp(action, 1).join('\n');
        }
        qCDebug(logUtil) << "General help result length:" << ret.length();
        return ret;
    }
}

QDBusVariant DExportedInterfaceDBusInterface::invoke(QString action, QString parameters)
{
    qCDebug(logUtil) << "DBus interface invoking action:" << action << "with parameters:" << parameters;
    QDBusVariant ret;
    if (!p->actions.contains(action)) {
        qCWarning(logUtil) << "Action not found in DBus interface:" << action;
        sendErrorReply(QDBusError::ErrorType::InvalidArgs, QString("Action \"%1\" is not registered").arg(action));
    } else {
        qCDebug(logUtil) << "Action found, invoking through public interface";
        ret.setVariant(p->q_func()->invoke(action, parameters));
        qCDebug(logUtil) << "DBus invoke completed successfully";
    }
    return ret;
}

}
DCORE_END_NAMESPACE

#include "dexportedinterface.moc"
