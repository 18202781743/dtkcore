// SPDX-FileCopyrightText: 2017 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dbasefilewatcher.h"
#include "private/dbasefilewatcher_p.h"

#include <QEvent>
#include <QDebug>
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(logFileSystem, "dtk.core.filesystem")

QList<DBaseFileWatcher*> DBaseFileWatcherPrivate::watcherList;
DBaseFileWatcherPrivate::DBaseFileWatcherPrivate(DBaseFileWatcher *qq)
    : DObjectPrivate(qq)
{

}

/*!
@~english
    @class Dtk::Core::DBaseFileWatcher
    @ingroup dtkcore

    @brief The DBaseFileWatcher class provides an interface for monitoring files and directories for modifications.
*/

DBaseFileWatcher::~DBaseFileWatcher()
{
    qCDebug(logFileSystem, "DBaseFileWatcher destructor called for URL: %s", qPrintable(fileUrl().toString()));
    stopWatcher();
    DBaseFileWatcherPrivate::watcherList.removeOne(this);
}

QUrl DBaseFileWatcher::fileUrl() const
{
    Q_D(const DBaseFileWatcher);

    qCDebug(logFileSystem, "Getting file URL: %s", qPrintable(d->url.toString()));
    return d->url;
}

/*!
@~english
  @brief Let file watcher start watching file changes.
  @sa stopWatcher(), restartWatcher()
 */
bool DBaseFileWatcher::startWatcher()
{
    Q_D(DBaseFileWatcher);

    qCDebug(logFileSystem, "Starting file watcher for URL: %s", qPrintable(d->url.toString()));

    if (d->started) {
        qCDebug(logFileSystem, "File watcher already started");
        return true;
    }

    if (d->start()) {
        d->started = true;
        qCDebug(logFileSystem, "File watcher started successfully");
        return true;
    }

    qCWarning(logFileSystem, "Failed to start file watcher");
    return false;
}

/*!
@~english
  @brief Stop watching file changes.
  @sa startWatcher(), restartWatcher()
 */
bool DBaseFileWatcher::stopWatcher()
{
    Q_D(DBaseFileWatcher);

    qCDebug(logFileSystem, "Stopping file watcher for URL: %s", qPrintable(d->url.toString()));

    if (!d->started) {
        qCDebug(logFileSystem, "File watcher not started");
        return false;
    }

    if (d->stop()) {
        d->started = false;
        qCDebug(logFileSystem, "File watcher stopped successfully");
        return true;
    }

    qCWarning(logFileSystem, "Failed to stop file watcher");
    return false;
}

/*!
@~english
  @brief Stop file watcher and then restart it to watching file changes.
  @sa startWatcher(), stopWatcher()
 */
bool DBaseFileWatcher::restartWatcher()
{
    qCDebug(logFileSystem, "Restarting file watcher for URL: %s", qPrintable(fileUrl().toString()));
    bool ok = stopWatcher();
    return ok && startWatcher();
}

/*!
@~english
  @brief Set enable file watcher for \a subfileUrl or not
  @param[in] subfileUrl The given url
  @param[in] enabled Enable file change watching or not.
 */
void DBaseFileWatcher::setEnabledSubfileWatcher(const QUrl &subfileUrl, bool enabled)
{
    qCDebug(logFileSystem, "Setting subfile watcher enabled: %s, enabled: %s", 
            qPrintable(subfileUrl.toString()), enabled ? "true" : "false");
    Q_UNUSED(subfileUrl)
    Q_UNUSED(enabled)
}

/*!
@~english
  @brief Emit a signal about \a targetUrl got a \a signal with \a arg1
  Example usage:

  @code
  DBaseFileWatcher::ghostSignal(QUrl("bookmark:///"), &DBaseFileWatcher::fileDeleted, QUrl("bookmark:///bookmarkFile1"));
  @endcode

  @return 成功发送返回 true,否则返回 false.
 */
bool DBaseFileWatcher::ghostSignal(const QUrl &targetUrl, DBaseFileWatcher::SignalType1 signal, const QUrl &arg1)
{
    qCDebug(logFileSystem, "Emitting ghost signal for target URL: %s, arg1: %s", 
            qPrintable(targetUrl.toString()), qPrintable(arg1.toString()));

    if (!signal) {
        qCWarning(logFileSystem, "Invalid signal pointer");
        return false;
    }

    bool ok = false;

    for (DBaseFileWatcher *watcher : DBaseFileWatcherPrivate::watcherList) {
        if (watcher->fileUrl() == targetUrl) {
            ok = true;
            qCDebug(logFileSystem, "Found matching watcher, emitting signal");
            (watcher->*signal)(arg1);
        }
    }

    qCDebug(logFileSystem, "Ghost signal emission result: %s", ok ? "success" : "failed");
    return ok;
}

/*!
@~english
  @brief Emit a signal about \a targetUrl got a \a signal with \a arg1 and \a arg2
  Example usage:
  @code
  DBaseFileWatcher::ghostSignal(QUrl("bookmark:///"), &DBaseFileWatcher::fileMoved, QUrl("bookmark:///bookmarkFile1"), QUrl("bookmark:///NewNameFile1"));
  @endcode
 */
bool DBaseFileWatcher::ghostSignal(const QUrl &targetUrl, DBaseFileWatcher::SignalType2 signal, const QUrl &arg1, const QUrl &arg2)
{
    qCDebug(logFileSystem, "Emitting ghost signal for target URL: %s, arg1: %s, arg2: %s", 
            qPrintable(targetUrl.toString()), qPrintable(arg1.toString()), qPrintable(arg2.toString()));

    if (!signal) {
        qCWarning(logFileSystem, "Invalid signal pointer");
        return false;
    }

    bool ok = false;

    for (DBaseFileWatcher *watcher : DBaseFileWatcherPrivate::watcherList) {
        if (watcher->fileUrl() == targetUrl) {
            ok = true;
            qCDebug(logFileSystem, "Found matching watcher, emitting signal");
            (watcher->*signal)(arg1, arg2);
        }
    }

    qCDebug(logFileSystem, "Ghost signal emission result: %s", ok ? "success" : "failed");
    return ok;
}

DBaseFileWatcher::DBaseFileWatcher(DBaseFileWatcherPrivate &dd,
                                           const QUrl &url, QObject *parent)
    : QObject(parent)
    , DObject(dd)
{
    Q_ASSERT(url.isValid());

    qCDebug(logFileSystem, "Creating DBaseFileWatcher for URL: %s", qPrintable(url.toString()));
    d_func()->url = url;
    DBaseFileWatcherPrivate::watcherList << this;
}

DCORE_END_NAMESPACE

//#include "moc_dbasefilewatcher.cpp"
