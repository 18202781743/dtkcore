// SPDX-FileCopyrightText: 2017 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dfilesystemwatcher.h"
#include "private/dfilesystemwatcher_linux_p.h"

#include <QFileInfo>
#include <QDir>
#include <QSocketNotifier>
#include <QDebug>
#include <QMultiMap>
#include <QLoggingCategory>
#include <QSet>
#include <QList>

#include <sys/inotify.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

DCORE_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(logFilesystem)

DFileSystemWatcherPrivate::DFileSystemWatcherPrivate(int fd, DFileSystemWatcher *qq)
    : DObjectPrivate(qq)
    , inotifyFd(fd)
    , notifier(fd, QSocketNotifier::Read, qq)
{
    qCDebug(logFilesystem, "DFileSystemWatcherPrivate created with fd: %d", fd);
    fcntl(inotifyFd, F_SETFD, FD_CLOEXEC);
    qq->connect(&notifier, SIGNAL(activated(int)), qq, SLOT(_q_readFromInotify()));
}

DFileSystemWatcherPrivate::~DFileSystemWatcherPrivate()
{
    qCDebug(logFilesystem, "DFileSystemWatcherPrivate destroyed");
    notifier.setEnabled(false);
    Q_FOREACH (int id, pathToID)
        inotify_rm_watch(inotifyFd, id < 0 ? -id : id);

    ::close(inotifyFd);
}

QStringList DFileSystemWatcherPrivate::addPaths(const QStringList &paths, QStringList *files, QStringList *directories)
{
    qCDebug(logFilesystem, "Adding %d paths to watch", paths.size());
    QStringList p = paths;
    QMutableListIterator<QString> it(p);
    while (it.hasNext()) {
        QString path = it.next();
        QFileInfo fi(path);
        bool isDir = fi.isDir();
        qCDebug(logFilesystem, "Processing path: %s, isDir: %s", qPrintable(path), isDir ? "true" : "false");
        
        if (isDir) {
            if (directories->contains(path)) {
                qCDebug(logFilesystem, "Directory already watched: %s", qPrintable(path));
                continue;
            }
        } else {
            if (files->contains(path)) {
                qCDebug(logFilesystem, "File already watched: %s", qPrintable(path));
                continue;
            }
        }

        int wd = inotify_add_watch(inotifyFd,
                                   QFile::encodeName(path),
                                   (isDir
                                    ? (0
                                       | IN_ATTRIB
                                       | IN_MOVE
                                       | IN_MOVE_SELF
                                       | IN_CREATE
                                       | IN_DELETE
                                       | IN_DELETE_SELF
                                       | IN_MODIFY
                                       )
                                    : (0
                                       | IN_ATTRIB
                                       | IN_CLOSE_WRITE
                                       | IN_MODIFY
                                       | IN_MOVE
                                       | IN_MOVE_SELF
                                       | IN_DELETE_SELF
                                       )));
        if (wd < 0) {
            qCWarning(logFilesystem, "inotify_add_watch failed for path: %s", qPrintable(path));
            perror("DFileSystemWatcherPrivate::addPaths: inotify_add_watch failed");
            continue;
        }

        qCDebug(logFilesystem, "Successfully added watch for path: %s, wd: %d", qPrintable(path), wd);
        it.remove();

        int id = isDir ? -wd : wd;
        if (id < 0) {
            directories->append(path);
            qCDebug(logFilesystem, "Added directory to watch list: %s", qPrintable(path));
        } else {
            files->append(path);
            qCDebug(logFilesystem, "Added file to watch list: %s", qPrintable(path));
        }

        pathToID.insert(path, id);
        idToPath.insert(id, path);
    }

    qCDebug(logFilesystem, "Added %d paths successfully", paths.size() - p.size());
    return p;
}

