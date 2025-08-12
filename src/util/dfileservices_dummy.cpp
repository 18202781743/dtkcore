// SPDX-FileCopyrightText: 2017 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dfileservices.h"
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(logUtil, "dtk.core.util")

static QStringList urls2uris(const QList<QUrl> &urls)
{
    qCDebug(logUtil) << "Converting" << urls.size() << "URLs to URIs";
    QStringList list;

    list.reserve(urls.size());

    for (const QUrl url : urls) {
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
    qCWarning(logUtil, "Dummy implementation - showFolder always returns false");
    Q_UNUSED(localFilePath);
    Q_UNUSED(startupId);
    return false;
}

bool DFileServices::showFolders(const QList<QString> localFilePaths, const QString &startupId)
{
    qCDebug(logUtil) << "showFolders called with" << localFilePaths.size() << "paths, startupId:" << startupId;
    qCWarning(logUtil, "Dummy implementation - showFolders always returns false");
    Q_UNUSED(localFilePaths);
    Q_UNUSED(startupId);
    return false;
}

bool DFileServices::showFolder(QUrl url, const QString &startupId)
{
    qCDebug(logUtil) << "showFolder called with URL:" << url.toString() << "startupId:" << startupId;
    qCWarning(logUtil, "Dummy implementation - showFolder always returns false");
    Q_UNUSED(url);
    Q_UNUSED(startupId);
    return false;
}

bool DFileServices::showFolders(const QList<QUrl> urls, const QString &startupId)
{
    qCDebug(logUtil) << "showFolders called with" << urls.size() << "URLs, startupId:" << startupId;
    qCWarning(logUtil, "Dummy implementation - showFolders always returns false");
    Q_UNUSED(urls);
    Q_UNUSED(startupId);
    return false;
}

bool DFileServices::showFileItemPropertie(QString localFilePath, const QString &startupId)
{
    qCDebug(logUtil) << "showFileItemPropertie called with path:" << localFilePath << "startupId:" << startupId;
    qCWarning(logUtil, "Dummy implementation - showFileItemPropertie always returns false");
    Q_UNUSED(localFilePath);
    Q_UNUSED(startupId);
    return false;
}

bool DFileServices::showFileItemProperties(const QList<QString> localFilePaths, const QString &startupId)
{
    qCDebug(logUtil) << "showFileItemProperties called with" << localFilePaths.size() << "paths, startupId:" << startupId;
    qCWarning(logUtil, "Dummy implementation - showFileItemProperties always returns false");
    Q_UNUSED(localFilePaths);
    Q_UNUSED(startupId);
    return false;
}

bool DFileServices::showFileItemPropertie(QUrl url, const QString &startupId)
{
    qCDebug(logUtil) << "showFileItemPropertie called with URL:" << url.toString() << "startupId:" << startupId;
    qCWarning(logUtil, "Dummy implementation - showFileItemPropertie always returns false");
    Q_UNUSED(url);
    Q_UNUSED(startupId);
    return false;
}

bool DFileServices::showFileItemProperties(const QList<QUrl> urls, const QString &startupId)
{
    qCDebug(logUtil) << "showFileItemProperties called with" << urls.size() << "URLs, startupId:" << startupId;
    qCWarning(logUtil, "Dummy implementation - showFileItemProperties always returns false");
    Q_UNUSED(urls);
    Q_UNUSED(startupId);
    return false;
}

bool DFileServices::showFileItem(QString localFilePath, const QString &startupId)
{
    qCDebug(logUtil) << "showFileItem called with path:" << localFilePath << "startupId:" << startupId;
    qCWarning(logUtil, "Dummy implementation - showFileItem always returns false");
    Q_UNUSED(localFilePath);
    Q_UNUSED(startupId);
    return false;
}

bool DFileServices::showFileItems(const QList<QString> localFilePaths, const QString &startupId)
{
    qCDebug(logUtil) << "showFileItems called with" << localFilePaths.size() << "paths, startupId:" << startupId;
    qCWarning(logUtil, "Dummy implementation - showFileItems always returns false");
    Q_UNUSED(localFilePaths);
    Q_UNUSED(startupId);
    return false;
}

bool DFileServices::showFileItem(QUrl url, const QString &startupId)
{
    qCDebug(logUtil) << "showFileItem called with URL:" << url.toString() << "startupId:" << startupId;
    qCWarning(logUtil, "Dummy implementation - showFileItem always returns false");
    Q_UNUSED(url);
    Q_UNUSED(startupId);
    return false;
}

bool DFileServices::showFileItems(const QList<QUrl> urls, const QString &startupId)
{
    qCDebug(logUtil) << "showFileItems called with" << urls.size() << "URLs, startupId:" << startupId;
    qCWarning(logUtil, "Dummy implementation - showFileItems always returns false");
    Q_UNUSED(urls);
    Q_UNUSED(startupId);
    return false;
}

bool DFileServices::trash(QString localFilePath)
{
    qCDebug(logUtil) << "trash called with path:" << localFilePath;
    qCWarning(logUtil, "Dummy implementation - trash always returns false");
    Q_UNUSED(localFilePath);
    return false;
}

bool DFileServices::trash(const QList<QString> localFilePaths)
{
    qCDebug(logUtil) << "trash called with" << localFilePaths.size() << "paths";
    qCWarning(logUtil, "Dummy implementation - trash always returns false");
    Q_UNUSED(localFilePaths);
    return false;
}

bool DFileServices::trash(QUrl url)
{
    qCDebug(logUtil) << "trash called with URL:" << url.toString();
    qCWarning(logUtil, "Dummy implementation - trash always returns false");
    Q_UNUSED(url);
    return false;
}

bool DFileServices::trash(const QList<QUrl> urls)
{
    qCDebug(logUtil) << "trash called with" << urls.size() << "URLs";
    qCWarning(logUtil, "Dummy implementation - trash always returns false");
    Q_UNUSED(urls);
    return false;
}


QString DFileServices::errorMessage()
{
    qCDebug(logUtil) << "errorMessage called";
    qCWarning(logUtil, "Dummy implementation - errorMessage returns empty string");
    return QString();
}

DCORE_END_NAMESPACE
