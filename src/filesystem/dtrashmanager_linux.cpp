// SPDX-FileCopyrightText: 2017 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dtrashmanager.h"
#include "dstandardpaths.h"
#include "base/private/dobject_p.h"

#include <QDirIterator>
#include <QStorageInfo>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QLoggingCategory>

#define TRASH_PATH \
    DStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/Trash"
#define TRASH_INFO_PATH TRASH_PATH"/info"
#define TRASH_FILES_PATH TRASH_PATH"/files"

DCORE_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(logFilesystem)

class DTrashManager_ : public DTrashManager {};
Q_GLOBAL_STATIC(DTrashManager_, globalTrashManager)

static QString getNotExistsFileName(const QString &fileName, const QString &targetPath)
{
    qCDebug(logFilesystem) << "Getting non-existent filename for:" << fileName << "in:" << targetPath;
    QByteArray name = fileName.toUtf8();

    int index = name.lastIndexOf('.');
    QByteArray suffix;

    if (index >= 0) {
        suffix = name.mid(index);
        qCDebug(logFilesystem) << "Found suffix:" << suffix;
    }

    if (suffix.size() > 200) {
        qCWarning(logFilesystem) << "Suffix too long, truncating to 200 characters";
        suffix = suffix.left(200);
    }

    name.chop(suffix.size());
    name = name.left(200 - suffix.size());

    while (QFile::exists(targetPath + "/" + name + suffix)) {
        qCDebug(logFilesystem) << "File exists, generating new name with hash";
        name = QCryptographicHash::hash(name, QCryptographicHash::Md5).toHex();
    }

    QString result = QString::fromUtf8(name + suffix);
    qCDebug(logFilesystem) << "Generated filename:" << result;
    return result;
}

static bool writeTrashInfo(const QString &fileBaseName, const QString &sourceFilePath, const QDateTime &datetime, QString *errorString = NULL)
{
    qCDebug(logFilesystem) << "Writing trash info for:" << fileBaseName << "source:" << sourceFilePath;
    QFile metadata(TRASH_INFO_PATH"/" + fileBaseName + ".trashinfo");

    if (metadata.exists()) {
        qCWarning(logFilesystem) << "Trash info file already exists:" << metadata.fileName();
        if (errorString) {
            *errorString = QString("The %1 file is exists").arg(metadata.fileName());
        }

        return false;
    }

    if (!metadata.open(QIODevice::WriteOnly)) {
        qCWarning(logFilesystem) << "Failed to open trash info file for writing:" << metadata.errorString();
        if (errorString) {
            *errorString = metadata.errorString();
        }

        return false;
    }

    QByteArray data;

    data.append("[Trash Info]\n");
    data.append("Path=").append(sourceFilePath.toUtf8().toPercentEncoding("/")).append("\n");
    data.append("DeletionDate=").append(datetime.toString(Qt::ISODate).toLatin1()).append("\n");

    qint64 size = metadata.write(data);
    metadata.close();

    if (size <= 0) {
        qCWarning(logFilesystem) << "Failed to write trash info file:" << metadata.errorString();
        if (errorString) {
            *errorString = metadata.errorString();
        }

        return false;
    }

    qCDebug(logFilesystem) << "Successfully wrote trash info file, size:" << size;
    return true;
}

static bool renameFile(const QFileInfo &fileInfo, const QString &target, QString *errorString = NULL)
{
    qCDebug(logFilesystem) << "Renaming file from:" << fileInfo.absoluteFilePath() << "to:" << target;
    
    if (fileInfo.isSymLink()) {
        qCDebug(logFilesystem) << "File is a symlink, following it";
        QFileInfo targetInfo(fileInfo.symLinkTarget());
        if (!targetInfo.exists()) {
            qCWarning(logFilesystem) << "Symlink target does not exist:" << fileInfo.symLinkTarget();
            if (errorString) {
                *errorString = QString("The symlink target %1 is not exists").arg(fileInfo.symLinkTarget());
            }
            return false;
        }
        // Use targetInfo instead of trying to modify const fileInfo
        QFile file(targetInfo.absoluteFilePath());
        if (!file.rename(target)) {
            qCWarning(logFilesystem) << "Failed to rename file:" << file.errorString();
            if (errorString) {
                *errorString = file.errorString();
            }
            return false;
        }
        
        qCDebug(logFilesystem) << "Successfully renamed file to:" << target;
        return true;
    }

    // Handle non-symlink files
    if (!fileInfo.exists()) {
        qCWarning(logFilesystem) << "File does not exist:" << fileInfo.absoluteFilePath();
        if (errorString) {
            *errorString = QString("The %1 file is not exists").arg(fileInfo.absoluteFilePath());
        }
        return false;
    }

    QFile file(fileInfo.absoluteFilePath());
    if (!file.rename(target)) {
        qCWarning(logFilesystem) << "Failed to rename file:" << file.errorString();
        if (errorString) {
            *errorString = file.errorString();
        }
        return false;
    }

    qCDebug(logFilesystem) << "Successfully renamed file to:" << target;
    return true;
}

class DTrashManagerPrivate : public DTK_CORE_NAMESPACE::DObjectPrivate
{
public:
    DTrashManagerPrivate(DTrashManager *q_ptr)
        : DObjectPrivate(q_ptr) {
        qCDebug(logFilesystem) << "DTrashManagerPrivate created";
    }

