// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dcapmanager.h"
#include "dobject_p.h"
#include "dstandardpaths.h"
#include "private/dcapfsfileengine_p.h"

#include <QStandardPaths>
#include <QDebug>
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(logFilesystem)

QString _d_cleanPath(const QString &path) {
    qCDebug(logFilesystem, "Cleaning path: %s", qPrintable(path));
    QString result = path.size() < 2 || !path.endsWith(QDir::separator()) ? path : path.chopped(1);
    qCDebug(logFilesystem, "Cleaned path result: %s", qPrintable(result));
    return result;
}

bool _d_isSubFileOf(const QString &filePath, const QString &directoryPath)
{
    qCDebug(logFilesystem, "Checking if file is subfile: %s, directory: %s", qPrintable(filePath), qPrintable(directoryPath));
    QString path = _d_cleanPath(filePath);
    bool ret = path.startsWith(directoryPath);
    qCDebug(logFilesystem, "Is subfile result: %s", ret ? "true" : "false");
    return ret;
}

static QStringList defaultWriteablePaths() {
    qCDebug(logFilesystem, "Getting default writable paths");
    QStringList paths;
    int list[] = {QStandardPaths::AppConfigLocation,
                  QStandardPaths::AppDataLocation,
                  QStandardPaths::CacheLocation,
                  QStandardPaths::TempLocation,
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                  QStandardPaths::DataLocation,
#endif
                  QStandardPaths::GenericConfigLocation,
                  QStandardPaths::HomeLocation,
                  QStandardPaths::MusicLocation,
                  QStandardPaths::DocumentsLocation,
                  QStandardPaths::MoviesLocation,
                  QStandardPaths::PicturesLocation,
                  QStandardPaths::DownloadLocation};

    for (uint i = 0; i < sizeof (list) / sizeof (int); ++i) {
        const QString &path  = QStandardPaths::writableLocation(QStandardPaths::StandardLocation(list[i]));
        if (path.isEmpty())
            continue;

        qCDebug(logFilesystem, "Adding standard path: %s", qPrintable(path));
        paths.append(path);
    }

    for (int i = 0; i <= static_cast<int>(DStandardPaths::XDG::RuntimeDir); ++i) {
        const QString &path = DStandardPaths::path(DStandardPaths::XDG(i));
        if (path.isEmpty())
            continue;

        qCDebug(logFilesystem, "Adding XDG path: %s", qPrintable(path));
        paths.append(path);
    }

    for (int i = 0; i <= static_cast<int>(DStandardPaths::DSG::DataDir); ++i) {
        const QStringList &pathList = DStandardPaths::paths(DStandardPaths::DSG(i));
        if (pathList.isEmpty())
            continue;

        for (auto path : pathList) {
            if (path.isEmpty() || paths.contains(path))
                continue;

            qCDebug(logFilesystem, "Adding DSG path: %s", qPrintable(path));
            paths.append(path);
        }
    }
    qCDebug(logFilesystem, "Total writable paths: %d", paths.size());
    return paths;
}

class DCapManagerPrivate : public DObjectPrivate
{
    D_DECLARE_PUBLIC(DCapManager)
public:
    DCapManagerPrivate(DCapManager *qq);

    QStringList pathList;
};

class DCapManager_ : public DCapManager {};
Q_GLOBAL_STATIC(DCapManager_, capManager)

DCapManagerPrivate::DCapManagerPrivate(DCapManager *qq)
    : DObjectPrivate(qq)
{
    qCDebug(logFilesystem, "DCapManagerPrivate created");
    pathList = defaultWriteablePaths();
    qCDebug(logFilesystem, "Initialized with %d paths", pathList.size());
}

DCapManager::DCapManager()
    : DObject(*new DCapManagerPrivate(this))
{
    qCDebug(logFilesystem, "DCapManager created");
}

DCapManager *DCapManager::instance()
{
    qCDebug(logFilesystem, "Getting DCapManager instance");
    return capManager;
}

#if DTK_VERSION < DTK_VERSION_CHECK(6, 0, 0, 0)
void DCapManager::registerFileEngine()
{
}

void DCapManager::unregisterFileEngine()
{
}
#endif

void DCapManager::appendPath(const QString &path)
{
    qCDebug(logFilesystem, "DCapManager appendPath called with: %s", qPrintable(path));
    D_D(DCapManager);
    const QString &targetPath = _d_cleanPath(path);
    bool exist = std::any_of(d->pathList.cbegin(), d->pathList.cend(),
                             std::bind(_d_isSubFileOf, targetPath, std::placeholders::_1));
    if (exist) {
        qCDebug(logFilesystem, "DCapManager path already exists, skipping");
        return;
    }
    d->pathList.append(targetPath);
    qCDebug(logFilesystem, "DCapManager path added successfully");
}

void DCapManager::appendPaths(const QStringList &pathList)
{
    qCDebug(logFilesystem, "DCapManager appendPaths called with %d paths", pathList.size());
    for (auto path : pathList)
        appendPath(path);
    qCDebug(logFilesystem, "DCapManager appendPaths completed");
}

void DCapManager::removePath(const QString &path)
{
    qCDebug(logFilesystem, "DCapManager removePath called with: %s", qPrintable(path));
    D_D(DCapManager);
    const QString &targetPath = _d_cleanPath(path);
    if (!d->pathList.contains(targetPath)) {
        qCDebug(logFilesystem, "DCapManager path not found, skipping removal");
        return;
    }
    d->pathList.removeOne(targetPath);
    qCDebug(logFilesystem, "DCapManager path removed successfully");
}

void DCapManager::removePaths(const QStringList &paths)
{
    qCDebug(logFilesystem, "DCapManager removePaths called with %d paths", paths.size());
    for (auto path : paths)
        removePath(path);
    qCDebug(logFilesystem, "DCapManager removePaths completed");
}

QStringList DCapManager::paths() const
{
    qCDebug(logFilesystem, "DCapManager paths called");
    D_DC(DCapManager);
    qCDebug(logFilesystem, "DCapManager paths returning %d paths", d->pathList.size());
    return d->pathList;
}

DCORE_END_NAMESPACE
