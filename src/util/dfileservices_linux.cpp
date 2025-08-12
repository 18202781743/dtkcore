// SPDX-FileCopyrightText: 2017 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDebug>
#include <QFile>
#include <QLoggingCategory>

#include "dfileservices.h"

DCORE_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(logUtil, "dtk.core.util")

#define EASY_CALL_DBUS(name)\
    QDBusInterface *interface = fileManager1DBusInterface();\
    return interface && interface->call(#name, urls2uris(urls), startupId).type() != QDBusMessage::ErrorMessage;

static QDBusInterface *fileManager1DBusInterface()
{
    qCDebug(logUtil) << "Getting FileManager1 DBus interface";
    static QDBusInterface interface(QStringLiteral("org.freedesktop.FileManager1"),
                                    QStringLiteral("/org/freedesktop/FileManager1"),
                                    QStringLiteral("org.freedesktop.FileManager1"));
    return &interface;
}

static QStringList urls2uris(const QList<QUrl> &urls)
{
    qCDebug(logUtil) << "Converting" << urls.size() << "URLs to URIs";
    QStringList list;

    list.reserve(urls.size());

    for (const QUrl &url : urls) {
        list << url.toString();
    }

    return list;
}

static QList<QUrl> path2urls(const QList<QString> &paths)
{
    qCDebug(logUtil) << "Converting" << paths.size() << "paths to URLs";
    QList<QUrl> list;

    list.reserve(paths.size());

    for (const QString &path : paths) {
        list << QUrl::fromLocalFile(path);
    }

    return list;
}

bool DFileServices::showFolder(QString localFilePath, const QString &startupId)
{
    qCDebug(logUtil) << "showFolder called with path:" << localFilePath << "startupId:" << startupId;
    return showFolder(QUrl::fromLocalFile(localFilePath), startupId);
}

bool DFileServices::showFolders(const QList<QString> localFilePaths, const QString &startupId)
{
    qCDebug(logUtil) << "showFolders called with" << localFilePaths.size() << "paths, startupId:" << startupId;
    return showFolders(path2urls(localFilePaths), startupId);
}

bool DFileServices::showFolder(QUrl url, const QString &startupId)
{
    qCDebug(logUtil) << "showFolder called with URL:" << url.toString() << "startupId:" << startupId;
    return showFolders(QList<QUrl>() << url, startupId);
}

bool DFileServices::showFolders(const QList<QUrl> urls, const QString &startupId)
{
    qCDebug(logUtil) << "showFolders called with" << urls.size() << "URLs, startupId:" << startupId;
    EASY_CALL_DBUS(ShowFolders)
}

bool DFileServices::showFileItemPropertie(QString localFilePath, const QString &startupId)
{
    qCDebug(logUtil) << "showFileItemPropertie called with path:" << localFilePath << "startupId:" << startupId;
    return showFileItemPropertie(QUrl::fromLocalFile(localFilePath), startupId);
}

bool DFileServices::showFileItemProperties(const QList<QString> localFilePaths, const QString &startupId)
{
    qCDebug(logUtil) << "showFileItemProperties called with" << localFilePaths.size() << "paths, startupId:" << startupId;
    return showFileItemProperties(path2urls(localFilePaths), startupId);
}

bool DFileServices::showFileItemPropertie(QUrl url, const QString &startupId)
{
    qCDebug(logUtil) << "showFileItemPropertie called with URL:" << url.toString() << "startupId:" << startupId;
    return showFileItemProperties(QList<QUrl>() << url, startupId);
}

bool DFileServices::showFileItemProperties(const QList<QUrl> urls, const QString &startupId)
{
    qCDebug(logUtil) << "showFileItemProperties called with" << urls.size() << "URLs, startupId:" << startupId;
    EASY_CALL_DBUS(ShowItemProperties)
}

bool DFileServices::showFileItem(QString localFilePath, const QString &startupId)
{
    qCDebug(logUtil) << "showFileItem called with path:" << localFilePath << "startupId:" << startupId;
    return showFileItem(QUrl::fromLocalFile(localFilePath), startupId);
}

bool DFileServices::showFileItems(const QList<QString> localFilePaths, const QString &startupId)
{
    qCDebug(logUtil) << "showFileItems called with" << localFilePaths.size() << "paths, startupId:" << startupId;
    return showFileItems(path2urls(localFilePaths), startupId);
}

bool DFileServices::showFileItem(QUrl url, const QString &startupId)
{
    qCDebug(logUtil) << "showFileItem called with URL:" << url.toString() << "startupId:" << startupId;
    return showFileItems(QList<QUrl>() << url, startupId);
}

bool DFileServices::showFileItems(const QList<QUrl> urls, const QString &startupId)
{
    qCDebug(logUtil) << "showFileItems called with" << urls.size() << "URLs, startupId:" << startupId;
    EASY_CALL_DBUS(ShowItems)
}

bool DFileServices::trash(QString localFilePath)
{
    qCDebug(logUtil) << "trash called with path:" << localFilePath;
    return trash(QUrl::fromLocalFile(localFilePath));
}

bool DFileServices::trash(const QList<QString> localFilePaths)
{
    qCDebug(logUtil) << "trash called with" << localFilePaths.size() << "paths";
    return trash(path2urls(localFilePaths));
}

bool DFileServices::trash(QUrl url)
{
    qCDebug(logUtil) << "trash called with URL:" << url.toString();
    return trash(QList<QUrl>() << url);
}

bool DFileServices::trash(const QList<QUrl> urls)
{
    qCDebug(logUtil) << "trash called with" << urls.size() << "URLs";
    EASY_CALL_DBUS(Trash)
}

QString DFileServices::errorMessage()
{
    qCDebug(logUtil) << "errorMessage called";
    return QString();
}

DCORE_END_NAMESPACE
