// SPDX-FileCopyrightText: 2017 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dfilewatchermanager.h"
#include "dfilewatcher.h"
#include "base/private/dobject_p.h"

#include <QMap>
#include <QUrl>
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(logFilesystem)

class DFileWatcherManagerPrivate : public DObjectPrivate
{
public:
    DFileWatcherManagerPrivate(DFileWatcherManager *qq);

    QMap<QString, DFileWatcher *> watchersMap;

    D_DECLARE_PUBLIC(DFileWatcherManager)
};

DFileWatcherManagerPrivate::DFileWatcherManagerPrivate(DFileWatcherManager *qq)
    : DObjectPrivate(qq)
{
    qCDebug(logFilesystem, "DFileWatcherManagerPrivate created");
}

/*!
@~english
    \class Dtk::Core::DFileWatcherManager
    \inmodule dtkcore
    \brief The DFileWatcherManager class can help you manage file watchers and get signal when file got changed.
*/

DFileWatcherManager::DFileWatcherManager(QObject *parent)
    : QObject(parent)
    , DObject(*new DFileWatcherManagerPrivate(this))
{
    qCDebug(logFilesystem, "DFileWatcherManager created with parent");
}

DFileWatcherManager::~DFileWatcherManager() {
    qCDebug(logFilesystem, "DFileWatcherManager destroyed");
}

/*!
@~english
  \brief Add file watcher for \a filePath to the file watcher manager.
  \return The file watcher which got created and added into the file watcher manager.
 */
DFileWatcher *DFileWatcherManager::add(const QString &filePath)
{
    qCDebug(logFilesystem, "Adding file watcher for: %s", qPrintable(filePath));
    Q_D(DFileWatcherManager);

    DFileWatcher *watcher = d->watchersMap.value(filePath);

    if (watcher) {
        qCDebug(logFilesystem, "Watcher already exists for: %s", qPrintable(filePath));
        return watcher;
    }

    qCDebug(logFilesystem, "Creating new watcher for: %s", qPrintable(filePath));
    watcher = new DFileWatcher(filePath, this);

    connect(watcher, &DFileWatcher::fileAttributeChanged, this, [this](const QUrl &url) {
        qCDebug(logFilesystem, "File attribute changed: %s", qPrintable(url.toLocalFile()));
        Q_EMIT fileAttributeChanged(url.toLocalFile());
    });
    connect(watcher, &DFileWatcher::fileClosed, this, [this](const QUrl &url) { 
        qCDebug(logFilesystem, "File closed: %s", qPrintable(url.toLocalFile()));
        Q_EMIT fileClosed(url.toLocalFile()); 
    });
    connect(watcher, &DFileWatcher::fileDeleted, this, [this](const QUrl &url) { 
        qCDebug(logFilesystem, "File deleted: %s", qPrintable(url.toLocalFile()));
        Q_EMIT fileDeleted(url.toLocalFile()); 
    });
    connect(watcher, &DFileWatcher::fileModified, this, [this](const QUrl &url) { 
        qCDebug(logFilesystem, "File modified: %s", qPrintable(url.toLocalFile()));
        Q_EMIT fileModified(url.toLocalFile()); 
    });
    connect(watcher, &DFileWatcher::fileMoved, this, [this](const QUrl &fromUrl, const QUrl &toUrl) {
        qCDebug(logFilesystem, "File moved from: %s to: %s", qPrintable(fromUrl.toLocalFile()), qPrintable(toUrl.toLocalFile()));
        Q_EMIT fileMoved(fromUrl.toLocalFile(), toUrl.toLocalFile());
    });
    connect(watcher, &DFileWatcher::subfileCreated, this, [this](const QUrl &url) { 
        qCDebug(logFilesystem, "Subfile created: %s", qPrintable(url.toLocalFile()));
        Q_EMIT subfileCreated(url.toLocalFile()); 
    });

    d->watchersMap[filePath] = watcher;
    watcher->startWatcher();
    qCDebug(logFilesystem, "Watcher started for: %s", qPrintable(filePath));

    return watcher;
}

/*!
@~english
  \brief Remove file watcher for \a filePath from the file watcher manager.
 */
void DFileWatcherManager::remove(const QString &filePath)
{
    qCDebug(logFilesystem, "Removing file watcher for: %s", qPrintable(filePath));
    Q_D(DFileWatcherManager);

    DFileWatcher *watcher = d->watchersMap.take(filePath);

    if (watcher) {
        qCDebug(logFilesystem, "Deleting watcher for: %s", qPrintable(filePath));
        watcher->deleteLater();
    } else {
        qCWarning(logFilesystem, "No watcher found for: %s", qPrintable(filePath));
    }
}

/*!
@~english
  @brief Remove all file watcher
*/
void DFileWatcherManager::removeAll()
{
    qCDebug(logFilesystem, "Removing all file watchers");
    Q_D(DFileWatcherManager);
    qCDebug(logFilesystem, "Removing %d watchers", d->watchersMap.size());
    for (auto it : d->watchersMap) {
        it->deleteLater();
    }
    d->watchersMap.clear();
    qCDebug(logFilesystem, "All watchers removed");
}

/*!
@~english
  @brief Show all file watcher
*/
QStringList DFileWatcherManager::watchedFiles() const
{
    Q_D(const DFileWatcherManager);
    QStringList result = d->watchersMap.keys();
    qCDebug(logFilesystem, "Watched files count: %d", result.size());
    return result;
}

DCORE_END_NAMESPACE
