// SPDX-FileCopyrightText: 2017 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dtrashmanager.h"

#include "DObjectPrivate"

#include <QDirIterator>
#include <QStorageInfo>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(logFilesystem)

class DTrashManager_ : public DTrashManager {};
Q_GLOBAL_STATIC(DTrashManager_, globalTrashManager)

static QString getNotExistsFileName(const QString &fileName, const QString &targetPath)
{
    qCDebug(logFilesystem, "Getting non-existent filename for: %s in: %s", qPrintable(fileName), qPrintable(targetPath));
    QByteArray name = fileName.toUtf8();

    int index = name.lastIndexOf('.');
    QByteArray suffix;

    if (index >= 0)
    {
        suffix = name.mid(index);
        qCDebug(logFilesystem, "Found suffix: %s", suffix.data());
    }

    if (suffix.size() > 200)
    {
        qCWarning(logFilesystem, "Suffix too long, truncating to 200 characters");
        suffix = suffix.left(200);
    }

    name.chop(suffix.size());
    name = name.left(200 - suffix.size());

    while (QFile::exists(targetPath + "/" + name + suffix))
    {
        qCDebug(logFilesystem, "File exists, generating new name with hash");
        name = QCryptographicHash::hash(name, QCryptographicHash::Md5).toHex();
    }

    QString result = QString::fromUtf8(name + suffix);
    qCDebug(logFilesystem, "Generated filename: %s", qPrintable(result));
    return result;
}

static bool renameFile(const QFileInfo &fileInfo, const QString &target, QString *errorString = NULL)
{
    qCDebug(logFilesystem, "Renaming file from: %s to: %s", qPrintable(fileInfo.filePath()), qPrintable(target));
    
    if (fileInfo.isFile() || fileInfo.isSymLink())
    {
        qCDebug(logFilesystem, "Renaming file/symlink");
        QFile file(fileInfo.filePath());

        if (!file.rename(target))
        {
            qCWarning(logFilesystem, "Failed to rename file: %s", qPrintable(file.errorString()));
            if (errorString)
            {
                *errorString = file.errorString();
            }

            return false;
        }

        qCDebug(logFilesystem, "File renamed successfully");
        return true;
    }
    else
    {
        qCDebug(logFilesystem, "Renaming directory");
        QDirIterator iterator(fileInfo.filePath(),
                              QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);

        while (iterator.hasNext())
        {
            iterator.next();

            const QString newFile = iterator.filePath().replace(0, fileInfo.filePath().length(), target);

            if (!QDir().mkpath(QFileInfo(newFile).path()))
            {
                qCWarning(logFilesystem, "Failed to create path: %s", qPrintable(QFileInfo(newFile).path()));
                if (errorString)
                {
                    *errorString = QString("Make the %1 path is failed").arg(QFileInfo(newFile).path());
                }

                return false;
            }

            if (!renameFile(iterator.fileInfo(), newFile, errorString))
            {
                qCWarning(logFilesystem, "Failed to rename subfile");
                return false;
            }
        }

        if (!QDir().rmdir(fileInfo.filePath()))
        {
            qCWarning(logFilesystem, "Failed to remove directory: %s", qPrintable(fileInfo.filePath()));
            if (errorString)
            {
                *errorString = QString("Cannot remove the %1 dir").arg(fileInfo.filePath());
            }

            return false;
        }

        qCDebug(logFilesystem, "Directory renamed successfully");
        return true;
    }
}

class DTrashManagerPrivate : public DTK_CORE_NAMESPACE::DObjectPrivate
{
public:
    DTrashManagerPrivate(DTrashManager *q_ptr)
        : DObjectPrivate(q_ptr) {}

    D_DECLARE_PUBLIC(DTrashManager)
};

DTrashManager *DTrashManager::instance()
{
    qCDebug(logFilesystem, "DTrashManager instance called");
    qCDebug(logFilesystem, "DTrashManager returning global instance");
    return globalTrashManager;
}

bool DTrashManager::trashIsEmpty() const
{
    qCDebug(logFilesystem, "DTrashManager trashIsEmpty called");
    qCDebug(logFilesystem, "DTrashManager trashIsEmpty returning false (dummy implementation)");
    return false;
}

bool DTrashManager::cleanTrash()
{
    qCDebug(logFilesystem, "DTrashManager cleanTrash called");
    qCDebug(logFilesystem, "DTrashManager cleanTrash returning false (dummy implementation)");
    return false;
}

bool DTrashManager::moveToTrash(const QString &filePath, bool followSymlink)
{
    qCDebug(logFilesystem, "DTrashManager moveToTrash called for: %s, followSymlink: %s", 
            qPrintable(filePath), followSymlink ? "true" : "false");
    qCDebug(logFilesystem, "DTrashManager moveToTrash returning false (dummy implementation)");
    return false;
}

DTrashManager::DTrashManager()
    : QObject()
    , DObject(*new DTrashManagerPrivate(this))
{

}

DCORE_END_NAMESPACE
