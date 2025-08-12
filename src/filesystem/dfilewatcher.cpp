// SPDX-FileCopyrightText: 2017 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dfilewatcher.h"
#include "private/dbasefilewatcher_p.h"

#include "dfilesystemwatcher.h"

#include <QDir>
#include <QDebug>
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(logFilesystem)

static QString joinFilePath(const QString &path, const QString &name)
{
    qCDebug(logFilesystem) << "Joining file path:" << path << "+" << name;
    if (path.endsWith(QDir::separator()))
        return path + name;

    return path + QDir::separator() + name;
}

class DFileWatcherPrivate : DBaseFileWatcherPrivate
{
public:
    DFileWatcherPrivate(DFileWatcher *qq)
        : DBaseFileWatcherPrivate(qq) {
        qCDebug(logFilesystem) << "DFileWatcherPrivate created";
    }

    bool start() Q_DECL_OVERRIDE;
    bool stop() Q_DECL_OVERRIDE;

    void _q_handleFileDeleted(const QString &path, const QString &parentPath);
    void _q_handleFileAttributeChanged(const QString &path, const QString &parentPath);
    void _q_handleFileMoved(const QString &from, const QString &fromParent, const QString &to, const QString &toParent);
    void _q_handleFileCreated(const QString &path, const QString &parentPath);
    void _q_handleFileModified(const QString &path, const QString &parentPath);
    void _q_handleFileClose(const QString &path, const QString &parentPath);

    static QString formatPath(const QString &path);

    QString path;
    QStringList watchFileList;

    static QMap<QString, int> filePathToWatcherCount;

    Q_DECLARE_PUBLIC(DFileWatcher)
};

QMap<QString, int> DFileWatcherPrivate::filePathToWatcherCount;
Q_GLOBAL_STATIC(DFileSystemWatcher, watcher_file_private)

QStringList parentPathList(const QString &path)
{
    qCDebug(logFilesystem) << "Getting parent path list for:" << path;
    QStringList list;
    QDir dir(path);

    list << path;

    while (dir.cdUp()) {
        list << dir.absolutePath();
    }

    qCDebug(logFilesystem) << "Parent path list size:" << list.size();
    return list;
}

bool DFileWatcherPrivate::start()
{
    qCDebug(logFilesystem) << "Starting file watcher for path:" << path;
    Q_Q(DFileWatcher);

    started = true;

    Q_FOREACH (const QString &path, parentPathList(this->path)) {
        if (watchFileList.contains(path))
            continue;

        if (filePathToWatcherCount.value(path, -1) <= 0) {
            if (!watcher_file_private->addPath(path)) {
                qCWarning(logFilesystem) << "Start watch failed, file path:" << path;
                q->stopWatcher();
                started = false;
                return false;
            }
        }

        watchFileList << path;
        filePathToWatcherCount[path] = filePathToWatcherCount.value(path, 0) + 1;
        qCDebug(logFilesystem) << "Added path to watch:" << path;
    }

    q->connect(watcher_file_private, &DFileSystemWatcher::fileDeleted,
               q, &DFileWatcher::onFileDeleted);
    q->connect(watcher_file_private, &DFileSystemWatcher::fileAttributeChanged,
               q, &DFileWatcher::onFileAttributeChanged);
    q->connect(watcher_file_private, &DFileSystemWatcher::fileMoved,
               q, &DFileWatcher::onFileMoved);
    q->connect(watcher_file_private, &DFileSystemWatcher::fileCreated,
               q, &DFileWatcher::onFileCreated);
    q->connect(watcher_file_private, &DFileSystemWatcher::fileModified,
               q, &DFileWatcher::onFileModified);
    q->connect(watcher_file_private, &DFileSystemWatcher::fileClosed,
               q, &DFileWatcher::onFileClosed);

    qCDebug(logFilesystem, "File watcher started successfully");
    return true;
}

bool DFileWatcherPrivate::stop()
{
    qCDebug(logFilesystem) << "Stopping file watcher for path:" << path;
    Q_Q(DFileWatcher);

    started = false;

    q->disconnect(watcher_file_private, 0, q, 0);

    bool ok = true;

    Q_FOREACH (const QString &path, watchFileList) {
        int count = filePathToWatcherCount.value(path, 0);
        qCDebug(logFilesystem) << "Processing path:" << path << "current count:" << count;

        --count;

        if (count <= 0) {
            qCDebug(logFilesystem) << "Removing path from watcher:" << path;
            filePathToWatcherCount.remove(path);
            watchFileList.removeOne(path);
            bool removeResult = watcher_file_private->removePath(path);
            ok = ok && removeResult;
            qCDebug(logFilesystem) << "Path removal result:" << (removeResult ? "success" : "failed");
        } else {
            qCDebug(logFilesystem) << "Updating count for path:" << path << "new count:" << count;
            filePathToWatcherCount[path] = count;
        }
    }

    qCDebug(logFilesystem) << "File watcher stop result:" << (ok ? "success" : "failed");
    return ok;
}

