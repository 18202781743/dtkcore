// SPDX-FileCopyrightText: 2017 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dstandardpaths.h"

#include <QProcessEnvironment>
#include <QLoggingCategory>
#include <unistd.h>
#include <pwd.h>

DCORE_BEGIN_NAMESPACE

#ifdef QT_DEBUG
Q_LOGGING_CATEGORY(logFilesystem, "dtk.core.filesystem")
#else
Q_LOGGING_CATEGORY(logFilesystem, "dtk.core.filesystem", QtInfoMsg)
#endif

class DSnapStandardPathsPrivate
{
public:
    inline  static QString writableLocation(QStandardPaths::StandardLocation /*type*/)
    {
        qCDebug(logFilesystem, "Getting writable location for Snap mode");
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QString result = env.value("SNAP_USER_COMMON");
        qCDebug(logFilesystem, "Snap writable location: %s", qPrintable(result));
        return result;
    }

    inline static QStringList standardLocations(QStandardPaths::StandardLocation type)
    {
        qCDebug(logFilesystem, "Getting standard locations for type: %d", type);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

        switch (type) {
        case QStandardPaths::GenericDataLocation: {
            QString snapRoot = env.value("SNAP");
            QString genericDataDir = snapRoot + PREFIX"/share/";
            qCDebug(logFilesystem, "Snap generic data directory: %s", qPrintable(genericDataDir));
            return QStringList() << genericDataDir;
        }
        default:
            break;
        }

        QString result = env.value("SNAP_USER_COMMON");
        qCDebug(logFilesystem, "Snap standard location: %s", qPrintable(result));
        return QStringList() << result;
    }

private:
    DSnapStandardPathsPrivate();
    ~DSnapStandardPathsPrivate();
    Q_DISABLE_COPY(DSnapStandardPathsPrivate)
};


/*!
  \class Dtk::Core::DStandardPaths
  \inmodule dtkcore
  \brief DStandardPaths提供兼容Snap/Dtk标准的路径模式。DStandardPaths实现了Qt的QStandardPaths主要接口.
  \sa QStandardPaths
 */

/*!
  \enum Dtk::Core::DStandardPaths::Mode
  \brief DStandardPaths支持的路径产生模式。
  \value Auto
  \brief 和Qt标准的行为表现一致。
  \value Snap
  \brief 读取SNAP相关的环境变量，支持将配置存储在SNAP对应目录。
  \value Test
  \brief 和Qt标准的行为表现一致，但是会开启测试模式，参考QStandardPaths::setTestModeEnabled。
 */


static DStandardPaths::Mode s_mode = DStandardPaths::Auto;

QString DStandardPaths::writableLocation(QStandardPaths::StandardLocation type)
{
    qCDebug(logFilesystem, "Getting writable location for type: %d, mode: %d", type, s_mode);
    
    switch (s_mode) {
    case Auto:
    case Test:
        qCDebug(logFilesystem, "Using Qt standard paths");
        return  QStandardPaths::writableLocation(type);
    case Snap:
        qCDebug(logFilesystem, "Using Snap standard paths");
        return DSnapStandardPathsPrivate::writableLocation(type);
    }
    qCWarning(logFilesystem, "Unknown mode, falling back to Qt standard paths");
    return QStandardPaths::writableLocation(type);
}

QStringList DStandardPaths::standardLocations(QStandardPaths::StandardLocation type)
{
    qCDebug(logFilesystem, "Getting standard locations for type: %d, mode: %d", type, s_mode);
    
    switch (s_mode) {
    case Auto:
    case Test:
        qCDebug(logFilesystem, "Using Qt standard locations");
        return  QStandardPaths::standardLocations(type);
    case Snap:
        qCDebug(logFilesystem, "Using Snap standard locations");
        return DSnapStandardPathsPrivate::standardLocations(type);
    }
    qCWarning(logFilesystem, "Unknown mode, falling back to Qt standard locations");
    return  QStandardPaths::standardLocations(type);
}

