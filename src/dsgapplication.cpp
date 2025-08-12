// SPDX-FileCopyrightText: 2022-2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dsgapplication.h"

#include <sys/syscall.h>
#include <unistd.h>

#include <QDir>
#include <QFile>
#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QRegularExpression>
#include <QLoggingCategory>

#include <dbus/dbus.h>

#ifdef QT_DEBUG
Q_LOGGING_CATEGORY(dsgApp, "dtk.core.dsg")
#else
Q_LOGGING_CATEGORY(dsgApp, "dtk.core.dsg", QtInfoMsg)
#endif

DCORE_BEGIN_NAMESPACE

// D-Bus resource deleters for RAII management
static void dbusErrorDeleter(DBusError *error) {
    if (error) { dbus_error_free(error); delete error; }
}

static void dbusConnectionDeleter(DBusConnection *conn) {
    if (conn) dbus_connection_unref(conn);
}

static void dbusMessageDeleter(DBusMessage *msg) {
    if (msg) dbus_message_unref(msg);
}

// D-Bus utility functions using libdbus-1
static bool checkDBusServiceActivatable(const QString &service)
{
    qCDebug(dsgApp) << "Checking if service is activatable:" << service;
    auto error = std::unique_ptr<DBusError, void(*)(DBusError*)>(new DBusError, dbusErrorDeleter);
    dbus_error_init(error.get());

    DBusConnection *connection = dbus_bus_get(DBUS_BUS_SESSION, error.get());
    if (dbus_error_is_set(error.get())) {
        qCWarning(dsgApp) << "Failed to connect to session bus:" << error->message;
        return false;
    }

    if (!connection) {
        qCWarning(dsgApp) << "Failed to get session bus connection";
        return false;
    }
    auto connGuard = std::unique_ptr<DBusConnection, void(*)(DBusConnection*)>(connection, dbusConnectionDeleter);

    // Create method call to check if service is registered
    DBusMessage *msg = dbus_message_new_method_call(
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "NameHasOwner"
    );

    if (!msg) {
        qCWarning(dsgApp) << "Failed to create D-Bus message";
        return false;
    }
    auto msgGuard = std::unique_ptr<DBusMessage, void(*)(DBusMessage*)>(msg, dbusMessageDeleter);

    const char *serviceName = service.toUtf8().constData();
    if (!dbus_message_append_args(msg, DBUS_TYPE_STRING, &serviceName, DBUS_TYPE_INVALID)) {
        qCWarning(dsgApp) << "Failed to append arguments to D-Bus message";
        return false;
    }

    // Send message and get reply
    DBusMessage *reply = dbus_connection_send_with_reply_and_block(connection, msg, 1000, error.get());
    msgGuard.reset(); // msg is consumed by the call

    if (dbus_error_is_set(error.get())) {
        qCWarning(dsgApp) << "D-Bus call failed:" << error->message;
        return false;
    }

    if (!reply) {
        qCWarning(dsgApp) << "No reply received from D-Bus";
        return false;
    }
    auto replyGuard = std::unique_ptr<DBusMessage, void(*)(DBusMessage*)>(reply, dbusMessageDeleter);

    dbus_bool_t hasOwner = FALSE;
    if (!dbus_message_get_args(reply, error.get(), DBUS_TYPE_BOOLEAN, &hasOwner, DBUS_TYPE_INVALID)) {
        qCWarning(dsgApp) << "Failed to parse D-Bus reply:" << error->message;
        return false;
    }

    if (!hasOwner) {
        qCDebug(dsgApp) << "Service has no owner";
        return false;
    }

    qCDebug(dsgApp) << "Service has owner, checking if activatable";

    // Check if service is activatable
    DBusMessage *msg2 = dbus_message_new_method_call(
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "ListActivatableNames"
    );

    if (!msg2) {
        qCWarning(dsgApp) << "Failed to create ListActivatableNames message";
        return false;
    }
    auto msg2Guard = std::unique_ptr<DBusMessage, void(*)(DBusMessage*)>(msg2, dbusMessageDeleter);

    DBusMessage *reply2 = dbus_connection_send_with_reply_and_block(connection, msg2, 1000, error.get());

    if (dbus_error_is_set(error.get())) {
        qCWarning(dsgApp) << "ListActivatableNames call failed:" << error->message;
        return false;
    }

    if (!reply2) {
        qCWarning(dsgApp) << "No reply received from ListActivatableNames";
        return false;
    }
    auto reply2Guard = std::unique_ptr<DBusMessage, void(*)(DBusMessage*)>(reply2, dbusMessageDeleter);

    DBusMessageIter iter, array_iter;
    dbus_message_iter_init(reply2, &iter);

    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) {
        qCWarning(dsgApp) << "Invalid reply format from ListActivatableNames";
        return false;
    }

    dbus_message_iter_recurse(&iter, &array_iter);
    bool found = false;

    while (dbus_message_iter_get_arg_type(&array_iter) == DBUS_TYPE_STRING) {
        const char *name;
        dbus_message_iter_get_basic(&array_iter, &name);
        if (service == QString::fromUtf8(name)) {
            found = true;
            qCDebug(dsgApp) << "Service found in activatable names list";
            break;
        }
        dbus_message_iter_next(&array_iter);
    }

    qCDebug(dsgApp) << "Service activatable check result:" << found;
    return found;
}