void DFileWatcherPrivate::_q_handleFileDeleted(const QString &path, const QString &parentPath)
{
    qCDebug(logFilesystem) << "Handling file deleted: path=" << path << "parentPath=" << parentPath << "this->path=" << this->path;
    if (path != this->path && parentPath != this->path) {
        qCDebug(logFilesystem) << "Ignoring file deleted event - not relevant to watched path";
        return;
    }

    Q_Q(DFileWatcher);
    Q_EMIT q->fileDeleted(QUrl::fromLocalFile(path));
    qCDebug(logFilesystem) << "Emitted fileDeleted signal for:" << path;
}

void DFileWatcherPrivate::_q_handleFileAttributeChanged(const QString &path, const QString &parentPath)
{
    qCDebug(logFilesystem) << "Handling file attribute changed: path=" << path << "parentPath=" << parentPath << "this->path=" << this->path;
    if (path != this->path && parentPath != this->path) {
        qCDebug(logFilesystem) << "Ignoring file attribute changed event - not relevant to watched path";
        return;
    }

    Q_Q(DFileWatcher);
    Q_EMIT q->fileAttributeChanged(QUrl::fromLocalFile(path));
    qCDebug(logFilesystem) << "Emitted fileAttributeChanged signal for:" << path;
}

void DFileWatcherPrivate::_q_handleFileMoved(const QString &from, const QString &fromParent, const QString &to, const QString &toParent)
{
    qCDebug(logFilesystem) << "Handling file moved: from=" << from << "fromParent=" << fromParent << "to=" << to << "toParent=" << toParent << "this->path=" << this->path;
    Q_Q(DFileWatcher);

    if ((fromParent == this->path && toParent == this->path) || from == this->path) {
        qCDebug(logFilesystem) << "File moved within watched directory";
        Q_EMIT q->fileMoved(QUrl::fromLocalFile(from), QUrl::fromLocalFile(to));
    } else if (fromParent == this->path) {
        qCDebug(logFilesystem) << "File moved out of watched directory";
        Q_EMIT q->fileDeleted(QUrl::fromLocalFile(from));
    } else if (watchFileList.contains(from)) {
        qCDebug(logFilesystem) << "File moved from watched list";
        Q_EMIT q->fileDeleted(url);
    } else if (toParent == this->path) {
        qCDebug(logFilesystem) << "File moved into watched directory";
        Q_EMIT q->subfileCreated(QUrl::fromLocalFile(to));
    } else {
        qCDebug(logFilesystem) << "File move event not relevant to watched path";
    }
}

void DFileWatcherPrivate::_q_handleFileCreated(const QString &path, const QString &parentPath)
{
    qCDebug(logFilesystem) << "Handling file created: path=" << path << "parentPath=" << parentPath << "this->path=" << this->path;
    if (path != this->path && parentPath != this->path) {
        qCDebug(logFilesystem) << "Ignoring file created event - not relevant to watched path";
        return;
    }

    Q_Q(DFileWatcher);
    Q_EMIT q->subfileCreated(QUrl::fromLocalFile(path));
    qCDebug(logFilesystem) << "Emitted subfileCreated signal for:" << path;
}

void DFileWatcherPrivate::_q_handleFileModified(const QString &path, const QString &parentPath)
{
    qCDebug(logFilesystem) << "Handling file modified: path=" << path << "parentPath=" << parentPath << "this->path=" << this->path;
    if (path != this->path && parentPath != this->path) {
        qCDebug(logFilesystem) << "Ignoring file modified event - not relevant to watched path";
        return;
    }

    Q_Q(DFileWatcher);
    Q_EMIT q->fileModified(QUrl::fromLocalFile(path));
    qCDebug(logFilesystem) << "Emitted fileModified signal for:" << path;
}

void DFileWatcherPrivate::_q_handleFileClose(const QString &path, const QString &parentPath)
{
    qCDebug(logFilesystem, "Handling file closed: path=%s, parentPath=%s, this->path=%s", 
            qPrintable(path), qPrintable(parentPath), qPrintable(this->path));
    if (path != this->path && parentPath != this->path) {
        qCDebug(logFilesystem, "Ignoring file closed event - not relevant to watched path");
        return;
    }

    Q_Q(DFileWatcher);
    Q_EMIT q->fileClosed(QUrl::fromLocalFile(path));
    qCDebug(logFilesystem, "Emitted fileClosed signal for: %s", qPrintable(path));
}

QString DFileWatcherPrivate::formatPath(const QString &path)
{
    qCDebug(logFilesystem, "Formatting path: %s", qPrintable(path));
    QString p = QFileInfo(path).absoluteFilePath();

    if (p.endsWith(QDir::separator())) {
        qCDebug(logFilesystem, "Removing trailing separator from path");
        p.chop(1);
    }

    QString result = p.isEmpty() ? path : p;
    qCDebug(logFilesystem, "Formatted path result: %s", qPrintable(result));
    return result;
}