QString DStandardPaths::locate(QStandardPaths::StandardLocation type, const QString &fileName, QStandardPaths::LocateOptions options)
{
    qCDebug(logFilesystem, "Locating file: %s, type: %d, options: %d", qPrintable(fileName), type, options);
    QString result = QStandardPaths::locate(type, fileName, options);
    qCDebug(logFilesystem, "Located file result: %s", qPrintable(result));
    return result;
}

QStringList DStandardPaths::locateAll(QStandardPaths::StandardLocation type, const QString &fileName, QStandardPaths::LocateOptions options)
{
    qCDebug(logFilesystem, "Locating all files: %s, type: %d, options: %d", qPrintable(fileName), type, options);
    QStringList result = QStandardPaths::locateAll(type, fileName, options);
    qCDebug(logFilesystem, "Located %d files", result.size());
    return result;
}

QString DStandardPaths::findExecutable(const QString &executableName, const QStringList &paths)
{
    return QStandardPaths::findExecutable(executableName, paths);
}

void DStandardPaths::setMode(DStandardPaths::Mode mode)
{
    s_mode = mode;
    QStandardPaths::setTestModeEnabled(mode == Test);
}

// https://gitlabwh.uniontech.com/wuhan/se/deepin-specifications/-/issues/21

QString DStandardPaths::homePath()
{
    const QByteArray &home = qgetenv("HOME");

    if (!home.isEmpty())
        return QString::fromLocal8Bit(home);

    return homePath(getuid());
}

QString DStandardPaths::path(DStandardPaths::XDG type)
{
    qCDebug(logFilesystem, "Getting XDG path for type: %d", type);
    QString result;
    switch (type) {
    case XDG::DataHome: {
        const QByteArray &path = qgetenv("XDG_DATA_HOME");
        if (!path.isEmpty()) {
            result = QString::fromLocal8Bit(path);
            qCDebug(logFilesystem, "Using XDG_DATA_HOME: %s", qPrintable(result));
        } else {
            result = homePath() + QStringLiteral("/.local/share");
            qCDebug(logFilesystem, "Using default data home: %s", qPrintable(result));
        }
        break;
    }
    case XDG::CacheHome: {
        const QByteArray &path = qgetenv("XDG_CACHE_HOME");
        if (!path.isEmpty()) {
            result = QString::fromLocal8Bit(path);
            qCDebug(logFilesystem, "Using XDG_CACHE_HOME: %s", qPrintable(result));
        } else {
            result = homePath() + QStringLiteral("/.cache");
            qCDebug(logFilesystem, "Using default cache home: %s", qPrintable(result));
        }
        break;
    }
    case XDG::ConfigHome: {
        const QByteArray &path = qgetenv("XDG_CONFIG_HOME");
        if (!path.isEmpty()) {
            result = QString::fromLocal8Bit(path);
            qCDebug(logFilesystem, "Using XDG_CONFIG_HOME: %s", qPrintable(result));
        } else {
            result = homePath() + QStringLiteral("/.config");
            qCDebug(logFilesystem, "Using default config home: %s", qPrintable(result));
        }
        break;
    }
    case XDG::RuntimeDir: {
        const QByteArray &path = qgetenv("XDG_RUNTIME_DIR");
        if (!path.isEmpty()) {
            result = QString::fromLocal8Bit(path);
            qCDebug(logFilesystem, "Using XDG_RUNTIME_DIR: %s", qPrintable(result));
        } else {
            result = QStringLiteral("/run/user/") + QString::number(getuid());
            qCDebug(logFilesystem, "Using default runtime dir: %s", qPrintable(result));
        }
        break;
    }
    case XDG::StateHome: {
        const QByteArray &path = qgetenv("XDG_STATE_HOME");
        if (!path.isEmpty()) {
            result = QString::fromLocal8Bit(path);
            qCDebug(logFilesystem, "Using XDG_STATE_HOME: %s", qPrintable(result));
        } else {
#ifdef Q_OS_LINUX
            result = homePath() + QStringLiteral("/.local/state");
            qCDebug(logFilesystem, "Using default state home: %s", qPrintable(result));
#else
            // TODO: handle it on mac
            result = QString();
            qCDebug(logFilesystem, "State home not supported on this platform");
#endif
        }
        break;
    }
    default:
        qCWarning(logFilesystem, "Unknown XDG type: %d", type);
        result = QString();
        break;
    }
    qCDebug(logFilesystem, "XDG path result: %s", qPrintable(result));
    return result;
}