static QByteArray callDBusIdentifyMethod(const QString &service, const QString &path, const QString &interface, int pidfd)
{
    qCDebug(dsgApp) << "Calling DBus Identify method for service:" << service << "path:" << path << "interface:" << interface << "pidfd:" << pidfd;
    auto error = std::unique_ptr<DBusError, void(*)(DBusError*)>(new DBusError, dbusErrorDeleter);
    dbus_error_init(error.get());

    DBusConnection *connection = dbus_bus_get(DBUS_BUS_SESSION, error.get());
    if (dbus_error_is_set(error.get())) {
        qCWarning(dsgApp) << "Failed to connect to session bus:" << error->message;
        return QByteArray();
    }

    if (!connection) {
        qCWarning(dsgApp) << "Failed to get session bus connection";
        return QByteArray();
    }
    auto connGuard = std::unique_ptr<DBusConnection, void(*)(DBusConnection*)>(connection, dbusConnectionDeleter);

    // Create method call
    DBusMessage *msg = dbus_message_new_method_call(
        service.toUtf8().constData(),
        path.toUtf8().constData(),
        interface.toUtf8().constData(),
        "Identify"
    );

    if (!msg) {
        qCWarning(dsgApp) << "Failed to create D-Bus message";
        return QByteArray();
    }
    auto msgGuard = std::unique_ptr<DBusMessage, void(*)(DBusMessage*)>(msg, dbusMessageDeleter);

    // Append Unix file descriptor
    if (!dbus_message_append_args(msg, DBUS_TYPE_UNIX_FD, &pidfd, DBUS_TYPE_INVALID)) {
        qCWarning(dsgApp) << "Failed to append Unix FD to D-Bus message";
        return QByteArray();
    }

    // Send message and get reply
    DBusMessage *reply = dbus_connection_send_with_reply_and_block(connection, msg, 5000, error.get());
    msgGuard.reset(); // msg is consumed by the call

    if (dbus_error_is_set(error.get())) {
        qCWarning(dsgApp) << "Identify from AM failed:" << error->message;
        return QByteArray();
    }

    if (!reply) {
        qCWarning(dsgApp) << "No reply received from Identify method";
        return QByteArray();
    }
    auto replyGuard = std::unique_ptr<DBusMessage, void(*)(DBusMessage*)>(reply, dbusMessageDeleter);

    const char *result;
    if (!dbus_message_get_args(reply, error.get(), DBUS_TYPE_STRING, &result, DBUS_TYPE_INVALID)) {
        qCWarning(dsgApp) << "Failed to parse Identify reply:" << error->message;
        return QByteArray();
    }

    QByteArray appId = QByteArray(result);
    qCInfo(dsgApp) << "AppId is fetched from AM, and value is" << appId;

    return appId;
}

static inline QByteArray getSelfAppId() {
    qCDebug(dsgApp) << "Getting self app ID from environment";
    // The env is set by the application starter(eg, org.desktopspec.ApplicationManager service)
    QByteArray selfId = qgetenv("DSG_APP_ID");
    if (!selfId.isEmpty()) {
        qCDebug(dsgApp) << "Found self app ID from environment:" << selfId;
        return selfId;
    }
    qCDebug(dsgApp) << "No self app ID in environment, getting from process ID";
    return DSGApplication::getId(QCoreApplication::applicationPid());
}