QStringList DFileSystemWatcherPrivate::removePaths(const QStringList &paths, QStringList *files, QStringList *directories)
{
    qCDebug(logFilesystem, "Removing %d paths from watch", paths.size());
    QStringList p = paths;
    QMutableListIterator<QString> it(p);
    while (it.hasNext()) {
        QString path = it.next();
        QFileInfo fi(path);
        bool isDir = fi.isDir();
        qCDebug(logFilesystem, "Processing path for removal: %s, isDir: %s", qPrintable(path), isDir ? "true" : "false");
        
        if (isDir) {
            if (!directories->contains(path)) {
                qCDebug(logFilesystem, "Directory not in watch list: %s", qPrintable(path));
                continue;
            }
        } else {
            if (!files->contains(path)) {
                qCDebug(logFilesystem, "File not in watch list: %s", qPrintable(path));
                continue;
            }
        }

        int wd = pathToID.value(path);
        if (wd == 0) {
            qCWarning(logFilesystem, "No watch descriptor found for path: %s", qPrintable(path));
            continue;
        }

        int ret = inotify_rm_watch(inotifyFd, wd < 0 ? -wd : wd);
        if (ret < 0) {
            qCWarning(logFilesystem, "inotify_rm_watch failed for path: %s", qPrintable(path));
            perror("DFileSystemWatcherPrivate::removePaths: inotify_rm_watch failed");
            continue;
        }

        qCDebug(logFilesystem, "Successfully removed watch for path: %s, wd: %d", qPrintable(path), wd);
        pathToID.remove(path);
        it.remove();

        if (wd < 0) {
            directories->removeOne(path);
            qCDebug(logFilesystem, "Removed directory from watch list: %s", qPrintable(path));
        } else {
            files->removeOne(path);
            qCDebug(logFilesystem, "Removed file from watch list: %s", qPrintable(path));
        }
    }
    
    return p;
}