QString DStandardPaths::path(DStandardPaths::DSG type)
{
    qCDebug(logFilesystem, "Getting DSG path for type: %d", type);
    const auto list = paths(type);
    QString result = list.isEmpty() ? nullptr : list.first();
    qCDebug(logFilesystem, "DSG path result: %s", qPrintable(result));
    return result;
}

QStringList DStandardPaths::paths(DSG type)
{
    qCDebug(logFilesystem, "Getting DSG paths for type: %d", type);
    QStringList paths;

    if (type == DSG::DataDir) {
        const QByteArray &path = qgetenv("DSG_DATA_DIRS");
        if (path.isEmpty()) {
            qCDebug(logFilesystem, "DSG_DATA_DIRS not set, using default");
            paths << QLatin1String(PREFIX"/share/dsg");
        } else {
            qCDebug(logFilesystem, "Using DSG_DATA_DIRS: %s", path.constData());
            const auto list = path.split(':');
            paths.reserve(list.size());
            for (const auto &i : list)
                paths.push_back(QString::fromLocal8Bit(i));
        }
    } else if (type == DSG::AppData) {
        const QByteArray &path = qgetenv("DSG_APP_DATA");
        qCDebug(logFilesystem, "Using DSG_APP_DATA: %s", path.constData());
        //TODO 应用数据目录规范:`/persistent/appdata/{appid}`, now `appid` is not captured.
        paths.push_back(QString::fromLocal8Bit(path));
    } else {
        qCWarning(logFilesystem, "Unknown DSG type: %d", type);
    }

    qCDebug(logFilesystem, "DSG paths result: %d paths", paths.size());
    return paths;
}

QString DStandardPaths::filePath(DStandardPaths::XDG type, QString fileName)
{
    qCDebug(logFilesystem, "Getting XDG file path for type: %d, fileName: %s", type, qPrintable(fileName));
    const QString &dir = path(type);

    if (dir.isEmpty()) {
        qCDebug(logFilesystem, "XDG directory is empty, returning empty string");
        return QString();
    }

    QString result = dir + QLatin1Char('/') + fileName;
    qCDebug(logFilesystem, "XDG file path result: %s", qPrintable(result));
    return result;
}

QString DStandardPaths::filePath(DStandardPaths::DSG type, const QString fileName)
{
    qCDebug(logFilesystem, "Getting DSG file path for type: %d, fileName: %s", type, qPrintable(fileName));
    const QString &dir = path(type);

    if (dir.isEmpty()) {
        qCDebug(logFilesystem, "DSG directory is empty, returning empty string");
        return QString();
    }

    QString result = dir + QLatin1Char('/') + fileName;
    qCDebug(logFilesystem, "DSG file path result: %s", qPrintable(result));
    return result;
}

QString DStandardPaths::homePath(const uint uid)
{
    qCDebug(logFilesystem, "Getting home path for UID: %u", uid);
    struct passwd *pw = getpwuid(uid);

    if (!pw) {
        qCWarning(logFilesystem, "Failed to get passwd for UID: %u", uid);
        return QString();
    }

    const char *homedir = pw->pw_dir;
    QString result = QString::fromLocal8Bit(homedir);
    qCDebug(logFilesystem, "Home path result: %s", qPrintable(result));
    return result;
}

DCORE_END_NAMESPACE