    static bool removeFileOrDir(const QString &path);
    static bool removeFromIterator(QDirIterator &iter);

    D_DECLARE_PUBLIC(DTrashManager)
};

DTrashManager *DTrashManager::instance()
{
    qCDebug(logFilesystem) << "Getting DTrashManager instance";
    return globalTrashManager;
}

bool DTrashManager::trashIsEmpty() const
{
    qCDebug(logFilesystem) << "Checking if trash is empty";
    QDirIterator iterator(TRASH_INFO_PATH,
//                          QStringList() << "*.trashinfo",
                          QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden);

    auto isEmpty = !iterator.hasNext();
    qCDebug(logFilesystem) << "Trash is empty:" << isEmpty;
    return isEmpty;
}

bool DTrashManager::cleanTrash()
{
    qCDebug(logFilesystem) << "Cleaning trash";
    QDirIterator iterator_info(TRASH_INFO_PATH,
                               QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden);

    QDirIterator iterator_files(TRASH_FILES_PATH,
                                QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                                QDirIterator::Subdirectories);

    auto result = DTrashManagerPrivate::removeFromIterator(iterator_info) &&
           DTrashManagerPrivate::removeFromIterator(iterator_files);
    qCDebug(logFilesystem) << "Clean trash result:" << result;
    return result;
}

bool DTrashManager::moveToTrash(const QString &filePath, bool followSymlink)
{
    qCDebug(logFilesystem) << "DTrashManager moveToTrash called for:" << filePath << "followSymlink:" << followSymlink;
    QFileInfo fileInfo(filePath);

    if (!fileInfo.exists() && (followSymlink || !fileInfo.isSymLink())) {
        qCWarning(logFilesystem, "File does not exist: %s", qPrintable(filePath));
        return false;
    }

    QDir trashDir(TRASH_FILES_PATH);
    QStorageInfo storageInfo(fileInfo.filePath());
    QStorageInfo trashStorageInfo(trashDir);

    if (storageInfo != trashStorageInfo) {
        qCWarning(logFilesystem, "Storage info mismatch for file: %s", qPrintable(filePath));
        return false;
    }

    if (!trashDir.mkpath(TRASH_INFO_PATH)) {
        qCWarning(logFilesystem, "Failed to create trash info path");
        return false;
    }

    if (!trashDir.mkpath(TRASH_FILES_PATH)) {
        qCWarning(logFilesystem, "Failed to create trash files path");
        return false;
    }

    if (followSymlink && fileInfo.isSymLink()) {
        qCDebug(logFilesystem, "Following symlink to: %s", qPrintable(fileInfo.symLinkTarget()));
        fileInfo.setFile(fileInfo.symLinkTarget());
    }

    const QString &fileName = getNotExistsFileName(fileInfo.fileName(), TRASH_FILES_PATH);
    qCDebug(logFilesystem, "Generated trash filename: %s", qPrintable(fileName));

    if (!writeTrashInfo(fileName, fileInfo.filePath(), QDateTime::currentDateTime())) {
        qCWarning(logFilesystem, "Failed to write trash info for: %s", qPrintable(fileName));
        return false;
    }

    const QString &newFilePath = TRASH_FILES_PATH"/" + fileName;
    qCDebug(logFilesystem, "Moving file to trash: %s", qPrintable(newFilePath));

    bool result = renameFile(fileInfo, newFilePath);
    qCDebug(logFilesystem, "Move to trash result: %s", result ? "success" : "failed");
    return result;
}

DTrashManager::DTrashManager()
    : QObject()
    , DObject(*new DTrashManagerPrivate(this))
{
    qCDebug(logFilesystem, "DTrashManager constructor called");
}

bool DTrashManagerPrivate::removeFileOrDir(const QString &path)
{
    qCDebug(logFilesystem, "Removing file or directory: %s", qPrintable(path));
    QFileInfo fileInfo(path);
    
    if (fileInfo.isDir()) {
        qCDebug(logFilesystem, "Removing directory: %s", qPrintable(path));
        QDir dir(path);
        if (!dir.removeRecursively()) {
            qCWarning(logFilesystem, "Failed to remove directory recursively: %s", qPrintable(path));
            return false;
        }
    } else {
        qCDebug(logFilesystem, "Removing file: %s", qPrintable(path));
        QFile file(path);
        if (!file.remove()) {
            qCWarning(logFilesystem, "Failed to remove file: %s", qPrintable(file.errorString()));
            return false;
        }
    }
    
    qCDebug(logFilesystem, "Successfully removed: %s", qPrintable(path));
    return true;
}

bool DTrashManagerPrivate::removeFromIterator(QDirIterator &iter)
{
    qCDebug(logFilesystem, "Removing files from iterator");
    bool success = true;
    
    while (iter.hasNext()) {
        QString path = iter.next();
        qCDebug(logFilesystem, "Processing path from iterator: %s", qPrintable(path));
        
        if (!removeFileOrDir(path)) {
            qCWarning(logFilesystem, "Failed to remove path from iterator: %s", qPrintable(path));
            success = false;
        }
    }
    
    qCDebug(logFilesystem, "Iterator removal completed, success: %s", success ? "true" : "false");
    return success;
}

DCORE_END_NAMESPACE