static bool isServiceActivatable(const QString &service)
{
    qCDebug(dsgApp) << "Checking if service is activatable:" << service;
    auto result = checkDBusServiceActivatable(service);
    qCDebug(dsgApp) << "Service activatable check result:" << result;
    return result;
}

// Format appId to valid.
static QByteArray formatAppId(const QByteArray &appId)
{
    qCDebug(dsgApp) << "Formatting app ID:" << appId;
    static const QRegularExpression regex("[^\\w\\-\\.]");
    QString format(appId);
    format.replace(QDir::separator(), ".");
    qCDebug(dsgApp) << "After replacing separators:" << format;
    format = format.replace(regex, QStringLiteral("-"));
    qCDebug(dsgApp) << "After regex replacement:" << format;
    const QString InvalidPrefix{"."};
    if (format.startsWith(InvalidPrefix)) {
        qCDebug(dsgApp) << "Removing invalid prefix from app ID";
        format = format.mid(InvalidPrefix.size());
        qCDebug(dsgApp) << "After removing prefix:" << format;
    }
    QByteArray result = format.toLocal8Bit();
    qCDebug(dsgApp) << "Formatted app ID:" << result;
    return result;
}

QByteArray DSGApplication::id()
{
    qCDebug(dsgApp) << "Getting application ID";
    static QByteArray selfId = getSelfAppId();
    if (!selfId.isEmpty()) {
        qCDebug(dsgApp) << "Using cached self ID:" << selfId;
        return selfId;
    }
    QByteArray result = selfId;
    if (!qEnvironmentVariableIsSet("DTK_DISABLED_FALLBACK_APPID")) {
        qCDebug(dsgApp) << "Fallback app ID not disabled, trying fallback methods";
        result = QCoreApplication::applicationName().toLocal8Bit();
        if (result.isEmpty()) {
            qCDebug(dsgApp) << "Application name is empty, trying /proc/self/cmdline";
            QFile file("/proc/self/cmdline");
            if (file.open(QIODevice::ReadOnly)) {
                result = file.readLine();
                qCDebug(dsgApp) << "Read from /proc/self/cmdline:" << result;
            }
        }
        if (result.isEmpty()) {
            qCDebug(dsgApp) << "Still empty, trying /proc/self/exe symlink";
            const QFileInfo file(QFile::symLinkTarget("/proc/self/exe"));
            if (file.exists()) {
                result = file.absoluteFilePath().toLocal8Bit();
                qCDebug(dsgApp) << "Read from /proc/self/exe:" << result;
            }
        }
        if (!result.isEmpty()) {
            result = formatAppId(result);
            qCDebug(dsgApp) << "The application ID is fallback to" << result;
        }
    }
    if (result.isEmpty()) {
        qCWarning(dsgApp) << "The application ID is empty.";
    }

    return result;
}

/**
 * Get application ID for a given process ID
 *
 * This function has been updated to use libdbus-1 instead of Qt D-Bus to fix
 * the bug(pms:BUG-278055) where calling this function before QCoreApplication 
 * initialization would fail. This is particularly important when the service 
 * is being started by D-Bus activation and DSGApplication::id() is called 
 * during early startup.
 *
 * @param pid Process ID to get the application ID for
 * @return Application ID as QByteArray, or empty array on failure
 */
QByteArray DSGApplication::getId(qint64 pid)
{
    qCDebug(dsgApp, "Getting application ID for PID: %lld", pid);
    if (!isServiceActivatable("org.desktopspec.ApplicationManager1")) {
        qCInfo(dsgApp) << "Can't getId from AM for the " << pid << ", because AM is unavailable.";
        return QByteArray();
    }

    int pidfd = syscall(SYS_pidfd_open, pid, 0);
    if (pidfd < 0) {
        qCWarning(dsgApp) << "pidfd open failed:" << strerror(errno) << ", the pid:" << pid;
        return QByteArray();
    }

    qCDebug(dsgApp, "Successfully opened pidfd: %d", pidfd);
    QByteArray appId = callDBusIdentifyMethod("org.desktopspec.ApplicationManager1",
                                              "/org/desktopspec/ApplicationManager1",
                                              "org.desktopspec.ApplicationManager1",
                                              pidfd);

    close(pidfd);
    qCDebug(dsgApp, "Application ID for PID %lld: %s", pid, appId.constData());
    return appId;
}

DCORE_END_NAMESPACE
