// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <filesystem>  //Avoid changing the access control of the standard library
#endif

#define private public
#define protected public
#include <private/qfile_p.h>
#undef private
#undef protected

#include "ddcifileengine_p.h"
#include "dci/ddcifile.h"

#include <QBuffer>
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

#ifdef QT_DEBUG
Q_LOGGING_CATEGORY(logFE, "dtk.dci.fileengine")
#else
Q_LOGGING_CATEGORY(logFE, "dtk.dci.fileengine", QtInfoMsg)
#endif

#define DCI_FILE_SCHEME "dci:"
#define DCI_FILE_SUFFIX ".dci"

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
std::unique_ptr<QAbstractFileEngine> DDciFileEngineHandler::create(const QString &fileName) const
#else
QAbstractFileEngine *DDciFileEngineHandler::create(const QString &fileName) const
#endif
{
    qCDebug(logFE) << "Creating file engine for:" << fileName;
    if (!fileName.startsWith(QStringLiteral(DCI_FILE_SCHEME))) {
        qCDebug(logFE) << "Not a DCI file scheme, returning nullptr";
        return nullptr;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    std::unique_ptr<DDciFileEngine> engine(new DDciFileEngine(fileName));
#else
    DDciFileEngine *engine = new DDciFileEngine(fileName);
#endif

    if (!engine->isValid()) {
        qCWarning(logFE) << "Created engine is not valid";
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
        delete engine;
#endif
        return nullptr;
    }

    qCDebug(logFE) << "File engine created successfully";
    return engine;
}

// 共享同个线程内的同个 DDciFile
static thread_local QHash<QString, QWeakPointer<DDciFile>> sharedDciFile;
static void doDeleteSharedDciFile(const QString &path, DDciFile *file) {
    qCDebug(logFE) << "Deleting shared DCI file:" << path;
    int count = sharedDciFile.remove(path);
    Q_ASSERT(count > 0);
    delete file;
}

static DDciFileShared getDciFile(const QString &dciFilePath, bool usePath = true)
{
    qCDebug(logFE) << "Getting DCI file:" << dciFilePath << "usePath:" << usePath;
    if (auto shared = sharedDciFile.value(dciFilePath)) {
        qCDebug(logFE) << "Found existing shared DCI file";
        return shared.toStrongRef();
    }

    DDciFileShared shared(usePath ? new DDciFile(dciFilePath) : new DDciFile(),
                          std::bind(doDeleteSharedDciFile, dciFilePath,
                                    std::placeholders::_1));
    sharedDciFile[dciFilePath] = shared.toWeakRef();
    qCDebug(logFE) << "Created new shared DCI file";
    return shared;
}

DDciFileEngineIterator::DDciFileEngineIterator(QDir::Filters filters, const QStringList &nameFilters)
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    : QAbstractFileEngineIterator(nullptr, filters, nameFilters)
#else
    : QAbstractFileEngineIterator(filters, nameFilters)
#endif
{
    qCDebug(logFE) << "DDciFileEngineIterator created with filters:" << filters << "nameFilters:" << nameFilters;
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 1)
DDciFileEngineIterator::DDciFileEngineIterator(QDirListing::IteratorFlags filters, const QStringList &nameFilters)
    : QAbstractFileEngineIterator(nullptr, filters, nameFilters)
{
    qCDebug(logFE) << "DDciFileEngineIterator created with listing filters:" << filters << "nameFilters:" << nameFilters;
}
#endif

#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
QString DDciFileEngineIterator::next()
{
    qCDebug(logFE) << "Getting next file name";
    current = nextValid;
    auto fileName = DDciFileEngineIterator::currentFileName();
    qCDebug(logFE) << "Next file name:" << fileName;
    return fileName;
}

bool DDciFileEngineIterator::hasNext() const
#else
bool DDciFileEngineIterator::advance()
#endif
{
    qCDebug(logFE) << "Checking if iterator has next";
    if (!file) {
        qCDebug(logFE) << "File not initialized, resolving path";
        const auto paths = DDciFileEngine::resolvePath(path());
        if (paths.first.isEmpty()
                || paths.second.isEmpty()) {
            qCDebug(logFE) << "Resolved paths are empty, returning false";
            return false;
        }

        file = getDciFile(paths.first);
        list = file->list(paths.second);
        qCDebug(logFE) << "File initialized, list count:" << list.count();
    }

    for (int i = current + 1; i < list.count(); ++i) {
        // 先检查文件类型
        const auto filters = this->filters();
        const auto fileType = file->type(list.at(i));
        qCDebug(logFE) << "Checking file:" << list.at(i) << "type:" << fileType;
        
        if (fileType == DDciFile::Directory) {
            if (!filters.testFlag(QDir::Files)) {
                qCDebug(logFE) << "Directory filtered out by QDir::Files flag";
                continue;
            }
        } else if (fileType == DDciFile::File) {
            if (!filters.testFlag(QDir::Files)) {
                qCDebug(logFE) << "File filtered out by QDir::Files flag";
                continue;
            }
        } else if (fileType == DDciFile::Symlink) {
            if (filters.testFlag(QDir::NoSymLinks)) {
                qCDebug(logFE) << "Symlink filtered out by QDir::NoSymLinks flag";
                continue;
            }
        } else { // DDciFile::UnknowFile
            qCDebug(logFE) << "Unknown file type, skipping";
            continue;
        }

        // 按名称进行过滤
        if (!nameFilters().isEmpty() && !QDir::match(nameFilters(), list.at(i))) {
            qCDebug(logFE) << "File filtered out by name filters:" << list.at(i);
            continue;
        }

        nextValid = i;
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        current = nextValid;
#endif
        qCDebug(logFE) << "Found next valid file:" << list.at(i);
        return true;
    }

    qCDebug(logFE) << "No more files found";
    return false;
}

QString DDciFileEngineIterator::currentFileName() const
{
    auto fileName = file->name(list.at(current));
    qCDebug(logFE) << "Current file name:" << fileName;
    return fileName;
}

DDciFileEngine::DDciFileEngine(const QString &fullPath)
{
    qCDebug(logFE) << "DDciFileEngine created for path:" << fullPath;
    setFileName(fullPath);
}

DDciFileEngine::~DDciFileEngine()
{
    qCDebug(logFE) << "DDciFileEngine destroyed";
    close();
}

bool DDciFileEngine::isValid() const
{
    auto valid = file && file->isValid();
    qCDebug(logFE) << "File engine valid check:" << valid;
    return valid;
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
bool DDciFileEngine::open(QIODevice::OpenMode openMode, std::optional<QFile::Permissions> permissions)
#else
bool DDciFileEngine::open(QIODevice::OpenMode openMode)
#endif
{
    qCDebug(logFE) << "Opening file with mode:" << openMode;
    if (fileBuffer) {
        qCWarning(logFE) << "File is already opened";
        setError(QFile::OpenError, "The file is opened");
        return false;
    }

    if (!file->isValid()) {
        qCWarning(logFE) << "DCI file is not valid";
        setError(QFile::OpenError, "The DCI file is invalid");
        return false;
    }

    if (file->type(subfilePath) == DDciFile::Directory) {
        qCWarning(logFE) << "Cannot open a directory";
        setError(QFile::OpenError, "Can't open a directory");
        return false;
    }

    if (file->type(subfilePath) == DDciFile::Symlink) {
        if (!file->exists(file->symlinkTarget(subfilePath))) {
            qCWarning(logFE) << "Symlink target does not exist";
            setError(QFile::OpenError, "The symlink target is not existed");
            return false;
        }
    }

    if (openMode & QIODevice::Text) {
        qCWarning(logFE) << "Text mode not supported";
        setError(QFile::OpenError, "Not supported open mode");
        return false;
    }

    if (openMode & QIODevice::NewOnly) {
        if (file->exists(subfilePath)) {
            qCWarning(logFE) << "File already exists";
            setError(QFile::OpenError, "The file is existed");
            return false;
        }
    }

    if ((openMode & QIODevice::ExistingOnly)
            || !(openMode & QIODevice::WriteOnly)) {
        if (!file->exists(subfilePath)) {
            qCWarning(logFE) << "File does not exist";
            setError(QFile::OpenError, "The file is not exists");
            return false;
        }
    }

    // 此时当文件不存在时应当创建它
    if (openMode & QIODevice::WriteOnly) {
        qCDebug(logFE) << "Opening real DCI file for writing";
        realDciFile.setFileName(dciFilePath);
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
        auto success = permissions ? realDciFile.open(openMode, permissions.value()) : realDciFile.open(openMode);
        if (!success) {
            qCWarning(logFE) << "Failed to open real DCI file";
            return false;
        }
#else
        if (!realDciFile.open(openMode)) {
            qCWarning(logFE) << "Failed to open real DCI file";
            return false;
        }
#endif

        // 不存在时尝试新建
        if (!file->exists(subfilePath)
                && !file->writeFile(subfilePath, QByteArray())) {
            qCWarning(logFE) << "Failed to create new file";
            return false;
        }
    }

    // 加载数据
    qCDebug(logFE) << "Loading file data";
    fileData = file->dataRef(subfilePath);
    fileBuffer = new QBuffer(&fileData);
    bool ok = fileBuffer->open(openMode);
    Q_ASSERT(ok);
    if (Q_UNLIKELY(!ok)) {
        qCWarning(logFE) << "Failed to open file buffer";
        delete fileBuffer;
        fileBuffer = nullptr;
        return false;
    }

    qCDebug(logFE) << "File opened successfully";
    return true;
}

bool DDciFileEngine::close()
{
    qCDebug(logFE) << "Closing file";
    if (!fileBuffer) {
        qCDebug(logFE) << "No file buffer to close";
        return false;
    }

    fileBuffer->close();
    delete fileBuffer;
    fileBuffer = nullptr;
    qCDebug(logFE) << "File buffer closed, calling flush";
    
    auto result = flush();
    qCDebug(logFE) << "Close result:" << result;
    return result;
}

bool DDciFileEngine::flushToFile(QFile *target, bool writeFile) const
{
    qCDebug(logFE) << "Flushing to file, writeFile:" << writeFile;
    if (target->isWritable()) {
        if (writeFile && !file->writeFile(subfilePath, fileData, true)) {
            qCWarning(logFE) << "Failed to write file data";
            return false;
        }
        if (!target->resize(0)) {
            qCWarning(logFE) << "Failed to resize target file";
            return false;
        }
        const QByteArray &data = file->toData();
        if (target->write(data) != data.size()) {
            qCWarning(logFE) << "Failed to write data to target file";
            return false;
        }
        qCDebug(logFE) << "File flushed successfully";
        return true;
    }

    qCWarning(logFE) << "Target file is not writable";
    return false;
}

bool DDciFileEngine::flush()
{
    qCDebug(logFE) << "Flushing file engine";
    if (!flushToFile(&realDciFile, true)) {
        qCWarning(logFE) << "Failed to flush to file";
        return false;
    }

    auto result = realDciFile.flush();
    qCDebug(logFE) << "File flush result:" << result;
    return result;
}

bool DDciFileEngine::syncToDisk()
{
    qCDebug(logFE) << "Syncing file to disk";
    if (!flush()) {
        qCWarning(logFE) << "Failed to flush before sync";
        return false;
    }
    auto result = realDciFile.d_func()->engine()->syncToDisk();
    qCDebug(logFE) << "Sync to disk result:" << result;
    return result;
}

qint64 DDciFileEngine::size() const
{
    qCDebug(logFE) << "Getting file size";
    if (fileBuffer) {
        auto size = fileData.size();
        qCDebug(logFE) << "File size from buffer:" << size;
        return size;
    }

    auto size = file->dataRef(subfilePath).size();
    qCDebug(logFE) << "File size from data ref:" << size;
    return size;
}

qint64 DDciFileEngine::pos() const
{
    qCDebug(logFE) << "Getting file position";
    auto position = fileBuffer->pos();
    qCDebug(logFE) << "File position:" << position;
    return position;
}

bool DDciFileEngine::seek(qint64 pos)
{
    qCDebug(logFE) << "Seeking to position:" << pos;
    if (!fileBuffer) {
        qCWarning(logFE) << "No file buffer available for seek";
        return false;
    }
    auto result = fileBuffer->seek(pos);
    qCDebug(logFE) << "Seek result:" << result;
    return result;
}

bool DDciFileEngine::isSequential() const
{
    qCDebug(logFE) << "Checking if file is sequential, returning false";
    return false;
}

bool DDciFileEngine::remove()
{
    qCDebug(logFE) << "Removing file:" << subfilePath;
    if (!file->isValid()) {
        qCWarning(logFE) << "File is not valid for removal";
        return false;
    }
    auto result = file->remove(subfilePath) && forceSave();
    qCDebug(logFE) << "Remove result:" << result;
    return result;
}

bool DDciFileEngine::copy(const QString &newName)
{
    qCDebug(logFE) << "Copying file to:" << newName;
    if (!file->isValid()) {
        qCWarning(logFE) << "File is not valid for copy";
        return false;
    }
    // 解析出新的 dci 内部文件路径
    const auto paths = resolvePath(newName, dciFilePath);
    if (paths.second.isEmpty()) {
        qCWarning(logFE) << "Failed to resolve target path for copy";
        return false;
    }

    auto result = file->copy(subfilePath, paths.second) && forceSave();
    qCDebug(logFE) << "Copy result:" << result;
    return result;
}

bool DDciFileEngine::rename(const QString &newName)
{
    qCDebug(logFE) << "Renaming file to:" << newName;
    if (!file->isValid()) {
        qCWarning(logFE) << "File is not valid for rename";
        return false;
    }
    // 解析出新的 dci 内部文件路径
    const auto paths = resolvePath(newName, dciFilePath);
    if (paths.second.isEmpty()) {
        qCWarning(logFE) << "Failed to resolve target path for rename";
        return false;
    }

    auto result = file->rename(subfilePath, paths.second, false) && forceSave();
    qCDebug(logFE) << "Rename result:" << result;
    return result;
}

bool DDciFileEngine::renameOverwrite(const QString &newName)
{
    qCDebug(logFE) << "Renaming file with overwrite to:" << newName;
    if (!file->isValid()) {
        qCWarning(logFE) << "File is not valid for renameOverwrite";
        return false;
    }
    // 解析出新的 dci 内部文件路径
    const auto paths = resolvePath(newName, dciFilePath);
    if (paths.second.isEmpty()) {
        qCWarning(logFE) << "Failed to resolve target path for renameOverwrite";
        return false;
    }

    auto result = file->rename(subfilePath, paths.second, true) && forceSave();
    qCDebug(logFE) << "RenameOverwrite result:" << result;
    return result;
}

bool DDciFileEngine::link(const QString &newName)
{
    qCDebug(logFE) << "Creating link to:" << newName;
    if (!file->isValid()) {
        qCWarning(logFE) << "File is not valid for link";
        return false;
    }

    // 解析出新的 dci 内部文件路径
    const auto paths = resolvePath(newName, dciFilePath);
    const QString &linkPath = paths.second.isEmpty() ? newName : paths.second;

    auto result = file->link(subfilePath, linkPath) && forceSave();
    qCDebug(logFE) << "Link result:" << result;
    return result;
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
bool DDciFileEngine::mkdir(const QString &dirName,
                           bool createParentDirectories,
                           std::optional<QFile::Permissions> permissions) const
{
    Q_UNUSED(permissions)
#else
bool DDciFileEngine::mkdir(const QString &dirName, bool createParentDirectories) const
{
#endif
    qCDebug(logFE) << "Creating directory:" << dirName << "createParentDirectories:" << createParentDirectories;
    if (!file->isValid()) {
        qCWarning(logFE) << "File is not valid for mkdir";
        return false;
    }
    // 解析出新的 dci 内部文件路径
    const auto paths = resolvePath(dirName, dciFilePath);
    if (paths.second.isEmpty()) {
        qCWarning(logFE) << "Failed to resolve target path for mkdir";
        return false;
    }

    if (!createParentDirectories) {
        auto result = file->mkdir(paths.second) && forceSave();
        qCDebug(logFE) << "Mkdir result:" << result;
        return result;
    }

    const QStringList dirItems = paths.second.split('/');
    QString currentPath;
    for (const QString &newDir : dirItems) {
        if (newDir.isEmpty())
            continue;
        currentPath += ("/" + newDir);
        if (file->exists(currentPath)) {
            qCDebug(logFE) << "Directory already exists:" << currentPath;
            continue;
        }
        // 创建此路径
        if (!file->mkdir(currentPath)) {
            qCWarning(logFE) << "Failed to create directory:" << currentPath;
            return false;
        }
        qCDebug(logFE) << "Created directory:" << currentPath;
    }

    auto result = forceSave();
    qCDebug(logFE) << "Mkdir with parent directories result:" << result;
    return result;
}

bool DDciFileEngine::rmdir(const QString &dirName, bool recurseParentDirectories) const
{
    qCDebug(logFE) << "Removing directory:" << dirName << "recurseParentDirectories:" << recurseParentDirectories;
    if (!file->isValid()) {
        qCWarning(logFE) << "File is not valid for rmdir";
        return false;
    }
    // 解析出新的 dci 内部文件路径
    const auto paths = resolvePath(dirName, dciFilePath);
    if (paths.second.isEmpty()) {
        qCWarning(logFE) << "Failed to resolve target path for rmdir";
        return false;
    }

    if (!file->remove(paths.second))
        return false;
    if (!recurseParentDirectories)
        return forceSave();

    // 查找空的父目录
    QDir dir(paths.second);

    while (dir.cdUp()) {
        // 不删除根
        if (dir.isRoot())
            break;

        if (file->childrenCount(dir.absolutePath()) > 0)
            continue;
        if (!file->remove(dir.absolutePath()))
            return false;
    }

    return forceSave();
}

bool DDciFileEngine::setSize(qint64 size)
{
    qCDebug(logFE) << "Setting file size to:" << size;
    if (!fileBuffer) {
        qCDebug(logFE) << "No file buffer, loading data from file";
        fileData = file->dataRef(subfilePath);
    }

    // 确保新数据填充为 0
    if (size > fileData.size()) {
        qCDebug(logFE) << "Expanding file data from" << fileData.size() << "to" << size;
        fileData.append(size - fileData.size(), '\0');
    } else {
        qCDebug(logFE) << "Truncating file data from" << fileData.size() << "to" << size;
        fileData.resize(size);
    }

    auto result = fileBuffer ? true : forceSave(true);
    qCDebug(logFE) << "SetSize result:" << result;
    return result;
}

bool DDciFileEngine::caseSensitive() const
{
    qCDebug(logFE) << "Checking case sensitivity, returning true";
    return true;
}

bool DDciFileEngine::isRelativePath() const
{
    auto isRelative = !subfilePath.startsWith('/');
    qCDebug(logFE) << "Checking if path is relative:" << isRelative;
    return isRelative;
}

QByteArray DDciFileEngine::id() const
{
    qCDebug(logFE) << "Getting file engine ID";
    auto id = fileName().toUtf8();
    qCDebug(logFE) << "File engine ID:" << id;
    return id;
}

uint DDciFileEngine::ownerId(QAbstractFileEngine::FileOwner owner) const
{
    qCDebug(logFE) << "Getting owner ID for:" << owner;
    QFileInfo info(dciFilePath);
    auto ownerId = owner == OwnerUser ? info.ownerId() : info.groupId();
    qCDebug(logFE) << "Owner ID:" << ownerId;
    return ownerId;
}

QString DDciFileEngine::owner(QAbstractFileEngine::FileOwner owner) const
{
    qCDebug(logFE) << "Getting owner for:" << owner;
    QFileInfo info(dciFilePath);
    auto ownerName = owner == OwnerUser ? info.owner() : info.group();
    qCDebug(logFE) << "Owner:" << ownerName;
    return ownerName;
}

QAbstractFileEngine::FileFlags DDciFileEngine::fileFlags(QAbstractFileEngine::FileFlags type) const
{
    qCDebug(logFE) << "Getting file flags for type:" << type;
    auto flags = QAbstractFileEngine::FileFlags();

    if (!file->isValid()) {
        qCDebug(logFE) << "File is not valid, returning empty flags";
        return flags;
    }

    if (type & TypesMask) {
        const auto fileType = file->type(subfilePath);
        qCDebug(logFE) << "File type:" << fileType;

        if (fileType == DDciFile::Directory) {
            flags |= DirectoryType;
            qCDebug(logFE) << "File is directory";
        } else if (fileType == DDciFile::File) {
            flags |= FileType;
            qCDebug(logFE) << "File is regular file";
        } else if (fileType == DDciFile::Symlink) {
            flags |= LinkType;
            qCDebug(logFE) << "File is symlink";
        }
    }

    if ((type & FlagsMask)) {
        if (file->exists(subfilePath)) {
            flags |= ExistsFlag;
            qCDebug(logFE) << "File exists";
        }

        if (subfilePath == QLatin1Char('/')) {
            flags |= RootFlag;
            qCDebug(logFE) << "File is root";
        }
    }

    if ((type & PermsMask) && file->exists(subfilePath)) {
        auto permissions = static_cast<FileFlags>(static_cast<int>(QFileInfo(dciFilePath).permissions()));
        flags |= permissions;
        qCDebug(logFE) << "File permissions:" << permissions;
    }

    qCDebug(logFE) << "File flags result:" << flags;
    return flags;
}

QString DDciFileEngine::fileName(QAbstractFileEngine::FileName file) const
{
    qCDebug(logFE) << "Getting file name for type:" << file;
    QString result;
    
    switch (file) {
    case AbsoluteName:
    case CanonicalName:
        result = QDir::cleanPath(DCI_FILE_SCHEME + dciFilePath + subfilePath);
        break;
    case DefaultName:
        result = QDir::cleanPath(DCI_FILE_SCHEME + dciFilePath + subfilePath);
        break;
    case AbsolutePathName:
        result = QDir::cleanPath(DCI_FILE_SCHEME + dciFilePath);
        break;
    case BaseName:
        result = QFileInfo(subfilePath).baseName();
        break;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    case AbsoluteLinkTarget:
#else
    case LinkName:
#endif
        result = this->file->type(subfilePath) == DDciFile::Symlink
                ? this->file->symlinkTarget(subfilePath)
                : QString();
        break;
    default:
        result = QString();
        break;
    }
    
    qCDebug(logFE) << "File name result:" << result;
    return result;
}

void DDciFileEngine::setFileName(const QString &fullPath)
{
    qCDebug(logFE) << "Setting file name to:" << fullPath;
    // 销毁旧的内容
    close();
    file.reset(nullptr);
    dciFilePath.clear();
    subfilePath.clear();

    const auto paths = resolvePath(fullPath, QString(), false);
    if (paths.first.isEmpty()
            || paths.second.isEmpty()) {
        qCWarning(logFE) << "Failed to resolve path for:" << fullPath;
        return;
    }

    dciFilePath = paths.first;
    subfilePath = paths.second;
    qCDebug(logFE) << "Resolved DCI file path:" << dciFilePath << "subfile path:" << subfilePath;
    file = getDciFile(dciFilePath, QFile::exists(dciFilePath));
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 1)
QDateTime DDciFileEngine::fileTime(QFile::FileTime time) const
{
    qCDebug(logFE) << "Getting file time for:" << time;
    auto result = QFileInfo(dciFilePath).fileTime(time);
    qCDebug(logFE) << "File time result:" << result;
    return result;
}
#else
QDateTime DDciFileEngine::fileTime(QAbstractFileEngine::FileTime time) const
{
    qCDebug(logFE) << "Getting abstract file time for:" << time;
    auto result = QFileInfo(dciFilePath).fileTime(static_cast<QFile::FileTime>(time));
    qCDebug(logFE) << "Abstract file time result:" << result;
    return result;
}
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 1)
QAbstractFileEngine::IteratorUniquePtr DDciFileEngine::beginEntryList(const QString &path, QDirListing::IteratorFlags filters, const QStringList &filterNames)
#elif QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
QAbstractFileEngine::IteratorUniquePtr DDciFileEngine::beginEntryList(const QString &path, QDir::Filters filters, const QStringList &filterNames)
#else
DDciFileEngine::Iterator *DDciFileEngine::beginEntryList(QDir::Filters filters, const QStringList &filterNames)
#endif
{
    qCDebug(logFE) << "Beginning entry list with filters:" << filters << "filterNames:" << filterNames;
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    Q_UNUSED(path);
    auto iterator = QAbstractFileEngine::IteratorUniquePtr(new DDciFileEngineIterator(filters, filterNames));
    qCDebug(logFE) << "Entry list iterator created successfully";
    return iterator;
#else
    auto iterator = new DDciFileEngineIterator(filters, filterNames);
    qCDebug(logFE) << "Entry list iterator created successfully";
    return iterator;
#endif
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
QAbstractFileEngine::IteratorUniquePtr DDciFileEngine::endEntryList()
#else
DDciFileEngine::Iterator *DDciFileEngine::endEntryList()
#endif
{
    qCDebug(logFE) << "Ending entry list, returning nullptr";
    return nullptr;
}

qint64 DDciFileEngine::read(char *data, qint64 maxlen)
{
    qCDebug(logFE) << "Reading" << maxlen << "bytes from file";
    auto bytesRead = fileBuffer->read(data, maxlen);
    qCDebug(logFE) << "Bytes read:" << bytesRead;
    return bytesRead;
}

qint64 DDciFileEngine::write(const char *data, qint64 len)
{
    qCDebug(logFE) << "Writing" << len << "bytes to file";
    auto bytesWritten = fileBuffer->write(data, len);
    qCDebug(logFE) << "Bytes written:" << bytesWritten;
    return bytesWritten;
}

bool DDciFileEngine::extension(QAbstractFileEngine::Extension extension,
                               const QAbstractFileEngine::ExtensionOption *option,
                               QAbstractFileEngine::ExtensionReturn *output)
{
    qCDebug(logFE) << "Extension called:" << extension;
    Q_UNUSED(option)
    Q_UNUSED(output)
    auto result = extension == AtEndExtension && fileBuffer->atEnd();
    qCDebug(logFE) << "Extension result:" << result;
    return result;
}

bool DDciFileEngine::supportsExtension(QAbstractFileEngine::Extension extension) const
{
    qCDebug(logFE) << "Checking support for extension:" << extension;
    auto result = extension == AtEndExtension;
    qCDebug(logFE) << "Supports extension result:" << result;
    return result;
}

bool DDciFileEngine::cloneTo(QAbstractFileEngine *target)
{
    qCDebug(logFE) << "Cloning file engine to target";
    const QByteArray &data = file->dataRef(subfilePath);
    auto result = target->write(data.constData(), data.size()) == data.size();
    qCDebug(logFE) << "Clone result:" << result;
    return result;
}

bool DDciFileEngine::forceSave(bool writeFile) const
{
    qCDebug(logFE) << "Force saving file, writeFile:" << writeFile;
    QFile file(dciFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(logFE) << "Failed to open file for writing:" << dciFilePath;
        return false;
    }
    
    auto result = flushToFile(&file, writeFile);
    qCDebug(logFE) << "Force save result:" << result;
    return result;
}

QPair<QString, QString> DDciFileEngine::resolvePath(const QString &fullPath,
                                                    const QString &realFilePath,
                                                    bool needRealFileExists)
{
    if (!fullPath.startsWith(QStringLiteral(DCI_FILE_SCHEME) + realFilePath))
        return {};

    qCDebug(logFE(), "Resolve the path: \"%s\"", qPrintable(fullPath));
    // 此路径来源于调用方，将其格式化为标准格式，尾部添加 "/" 以确保下文中得到的
    // subfilePath 绝对不为空
    QString formatFullPath = QDir::cleanPath(fullPath) + "/";
    QString dciFilePath = realFilePath, subfilePath;
    const int schemeLength = qstrlen(DCI_FILE_SCHEME);
    const int suffixLength = qstrlen(DCI_FILE_SUFFIX);

    if (dciFilePath.isEmpty()) {
        // 尾部加 "/" 是确保 ".dci" 为一个文件的结尾
        int dciSuffixIndex = formatFullPath.indexOf(DCI_FILE_SUFFIX "/", schemeLength);

        while (dciSuffixIndex > 0) {
            dciSuffixIndex += suffixLength;
            dciFilePath = formatFullPath.mid(schemeLength, dciSuffixIndex - schemeLength);
            // 查找一个有效的后缀名是 ".dci" 的文件
            if (needRealFileExists) {
                if (QFileInfo(dciFilePath).isFile())
                    break;
            } else {
                QFileInfo info(dciFilePath);
                // 不存在的文件允许被新建
                if (!info.exists() && !info.isSymLink())
                    break;
            }

            dciSuffixIndex = dciFilePath.indexOf(DCI_FILE_SUFFIX, dciSuffixIndex + 1);
        }
    } else {
        qCDebug(logFE(), "The base file path of user is: \"%s\"", qPrintable(realFilePath));
    }

    // 未找到有效的 dci 文件
    if (dciFilePath.isEmpty())
        return {};

    subfilePath = QDir::cleanPath(formatFullPath.mid(schemeLength + dciFilePath.length()));
    qCDebug(logFE(), "The DCI file path is: \"%s\", the subfile path is: \"%s\"",
            qPrintable(dciFilePath), qPrintable(subfilePath));
    Q_ASSERT(!subfilePath.isEmpty());

    return qMakePair(dciFilePath, subfilePath);
}

DCORE_END_NAMESPACE
