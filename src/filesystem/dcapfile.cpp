// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dcapfile.h"
#include "dobject_p.h"
#include "private/dcapfsfileengine_p.h"

#include <private/qdir_p.h>
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(logFilesystem)

extern QString _d_cleanPath(const QString &path);
extern bool _d_isSubFileOf(const QString &filePath, const QString &directoryPath);

class DCapFilePrivate : public DObjectPrivate
{
    D_DECLARE_PUBLIC(DCapFile)
public:
    DCapFilePrivate(DCapFile *qq, const QString &fileName = QString());
    ~DCapFilePrivate() override;
    static bool canReadWrite(const QString &path);

    QString fileName;
};

DCapFilePrivate::DCapFilePrivate(DCapFile *qq, const QString &fileName)
    : DObjectPrivate(qq)
    , fileName(fileName)
{
    qCDebug(logFilesystem, "DCapFilePrivate created with fileName: %s", qPrintable(fileName));
}

DCapFilePrivate::~DCapFilePrivate()
{
    qCDebug(logFilesystem, "DCapFilePrivate destructor called for fileName: %s", qPrintable(fileName));
}

bool DCapFilePrivate::canReadWrite(const QString &path)
{
    qCDebug(logFilesystem, "Checking canReadWrite for path: %s", qPrintable(path));
    DCapFSFileEngine engine(path);
    bool result = engine.canReadWrite(path);
    qCDebug(logFilesystem, "canReadWrite result: %s", result ? "true" : "false");
    return result;
}

DCapFile::DCapFile(QObject *parent)
    : QFile(parent)
    , DObject(*new DCapFilePrivate(this))
{
    qCDebug(logFilesystem, "DCapFile created with parent");
}

DCapFile::DCapFile(const QString &name, QObject *parent)
    : QFile(name, parent)
    , DObject(*new DCapFilePrivate(this, name))
{
    qCDebug(logFilesystem, "DCapFile created with name: %s", qPrintable(name));
}

DCapFile::~DCapFile()
{
    qCDebug(logFilesystem, "DCapFile destroyed");
}

void DCapFile::setFileName(const QString &name)
{
    qCDebug(logFilesystem, "Setting fileName to: %s", qPrintable(name));
    D_D(DCapFile);
    d->fileName = name;
    return QFile::setFileName(name);
}

bool DCapFile::exists() const
{
    D_DC(DCapFile);
    qCDebug(logFilesystem, "Checking if file exists: %s", qPrintable(d->fileName));
    
    if (!d->canReadWrite(d->fileName)) {
        qCWarning(logFilesystem, "Cannot read/write file: %s", qPrintable(d->fileName));
        return false;
    }

    bool result = QFile::exists();
    qCDebug(logFilesystem, "File exists result: %s", result ? "true" : "false");
    return result;
}

bool DCapFile::exists(const QString &fileName)
{
    qCDebug(logFilesystem, "Static exists check for: %s", qPrintable(fileName));
    return DCapFile(fileName).exists();
}

#if DTK_VERSION < DTK_VERSION_CHECK(6, 0, 0, 0)
QString DCapFile::readLink() const
{
    D_DC(DCapFile);
    qCDebug(logFilesystem, "Reading symlink for: %s", qPrintable(d->fileName));
    
    if (!d->canReadWrite(d->fileName)) {
        qCWarning(logFilesystem, "Cannot read/write symlink: %s", qPrintable(d->fileName));
        return QString();
    }

    QString result = QFile::readLink();
    qCDebug(logFilesystem, "Symlink read result: %s", qPrintable(result));
    return result;
}
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
QString DCapFile::symLinkTarget() const
{
    D_DC(DCapFile);
    qCDebug(logFilesystem, "Getting symlink target for: %s", qPrintable(d->fileName));

    QString result = QFile::symLinkTarget();
    qCDebug(logFilesystem, "Symlink target result: %s", qPrintable(result));
    return result;
}
#endif