void DFileSystemWatcherPrivate::_q_readFromInotify()
{
    D_Q(DFileSystemWatcher);
    qCDebug(logFilesystem, "Reading from inotify");
    
    // Local variables for event processing
    QList<inotify_event *> eventList;
    QMultiMap<int, QString> batch_pathmap;
    QMultiMap<int, QString> cookieToFilePath;
    QMultiMap<int, QString> cookieToFileName;
    QSet<int> hasMoveFromByCookie;
    
    int buffSize = 0;
    if (ioctl(inotifyFd, FIONREAD, (char *) &buffSize) == -1 || buffSize == 0) {
        qCDebug(logFilesystem, "No data available from inotify");
        return;
    }

    QVarLengthArray<char, 4096> buffer(buffSize);
    buffSize = read(inotifyFd, buffer.data(), buffSize);
    if (buffSize == -1) {
        qCWarning(logFilesystem, "Failed to read from inotify");
        return;
    }

    qCDebug(logFilesystem, "Read %d bytes from inotify", buffSize);
    int pos = 0;
    while (pos < buffSize) {
        // don't read past the end of the buffer
        if (pos + int(sizeof(struct inotify_event)) > buffSize) {
            qCWarning(logFilesystem, "Incomplete inotify event at position %d", pos);
            break;
        }

        struct inotify_event *pevent = reinterpret_cast<struct inotify_event *>(buffer.data() + pos);
        int eventSize = sizeof(struct inotify_event) + pevent->len;
        if (pos + eventSize > buffSize) {
            qCWarning(logFilesystem, "Incomplete inotify event at position %d", pos);
            break;
        }

        qCDebug(logFilesystem, "Processing inotify event: wd=%d, mask=0x%x, len=%d", 
                pevent->wd, pevent->mask, pevent->len);

        QStringList paths;

        pos += sizeof(inotify_event) + pevent->len;

        int id = pevent->wd;
        paths = idToPath.values(id);
        if (paths.empty()) {
            // perhaps a directory?
            id = -id;
            paths = idToPath.values(id);
            if (paths.empty())
                continue;
        }

        if (!(pevent->mask & IN_MOVED_TO) || !hasMoveFromByCookie.contains(pevent->cookie)) {
            auto it = std::find_if(eventList.begin(), eventList.end(), [pevent](inotify_event *e){
                    return pevent->wd == e->wd && pevent->mask == e->mask &&
                           pevent->cookie == e->cookie &&
                           pevent->len == e->len &&
                           !strcmp(pevent->name, e->name);
            });

            if (it==eventList.end()) {
                eventList.append(pevent);
            }
#ifdef QT_DEBUG
            else {
                qDebug() << "exist event:" << "pevent->wd" << pevent->wd <<
                            "pevent->mask" << pevent->mask <<
                            "pevent->cookie" << pevent->cookie << "exist counts " << ++exist_count;
            }
#endif
            const QList<QString> bps = batch_pathmap.values(id);
            for (auto &path : paths) {
                if (!bps.contains(path)) {
                    batch_pathmap.insert(id, path);
                }
            }
        }

        if (pevent->mask & IN_MOVED_TO) {
            for (auto &path : paths) {
                cookieToFilePath.insert(pevent->cookie, path);
            }
            cookieToFileName.insert(pevent->cookie, QString::fromUtf8(pevent->name));
        }

        if (pevent->mask & IN_MOVED_FROM)
            hasMoveFromByCookie << pevent->cookie;
    }

//    qDebug() << "event count:" << eventList.count();

    QList<inotify_event *>::const_iterator it = eventList.constBegin();
    while (it != eventList.constEnd()) {
        const inotify_event &event = **it;
        ++it;

//        qDebug() << "inotify event, wd" << event.wd << "cookie" << event.cookie << "mask" << hex << event.mask;

        int id = event.wd;
        QStringList paths = batch_pathmap.values(id);

        if (paths.empty()) {
            id = -id;
            paths = batch_pathmap.values(id);

            if (paths.empty())
                continue;
        }
        const QString &name = QString::fromUtf8(event.name);

        for (auto &path : paths) {
//            qDebug() << "event for path" << path;

//            /// TODO: Existence of invalid utf8 characters QFile can not read the file information
//            if (event.name != QString::fromLocal8Bit(event.name).toLocal8Bit()) {
//                if (event.mask & (IN_CREATE | IN_MOVED_TO)) {
//                    DFMGlobal::fileNameCorrection(path);
//                }
//            }

            if ((event.mask & (IN_DELETE_SELF | IN_MOVE_SELF | IN_UNMOUNT)) != 0) {
                do {
                    if (event.mask & IN_MOVE_SELF) {
                        QMultiMap<int, QString>::const_iterator iterator = cookieToFilePath.constBegin();

                        bool isMove = false;

                        while (iterator != cookieToFilePath.constEnd()) {
                            const QString &_path = iterator.value();
                            const QString &_name = cookieToFileName.value(iterator.key());

                            if (QFileInfo(_path + QDir::separator() + _name) == QFileInfo(path)) {
                                isMove = true;
                                break;
                            }

                            ++iterator;
                        }

                        if (isMove)
                            break;
                    }

                    /// Keep watcher
//                    pathToID.remove(path);
//                    idToPath.remove(id, getPathFromID(id));
//                    if (!idToPath.contains(id))
//                        inotify_rm_watch(inotifyFd, event.wd);

//                    if (id < 0)
//                        onDirectoryChanged(path, true);
//                    else
//                        onFileChanged(path, true);

                    Q_EMIT q->fileDeleted(path, QString(), DFileSystemWatcher::QPrivateSignal());
                } while (false);
            } else {
                if (id < 0)
                    onDirectoryChanged(path, false);
                else
                    onFileChanged(path, false);
            }

            QString filePath = path;

            if (id < 0) {
                if (path.endsWith(QDir::separator()))
                    filePath = path + name;
                else
                    filePath = path  + QDir::separator() + name;
            }

            if (event.mask & IN_CREATE) {
//                qDebug() << "IN_CREATE" << filePath << name;

                if (name.isEmpty()) {
                    if (pathToID.contains(path)) {
                        q->removePath(path);
                        q->addPath(path);
                    }
                } else if (pathToID.contains(filePath)) {
                    q->removePath(filePath);
                    q->addPath(filePath);
                }

                Q_EMIT q->fileCreated(path, name, DFileSystemWatcher::QPrivateSignal());
            }

            if (event.mask & IN_DELETE) {
//                qDebug() << "IN_DELETE" << filePath;

                Q_EMIT q->fileDeleted(path, name, DFileSystemWatcher::QPrivateSignal());
            }

            if (event.mask & IN_MOVED_FROM) {
                const QString toName = cookieToFileName.value(event.cookie);

                if (cookieToFilePath.values(event.cookie).empty()) {
                    Q_EMIT q->fileMoved(path, name, QString(), QString(), DFileSystemWatcher::QPrivateSignal());
                } else {
                    for (QString &toPath : cookieToFilePath.values(event.cookie)) {
//                        qDebug() << "IN_MOVED_FROM" << filePath << "to path:" << toPath << "to name:" << toName;

                        Q_EMIT q->fileMoved(path, name, toPath, toName, DFileSystemWatcher::QPrivateSignal());
                    }
                }
            }

            if (event.mask & IN_MOVED_TO) {
//                qDebug() << "IN_MOVED_TO" << filePath;

                if (!hasMoveFromByCookie.contains(event.cookie))
                    Q_EMIT q->fileMoved(QString(), QString(), path, name, DFileSystemWatcher::QPrivateSignal());
            }

            if (event.mask & IN_ATTRIB) {
//                qDebug() << "IN_ATTRIB" <<  event.mask << filePath;

                Q_EMIT q->fileAttributeChanged(path, name, DFileSystemWatcher::QPrivateSignal());
            }

            /*only monitor file close event which is opend by write mode*/
            if (event.mask & IN_CLOSE_WRITE) {
//                qDebug() << "IN_CLOSE_WRITE" <<  event.mask << filePath;

                Q_EMIT q->fileClosed(path, id < 0 ? name : QString(), DFileSystemWatcher::QPrivateSignal());
            }

            if (event.mask & IN_MODIFY) {
//                qDebug() << "IN_MODIFY" <<  event.mask << filePath << name;

                Q_EMIT q->fileModified(path, name, DFileSystemWatcher::QPrivateSignal());
            }
        }
    }
}