/*!
@~english
    \class Dtk::Core::DFileWatcher
    \inmodule dtkcore

    \brief The DFileWatcher class provides an implementation of DBaseFileWatcher for monitoring files and directories for modifications.
*/

DFileWatcher::DFileWatcher(const QString &filePath, QObject *parent)
    : DBaseFileWatcher(*new DFileWatcherPrivate(this), QUrl::fromLocalFile(filePath), parent)
{
    qCDebug(logFilesystem, "DFileWatcher created for path: %s", qPrintable(filePath));
    d_func()->path = DFileWatcherPrivate::formatPath(filePath);
}

void DFileWatcher::onFileDeleted(const QString &path, const QString &name)
{
    qCDebug(logFilesystem, "onFileDeleted called: path=%s, name=%s", qPrintable(path), qPrintable(name));
    if (name.isEmpty()) {
        qCDebug(logFilesystem, "Handling file deleted with empty name");
        d_func()->_q_handleFileDeleted(path, QString());
    } else {
        QString fullPath = joinFilePath(path, name);
        qCDebug(logFilesystem, "Handling file deleted with name, full path: %s", qPrintable(fullPath));
        d_func()->_q_handleFileDeleted(fullPath, path);
    }
}

void DFileWatcher::onFileAttributeChanged(const QString &path, const QString &name)
{
    qCDebug(logFilesystem, "onFileAttributeChanged called: path=%s, name=%s", qPrintable(path), qPrintable(name));
    if (name.isEmpty()) {
        qCDebug(logFilesystem, "Handling file attribute changed with empty name");
        d_func()->_q_handleFileAttributeChanged(path, QString());
    } else {
        QString fullPath = joinFilePath(path, name);
        qCDebug(logFilesystem, "Handling file attribute changed with name, full path: %s", qPrintable(fullPath));
        d_func()->_q_handleFileAttributeChanged(fullPath, path);
    }
}

void DFileWatcher::onFileMoved(const QString &from, const QString &fname, const QString &to, const QString &tname)
{
    qCDebug(logFilesystem, "onFileMoved called: from=%s, fname=%s, to=%s, tname=%s", 
            qPrintable(from), qPrintable(fname), qPrintable(to), qPrintable(tname));
    QString fromPath, fpPath;
    QString toPath, tpPath;

    if (fname.isEmpty()) {
        fromPath = from;
        qCDebug(logFilesystem, "From path is direct: %s", qPrintable(fromPath));
    } else {
        fromPath = joinFilePath(from, fname);
        fpPath = from;
        qCDebug(logFilesystem, "From path with name: %s, parent: %s", qPrintable(fromPath), qPrintable(fpPath));
    }

    if (tname.isEmpty()) {
        toPath = to;
        qCDebug(logFilesystem, "To path is direct: %s", qPrintable(toPath));
    } else {
        toPath = joinFilePath(to, tname);
        tpPath = to;
        qCDebug(logFilesystem, "To path with name: %s, parent: %s", qPrintable(toPath), qPrintable(tpPath));
    }

    d_func()->_q_handleFileMoved(fromPath, fpPath, toPath, tpPath);
}

void DFileWatcher::onFileCreated(const QString &path, const QString &name)
{
    qCDebug(logFilesystem, "onFileCreated called: path=%s, name=%s", qPrintable(path), qPrintable(name));
    QString fullPath = joinFilePath(path, name);
    qCDebug(logFilesystem, "Handling file created, full path: %s", qPrintable(fullPath));
    d_func()->_q_handleFileCreated(fullPath, path);
}

void DFileWatcher::onFileModified(const QString &path, const QString &name)
{
    qCDebug(logFilesystem, "onFileModified called: path=%s, name=%s", qPrintable(path), qPrintable(name));
    if (name.isEmpty()) {
        qCDebug(logFilesystem, "Handling file modified with empty name");
        d_func()->_q_handleFileModified(path, QString());
    } else {
        QString fullPath = joinFilePath(path, name);
        qCDebug(logFilesystem, "Handling file modified with name, full path: %s", qPrintable(fullPath));
        d_func()->_q_handleFileModified(fullPath, path);
    }
}

void DFileWatcher::onFileClosed(const QString &path, const QString &name)
{
    qCDebug(logFilesystem, "onFileClosed called: path=%s, name=%s", qPrintable(path), qPrintable(name));
    if (name.isEmpty()) {
        qCDebug(logFilesystem, "Handling file closed with empty name");
        d_func()->_q_handleFileClose(path, QString());
    } else {
        QString fullPath = joinFilePath(path, name);
        qCDebug(logFilesystem, "Handling file closed with name, full path: %s", qPrintable(fullPath));
        d_func()->_q_handleFileClose(fullPath, path);
    }
}

DCORE_END_NAMESPACE

#include "moc_dfilewatcher.cpp"