bool DCapFile::remove()
{
    D_D(DCapFile);
    qCDebug(logFilesystem, "Removing file: %s", qPrintable(d->fileName));
    
    if (!d->canReadWrite(d->fileName)) {
        qCWarning(logFilesystem, "Cannot read/write file for removal: %s", qPrintable(d->fileName));
        return false;
    }

    bool result = QFile::remove();
    qCDebug(logFilesystem, "File removal result: %s", result ? "true" : "false");
    return result;
}

bool DCapFile::remove(const QString &fileName)
{
    qCDebug(logFilesystem, "Static remove for: %s", qPrintable(fileName));
    return DCapFile(fileName).remove();
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
bool DCapFile::moveToTrash()
{
    D_D(DCapFile);
    qCDebug(logFilesystem, "DCapFile moveToTrash called for: %s", qPrintable(d->fileName));
    
    if (!d->canReadWrite(d->fileName)) {
        qCWarning(logFilesystem, "Cannot read/write file for move to trash: %s", qPrintable(d->fileName));
        return false;
    }

    bool result = QFile::moveToTrash();
    qCDebug(logFilesystem, "DCapFile moveToTrash result: %s", result ? "true" : "false");
    return result;
}

bool DCapFile::moveToTrash(const QString &fileName, QString *pathInTrash)
{
    qCDebug(logFilesystem, "DCapFile static moveToTrash called for: %s", qPrintable(fileName));
    DCapFile file(fileName);
    if (file.moveToTrash()) {
        if (pathInTrash) {
            *pathInTrash = file.fileName();
            qCDebug(logFilesystem, "DCapFile moveToTrash path in trash: %s", qPrintable(*pathInTrash));
        }
        qCDebug(logFilesystem, "DCapFile moveToTrash completed successfully");
        return true;
    }
    qCDebug(logFilesystem, "DCapFile moveToTrash failed");
    return false;
}
#endif

bool DCapFile::rename(const QString &newName)
{
    D_D(DCapFile);
    qCDebug(logFilesystem, "DCapFile rename called from %s to %s", qPrintable(d->fileName), qPrintable(newName));
    
    if (!d->canReadWrite(newName)) {
        qCWarning(logFilesystem, "Cannot read/write destination for rename: %s", qPrintable(newName));
        return false;
    }

    bool result = QFile::rename(newName);
    qCDebug(logFilesystem, "DCapFile rename result: %s", result ? "true" : "false");
    return result;
}

bool DCapFile::rename(const QString &oldName, const QString &newName)
{
    qCDebug(logFilesystem, "DCapFile static rename called from %s to %s", qPrintable(oldName), qPrintable(newName));
    
    if (!DCapFilePrivate::canReadWrite(oldName)) {
        qCWarning(logFilesystem, "Cannot read/write source for static rename: %s", qPrintable(oldName));
        return false;
    }

    bool result = DCapFile(oldName).rename(newName);
    qCDebug(logFilesystem, "DCapFile static rename result: %s", result ? "true" : "false");
    return result;
}

bool DCapFile::link(const QString &newName)
{
    D_D(DCapFile);
    qCDebug(logFilesystem, "DCapFile link called from %s to %s", qPrintable(d->fileName), qPrintable(newName));
    
    if (!d->canReadWrite(newName)) {
        qCWarning(logFilesystem, "Cannot read/write destination for link: %s", qPrintable(newName));
        return false;
    }

    bool result = QFile::link(newName);
    qCDebug(logFilesystem, "DCapFile link result: %s", result ? "true" : "false");
    return result;
}

bool DCapFile::link(const QString &oldName, const QString &newName)
{
    qCDebug(logFilesystem, "DCapFile static link called from %s to %s", qPrintable(oldName), qPrintable(newName));
    
    if (!DCapFilePrivate::canReadWrite(oldName)) {
        qCWarning(logFilesystem, "Cannot read/write source for static link: %s", qPrintable(oldName));
        return false;
    }

    bool result = DCapFile(oldName).link(newName);
    qCDebug(logFilesystem, "DCapFile static link result: %s", result ? "true" : "false");
    return result;
}

bool DCapFile::copy(const QString &newName)
{
    D_D(DCapFile);
    qCDebug(logFilesystem, "Copying file from %s to %s", qPrintable(d->fileName), qPrintable(newName));
    
    if (!d->canReadWrite(d->fileName)) {
        qCWarning(logFilesystem, "Cannot read/write source file: %s", qPrintable(d->fileName));
        return false;
    }

    if (!d->canReadWrite(newName)) {
        qCWarning(logFilesystem, "Cannot read/write destination file: %s", qPrintable(newName));
        return false;
    }

    bool result = QFile::copy(newName);
    qCDebug(logFilesystem, "File copy result: %s", result ? "true" : "false");
    return result;
}

bool DCapFile::copy(const QString &fileName, const QString &newName)
{
    qCDebug(logFilesystem, "Static copy from %s to %s", qPrintable(fileName), qPrintable(newName));
    return DCapFile(fileName).copy(newName);
}

bool DCapFile::open(QIODevice::OpenMode flags)
{
    D_D(DCapFile);
    qCDebug(logFilesystem, "DCapFile open called for: %s with flags: %d", qPrintable(d->fileName), static_cast<int>(flags));
    
    if (!d->canReadWrite(d->fileName)) {
        qCWarning(logFilesystem, "Cannot read/write file for opening: %s", qPrintable(d->fileName));
        return false;
    }

    bool result = QFile::open(flags);
    qCDebug(logFilesystem, "DCapFile open result: %s", result ? "true" : "false");
    return result;
}

bool DCapFile::resize(qint64 sz)
{
    D_D(DCapFile);
    qCDebug(logFilesystem, "DCapFile resize called for: %s to size: %lld", qPrintable(d->fileName), sz);
    
    if (!d->canReadWrite(d->fileName)) {
        qCWarning(logFilesystem, "Cannot read/write file for resizing: %s", qPrintable(d->fileName));
        return false;
    }

    bool result = QFile::resize(sz);
    qCDebug(logFilesystem, "DCapFile resize result: %s", result ? "true" : "false");
    return result;
}

bool DCapFile::resize(const QString &fileName, qint64 sz)
{
    qCDebug(logFilesystem, "DCapFile static resize called for: %s to size: %lld", qPrintable(fileName), sz);
    bool result = DCapFile(fileName).resize(sz);
    qCDebug(logFilesystem, "DCapFile static resize result: %s", result ? "true" : "false");
    return result;
}

bool DCapFile::open(FILE *, QIODevice::OpenMode, QFileDevice::FileHandleFlags)
{
    qCDebug(logFilesystem, "DCapFile open with FILE* called - not supported");
    return false;
}

bool DCapFile::open(int, QIODevice::OpenMode, QFileDevice::FileHandleFlags)
{
    qCDebug(logFilesystem, "DCapFile open with int called - not supported");
    return false;
}

class DCapDirPrivate  : public QSharedData
{
public:
    DCapDirPrivate(QString filePath);
    explicit DCapDirPrivate(const DCapDirPrivate &copy);

    QString filePath;
};

DCapDirPrivate::DCapDirPrivate(QString filePath)
    : filePath(filePath)
{
    qCDebug(logFilesystem, "DCapDirPrivate created with filePath: %s", qPrintable(filePath));
}

DCapDirPrivate::DCapDirPrivate(const DCapDirPrivate &copy)
    : QSharedData(copy)
    , filePath(copy.filePath)
{
    qCDebug(logFilesystem, "DCapDirPrivate copy constructor called");
}

DCapDir::DCapDir(const DCapDir &dir)
    : QDir(dir)
    , dd_ptr(dir.dd_ptr)
{
    qCDebug(logFilesystem, "DCapDir copy constructor called");
}

DCapDir::DCapDir(const QString &path)
    : QDir(path)
    , dd_ptr(new DCapDirPrivate(path))
{
    qCDebug(logFilesystem, "DCapDir created with path: %s", qPrintable(path));
}

DCapDir::DCapDir(const QString &path, const QString &nameFilter,
                 QDir::SortFlags sort, QDir::Filters filter)
    : QDir(path, nameFilter, sort, filter)
    , dd_ptr(new DCapDirPrivate(path))
{
    qCDebug(logFilesystem, "DCapDir created with path: %s, nameFilter: %s", qPrintable(path), qPrintable(nameFilter));
}

DCapDir::~DCapDir()
{
    qCDebug(logFilesystem, "DCapDir destructor called");
}

void DCapDir::setPath(const QString &path)
{
    qCDebug(logFilesystem, "DCapDir setPath called with: %s", qPrintable(path));
    dd_ptr = new DCapDirPrivate(path);
    return QDir::setPath(path);
}

bool DCapDir::cd(const QString &dirName)
{
    qCDebug(logFilesystem, "DCapDir cd called with: %s", qPrintable(dirName));
    auto old_d = d_ptr;
    bool ret = QDir::cd(dirName);
    if (!ret) {
        qCDebug(logFilesystem, "DCapDir cd failed");
        return ret;
    }

    // take the new path.
    auto path = QDir::filePath("");
    qCDebug(logFilesystem, "DCapDir new path: %s", qPrintable(path));
    QScopedPointer<DCapFSFileEngine> fsEngine(new DCapFSFileEngine(path));
    if (fsEngine->fileFlags(QAbstractFileEngine::FlagsMask) & QAbstractFileEngine::ExistsFlag) {
        qCDebug(logFilesystem, "DCapDir path exists, updating dd_ptr");
        dd_ptr = new DCapDirPrivate(path);
        return true;
    }
    qCDebug(logFilesystem, "DCapDir path does not exist, restoring old d_ptr");
    d_ptr = old_d;
    return false;
}

QStringList DCapDir::entryList(DCapDir::Filters filters, DCapDir::SortFlags sort) const
{
    qCDebug(logFilesystem, "DCapDir entryList called with filters: %d, sort: %d", filters, sort);
    const QDirPrivate* d = d_ptr.constData();
    return entryList(d->nameFilters, filters, sort);
}

QStringList DCapDir::entryList(const QStringList &nameFilters, DCapDir::Filters filters, DCapDir::SortFlags sort) const
{
    qCDebug(logFilesystem, "DCapDir entryList called with %d nameFilters, filters: %d, sort: %d", nameFilters.size(), filters, sort);
    if (!DCapFilePrivate::canReadWrite(dd_ptr->filePath)) {
        qCWarning(logFilesystem, "Cannot read/write directory: %s", qPrintable(dd_ptr->filePath));
        return {};
    }
    QStringList result = QDir::entryList(nameFilters, filters, sort);
    qCDebug(logFilesystem, "DCapDir entryList result: %d entries", result.size());
    return result;
}

QFileInfoList DCapDir::entryInfoList(QDir::Filters filters, QDir::SortFlags sort) const
{
    qCDebug(logFilesystem, "DCapDir entryInfoList called with filters: %d, sort: %d", filters, sort);
    const QDirPrivate* d = d_ptr.constData();
    return entryInfoList(d->nameFilters, filters, sort);
}

QFileInfoList DCapDir::entryInfoList(const QStringList &nameFilters, DCapDir::Filters filters, DCapDir::SortFlags sort) const
{
    qCDebug(logFilesystem, "DCapDir entryInfoList called with %d nameFilters, filters: %d, sort: %d", nameFilters.size(), filters, sort);
    if (!DCapFilePrivate::canReadWrite(dd_ptr->filePath)) {
        qCWarning(logFilesystem, "Cannot read/write directory: %s", qPrintable(dd_ptr->filePath));
        return {};
    }
    QFileInfoList result = QDir::entryInfoList(nameFilters, filters, sort);
    qCDebug(logFilesystem, "DCapDir entryInfoList result: %d entries", result.size());
    return result;
}

bool DCapDir::mkdir(const QString &dirName) const
{
    qCDebug(logFilesystem, "DCapDir mkdir called with: %s", qPrintable(dirName));
    QString fn = filePath(dirName);
    if (!DCapFilePrivate::canReadWrite(fn)) {
        qCWarning(logFilesystem, "Cannot read/write directory for mkdir: %s", qPrintable(fn));
        return false;
    }

    bool result = QDir::mkdir(dirName);
    qCDebug(logFilesystem, "DCapDir mkdir result: %s", result ? "true" : "false");
    return result;
}

bool DCapDir::rmdir(const QString &dirName) const
{
    qCDebug(logFilesystem, "DCapDir rmdir called with: %s", qPrintable(dirName));
    QString fn = filePath(dirName);
    if (!DCapFilePrivate::canReadWrite(fn)) {
        qCWarning(logFilesystem, "Cannot read/write directory for rmdir: %s", qPrintable(fn));
        return false;
    }

    bool result = QDir::rmdir(dirName);
    qCDebug(logFilesystem, "DCapDir rmdir result: %s", result ? "true" : "false");
    return result;
}

bool DCapDir::mkpath(const QString &dirPath) const
{
    qCDebug(logFilesystem, "DCapDir mkpath called with: %s", qPrintable(dirPath));
    QString fn = filePath(dirPath);
    if (!DCapFilePrivate::canReadWrite(fn)) {
        qCWarning(logFilesystem, "Cannot read/write directory for mkpath: %s", qPrintable(fn));
        return false;
    }

    bool result = QDir::mkpath(dirPath);
    qCDebug(logFilesystem, "DCapDir mkpath result: %s", result ? "true" : "false");
    return result;
}

bool DCapDir::rmpath(const QString &dirPath) const
{
    qCDebug(logFilesystem, "DCapDir rmpath called with: %s", qPrintable(dirPath));
    QString fn = filePath(dirPath);
    if (!DCapFilePrivate::canReadWrite(fn)) {
        qCWarning(logFilesystem, "Cannot read/write directory for rmpath: %s", qPrintable(fn));
        return false;
    }

    bool result = QDir::rmpath(dirPath);
    qCDebug(logFilesystem, "DCapDir rmpath result: %s", result ? "true" : "false");
    return result;
}

bool DCapDir::exists() const
{
    qCDebug(logFilesystem, "DCapDir exists called");
    if (!DCapFilePrivate::canReadWrite(dd_ptr->filePath)) {
        qCWarning(logFilesystem, "Cannot read/write directory: %s", qPrintable(dd_ptr->filePath));
        return false;
    }

    bool result = QDir::exists();
    qCDebug(logFilesystem, "DCapDir exists result: %s", result ? "true" : "false");
    return result;
}

bool DCapDir::exists(const QString &name) const
{
    qCDebug(logFilesystem, "DCapDir exists called with name: %s", qPrintable(name));
    if (name.isEmpty()) {
        qWarning("DCapFile::exists: Empty or null file name");
        return false;
    }
    bool result = DCapFile::exists(filePath(name));
    qCDebug(logFilesystem, "DCapDir exists result: %s", result ? "true" : "false");
    return result;
}

bool DCapDir::remove(const QString &fileName)
{
    qCDebug(logFilesystem, "DCapDir remove called with: %s", qPrintable(fileName));
    if (fileName.isEmpty()) {
        qWarning("DCapDir::remove: Empty or null file name");
        return false;
    }
    bool result = DCapFile::remove(filePath(fileName));
    qCDebug(logFilesystem, "DCapDir remove result: %s", result ? "true" : "false");
    return result;
}

bool DCapDir::rename(const QString &oldName, const QString &newName)
{
    qCDebug(logFilesystem, "DCapDir rename called from %s to %s", qPrintable(oldName), qPrintable(newName));
    if (oldName.isEmpty() || newName.isEmpty()) {
        qWarning("DCapDir::rename: Empty or null file name(s)");
        return false;
    }

    DCapFile file(filePath(oldName));
    if (!file.exists()) {
        qCWarning(logFilesystem, "Source file does not exist: %s", qPrintable(filePath(oldName)));
        return false;
    }
    bool result = file.rename(filePath(newName));
    qCDebug(logFilesystem, "DCapDir rename result: %s", result ? "true" : "false");
    return result;
}

DCORE_END_NAMESPACE