void DFileSystemWatcherPrivate::onFileChanged(const QString &path, bool removed)
{
    D_Q(DFileSystemWatcher);
    qCDebug(logFilesystem, "File changed: %s, removed: %s", qPrintable(path), removed ? "true" : "false");
    if (removed) {
        qCDebug(logFilesystem, "Emitting fileDeleted signal for: %s", qPrintable(path));
        Q_EMIT q->fileDeleted(path, QString(), DFileSystemWatcher::QPrivateSignal());
    } else {
        qCDebug(logFilesystem, "Emitting fileModified signal for: %s", qPrintable(path));
        Q_EMIT q->fileModified(path, QString(), DFileSystemWatcher::QPrivateSignal());
    }
}

void DFileSystemWatcherPrivate::onDirectoryChanged(const QString &path, bool removed)
{
    D_Q(DFileSystemWatcher);
    qCDebug(logFilesystem, "Directory changed: %s, removed: %s", qPrintable(path), removed ? "true" : "false");
    if (removed) {
        qCDebug(logFilesystem, "Emitting fileDeleted signal for directory: %s", qPrintable(path));
        Q_EMIT q->fileDeleted(path, QString(), DFileSystemWatcher::QPrivateSignal());
    } else {
        qCDebug(logFilesystem, "Emitting fileCreated signal for directory: %s", qPrintable(path));
        Q_EMIT q->fileCreated(path, QString(), DFileSystemWatcher::QPrivateSignal());
    }
}

/*!
    \class Dtk::Core::DFileSystemWatcher
    \inmodule dtkcore
    \brief The DFileSystemWatcher class provides an interface for monitoring files and directories for modifications.

    DFileSystemWatcher monitors the file system for changes to files
    and directories by watching a list of specified paths.

    Call addPath() to watch a particular file or directory. Multiple
    paths can be added using the addPaths() function. Existing paths can
    be removed by using the removePath() and removePaths() functions.

    DFileSystemWatcher examines each path added to it. Files that have
    been added to the DFileSystemWatcher can be accessed using the
    files() function, and directories using the directories() function.

    \note On systems running a Linux kernel without inotify support,
    file systems that contain watched paths cannot be unmounted.

    \note Windows CE does not support directory monitoring by
    default as this depends on the file system driver installed.

    \note The act of monitoring files and directories for
    modifications consumes system resources. This implies there is a
    limit to the number of files and directories your process can
    monitor simultaneously. On all BSD variants, for
    example, an open file descriptor is required for each monitored
    file. Some system limits the number of open file descriptors to 256
    by default. This means that addPath() and addPaths() will fail if
    your process tries to add more than 256 files or directories to
    the file system monitor. Also note that your process may have
    other file descriptors open in addition to the ones for files
    being monitored, and these other open descriptors also count in
    the total. OS X uses a different backend and does not
    suffer from this issue.
*/


/*!
    Constructs a new file system watcher object with the given \a parent.
*/
DFileSystemWatcher::DFileSystemWatcher(QObject *parent)
    : QObject(parent)
    , DObject()
{
    int fd = -1;
#ifdef IN_CLOEXEC
    fd = inotify_init1(IN_CLOEXEC | O_NONBLOCK);
#endif
    if (fd == -1) {
        fd = inotify_init1(O_NONBLOCK);
    }

    if (fd != -1) {
        d_d_ptr.reset(new DFileSystemWatcherPrivate(fd, this));
    } else {
        qCritical() << "inotify_init1 failed, and the DFileSystemWatcher is invalid." << strerror(errno);
    }
}

/*!
    Constructs a new file system watcher object with the given \a parent
    which monitors the specified \a paths list.
*/
DFileSystemWatcher::DFileSystemWatcher(const QStringList &paths, QObject *parent)
    : DFileSystemWatcher(parent)
{
    addPaths(paths);
}

/*!
    Destroys the file system watcher.
*/
DFileSystemWatcher::~DFileSystemWatcher()
{ }

/*!
    Adds \a path to the file system watcher if \a path exists. The
    path is not added if it does not exist, or if it is already being
    monitored by the file system watcher.

    If \a path specifies a directory, the directoryChanged() signal
    will be emitted when \a path is modified or removed from disk;
    otherwise the fileChanged() signal is emitted when \a path is
    modified, renamed or removed.

    If the watch was successful, true is returned.

    Reasons for a watch failure are generally system-dependent, but
    may include the resource not existing, access failures, or the
    total watch count limit, if the platform has one.

    \note There may be a system dependent limit to the number of
    files and directories that can be monitored simultaneously.
    If this limit is been reached, \a path will not be monitored,
    and false is returned.

    \sa addPaths(), removePath()
*/
bool DFileSystemWatcher::addPath(const QString &path)
{
    const QStringList &paths = addPaths(QStringList(path));
    return paths.isEmpty();
}

/*!
    Adds each path in \a paths to the file system watcher. Paths are
    not added if they not exist, or if they are already being
    monitored by the file system watcher.

    If a path specifies a directory, the directoryChanged() signal
    will be emitted when the path is modified or removed from disk;
    otherwise the fileChanged() signal is emitted when the path is
    modified, renamed, or removed.

    The return value is a list of paths that could not be watched.

    Reasons for a watch failure are generally system-dependent, but
    may include the resource not existing, access failures, or the
    total watch count limit, if the platform has one.

    \note There may be a system dependent limit to the number of
    files and directories that can be monitored simultaneously.
    If this limit has been reached, the excess \a paths will not
    be monitored, and they will be added to the returned QStringList.

    \sa addPath(), removePaths()
*/
QStringList DFileSystemWatcher::addPaths(const QStringList &paths)
{
    Q_D(DFileSystemWatcher);

    if (!d)
        return paths;

    QStringList p = paths;
    QMutableListIterator<QString> it(p);

    while (it.hasNext()) {
        const QString &path = it.next();
        if (path.isEmpty()) {
            qWarning() << Q_FUNC_INFO << "the path is empty and it is not be watched";
            it.remove();
        }
    }

    if (p.isEmpty()) {
        qWarning() << Q_FUNC_INFO << "all path are filtered and they are not be watched, paths are " << paths;
        return paths;
    }

    p = d->addPaths(p, &d->files, &d->directories);

    return p;
}

/*!
    Removes the specified \a path from the file system watcher.

    If the watch is successfully removed, true is returned.

    Reasons for watch removal failing are generally system-dependent,
    but may be due to the path having already been deleted, for example.

    \sa removePaths(), addPath()
*/
bool DFileSystemWatcher::removePath(const QString &path)
{
    const QStringList &paths = removePaths(QStringList(path));
    return paths.isEmpty();
}

/*!
    Removes the specified \a paths from the file system watcher.

    The return value is a list of paths which were not able to be
    unwatched successfully.

    Reasons for watch removal failing are generally system-dependent,
    but may be due to the path having already been deleted, for example.

    \sa removePath(), addPaths()
*/
QStringList DFileSystemWatcher::removePaths(const QStringList &paths)
{
    Q_D(DFileSystemWatcher);

    if (!d)
        return paths;

    QStringList p = paths;
    QMutableListIterator<QString> it(p);

    while (it.hasNext()) {
        const QString &path = it.next();
        if (path.isEmpty()) {
            qWarning() << Q_FUNC_INFO << "the path is empty and it is not be removed from watched list";
            it.remove();
        }
    }

    if (p.isEmpty()) {
        qWarning() << Q_FUNC_INFO << "all path are filtered and they are not be watched, paths are " << paths;
        return paths;
    }

    p = d->removePaths(p, &d->files, &d->directories);

    return p;
}

/*!
    \fn QStringList DFileSystemWatcher::directories() const

    Returns a list of paths to directories that are being watched.

    \sa files()
*/

/*!
    \fn QStringList DFileSystemWatcher::files() const

    Returns a list of paths to files that are being watched.

    \sa directories()
*/

QStringList DFileSystemWatcher::directories() const
{
    Q_D(const DFileSystemWatcher);

    if (!d)
        return QStringList();

    return d->directories;
}

QStringList DFileSystemWatcher::files() const
{
    Q_D(const DFileSystemWatcher);

    if (!d)
        return QStringList();

    return d->files;
}

DCORE_END_NAMESPACE

#include "moc_dfilesystemwatcher.cpp"
