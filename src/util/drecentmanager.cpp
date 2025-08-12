// SPDX-FileCopyrightText: 2017 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "drecentmanager.h"
#include <QMimeDatabase>
#include <QDomDocument>
#include <QTextStream>
#include <QDateTime>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QUrl>
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
#include <QStringConverter>
#include <QTimeZone>
#endif
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(logUtil, "dtk.core.util")

#define RECENT_PATH QDir::homePath() + "/.local/share/recently-used.xbel"

/*!
  \class Dtk::Core::DRecentManager
  \inmodule dtkcore
  
  \brief DRecentManager 是用来管理最近文件列表的类，提供了添加与删除文件项.
  
  遵循 freedesktop 标准，在本地 share 目录存放，文件名为: recently-used.xbel，所以每个用户都有不同的列表。
  该类的存在就是为 deepin 应用提供一个工具类，方便让打开的文件添加到最近文件列表中。

  \sa Dtk::Core::DRecentData
 */

/*!
  \class Dtk::Core::DRecentData
  \inmodule dtkcore

  \brief 文件信息结构体.
  
  \table
  \row
    \li appName
    \li 应用名称
  \row
    \li appExec
    \li 应用命令行名称
  \row
    \li mimeType
    \li 文件 mimetype 名称，一般不需要填写，DRecentManager 内部自动获取
  \endtable
  \sa Dtk::Core::DRecentManager
 */

/*!
  \brief DRecentManager::addItem 在最近列表中添加一个项.
  \a uri 文件路径
  \a data 数据信息
  \return 如果返回 true 则成功添加，false 为添加失败
 */

bool DRecentManager::addItem(const QString &uri, DRecentData &data)
{
    qCDebug(logUtil) << "Adding item to recent list: uri=" << uri << "appName=" << data.appName << "appExec=" << data.appExec;
    
    if (!QFileInfo(uri).exists() || uri.isEmpty()) {
        qCWarning(logUtil) << "File does not exist or URI is empty:" << uri;
        return false;
    }

    QFile file(RECENT_PATH);
    qCDebug(logUtil) << "Opening recent file:" << RECENT_PATH;
    file.open(QIODevice::ReadWrite | QIODevice::Text);

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    QString dateTime = QDateTime::currentDateTime().toTimeZone(QTimeZone::UTC).toString(Qt::ISODate);
#else
    QString dateTime = QDateTime::currentDateTime().toTimeSpec(Qt::OffsetFromUTC).toString(Qt::ISODate);
#endif
    qCDebug(logUtil) << "Current datetime:" << dateTime;
    QDomDocument doc;

    if (!doc.setContent(&file)) {
        qCDebug(logUtil) << "Creating new XML document";
        doc.clear();
        doc.appendChild(doc.createProcessingInstruction("xml","version=\'1.0\' encoding=\'utf-8\'"));
        QDomElement xbelEle = doc.createElement("xbel");
        xbelEle.setAttribute("xmlns:mime", "http://www.freedesktop.org/standards/shared-mime-info");
        xbelEle.setAttribute("version", "1.0");
        xbelEle.setAttribute("xmlns:bookmark", "http://www.freedesktop.org/standards/desktop-bookmarks");
        doc.appendChild(xbelEle);
    } else {
        qCDebug(logUtil) << "Loaded existing XML document";
    }
    file.close();

    // need to add file:// protocol.
    QUrl url = QUrl::fromLocalFile(uri);
    qCDebug(logUtil) << "Converted URI to URL:" << url.toString();

    // get the MimeType name of the file.
    if (data.mimeType.isEmpty()) {
        data.mimeType = QMimeDatabase().mimeTypeForFile(uri).name();
        qCDebug(logUtil) << "Detected MIME type:" << data.mimeType;
    } else {
        qCDebug(logUtil) << "Using provided MIME type:" << data.mimeType;
    }

    QDomElement rootEle = doc.documentElement();
    QDomNodeList nodeList = rootEle.elementsByTagName("bookmark");
    qCDebug(logUtil) << "Found" << nodeList.size() << "existing bookmarks";
    QDomElement bookmarkEle;
    bool isFound = false;

    // find bookmark element exists.
    for (int i = 0; i < nodeList.size(); ++i) {
        const QString fileUrl = nodeList.at(i).toElement().attribute("href");

        if (fileUrl == url.toEncoded(QUrl::FullyDecoded)) {
            qCDebug(logUtil) << "Found existing bookmark at index" << i;
            bookmarkEle = nodeList.at(i).toElement();
            isFound = true;
            break;
        }
    }
    
    if (!isFound) {
        qCDebug(logUtil) << "No existing bookmark found, will create new one";
    }

    // update element content.
    if (isFound) {
        qCDebug(logUtil) << "Updating existing bookmark";
        QDomNodeList appList = bookmarkEle.elementsByTagName("bookmark:application");
        qCDebug(logUtil) << "Found" << appList.size() << "applications for this bookmark";
        QDomElement appEle;
        bool appExists = false;

        for (int i = 0; i < appList.size(); ++i) {
            appEle = appList.at(i).toElement();

            if (appEle.attribute("name") == data.appName &&
                appEle.attribute("exec") == data.appExec) {
                qCDebug(logUtil) << "Found existing application at index" << i;
                appExists = true;
                break;
            }
        }

        if (appExists) {
            int count = appEle.attribute("count").toInt() + 1;
            qCDebug(logUtil) << "Updating existing application, new count:" << count;
            bookmarkEle.setAttribute("modified", dateTime);
            bookmarkEle.setAttribute("visited", dateTime);
            appEle.setAttribute("modified", dateTime);
            appEle.setAttribute("count", QString::number(count));
        } else {
            qCDebug(logUtil) << "Adding new application to existing bookmark";
            QDomNode appsNode = bookmarkEle.elementsByTagName("bookmark:applications").at(0);

            appEle = doc.createElement("bookmark:application");
            appEle.setAttribute("name", data.appName);
            appEle.setAttribute("exec", data.appExec);
            appEle.setAttribute("modified", dateTime);
            appEle.setAttribute("count", "1");
            appsNode.toElement().appendChild(appEle);
        }
    }
    // add new elements if they don't exist.
    else {
        qCDebug(logUtil) << "Creating new bookmark element";
        QDomElement bookmarkEle, infoEle, metadataEle, mimeEle, appsEle, appChildEle;
        QString hrefStr = url.toEncoded(QUrl::FullyEncoded);

        bookmarkEle = doc.createElement("bookmark");
        bookmarkEle.setAttribute("href", hrefStr);
        bookmarkEle.setAttribute("added", dateTime);
        bookmarkEle.setAttribute("modified", dateTime);
        bookmarkEle.setAttribute("visited", dateTime);

        infoEle = doc.createElement("info");
        bookmarkEle.appendChild(infoEle);

        metadataEle = doc.createElement("metadata");
        metadataEle.setAttribute("owner", "http://freedesktop.org");
        infoEle.appendChild(metadataEle);

        mimeEle = doc.createElement("mime:mime-type");
        mimeEle.setAttribute("type", data.mimeType);
        metadataEle.appendChild(mimeEle);

        appsEle = doc.createElement("bookmark:applications");
        appChildEle = doc.createElement("bookmark:application");
        appChildEle.setAttribute("name", data.appName);
        appChildEle.setAttribute("exec", data.appExec);
        appChildEle.setAttribute("modified", dateTime);
        appChildEle.setAttribute("count", "1");

        appsEle.appendChild(appChildEle);
        metadataEle.appendChild(appsEle);

        QDomNode result = rootEle.appendChild(bookmarkEle);
        if (result.isNull()) {
            qCWarning(logUtil) << "Failed to append bookmark element to document";
            return false;
        }
        qCDebug(logUtil) << "Successfully created new bookmark element";
    }

    // write to file.
    qCDebug(logUtil) << "Writing updated document to file";
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(logUtil) << "Failed to open file for writing:" << RECENT_PATH;
        return false;
    }

    QTextStream out(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#else
    out.setCodec("UTF-8");
#endif
    out << doc.toString();
    out.flush();
    file.close();

    qCDebug(logUtil) << "Successfully added item to recent list";
    return true;
}

/*!
  \brief DRecentManager::removeItem 在最近列表中移除单个文件路径
  \a target 需要移除的文件路径
 */

void DRecentManager::removeItem(const QString &target)
{
    qCDebug(logUtil) << "Removing single item:" << target;
    removeItems(QStringList() << target);
}

/*!
  \brief DRecentManager::removeItem 在最近列表中移除多个文件路径
  \a list 需要移除的文件路径列表
 */

void DRecentManager::removeItems(const QStringList &list)
{
    qCDebug(logUtil) << "Removing" << list.size() << "items from recent list";
    QFile file(RECENT_PATH);

    if (!file.open(QIODevice::ReadOnly)) {
        qCWarning(logUtil, "Failed to open recent file for reading: %s", qPrintable(RECENT_PATH));
        return;
    }

    QDomDocument doc;
    if (!doc.setContent(&file)) {
        qCWarning(logUtil, "Failed to parse XML content from recent file");
        file.close();
        return;
    }
    file.close();

    QDomElement rootEle = doc.documentElement();
    QDomNodeList nodeList = rootEle.elementsByTagName("bookmark");
    qCDebug(logUtil) << "Found" << nodeList.count() << "bookmarks to check";

    for (int i = 0; i < nodeList.count(); ) {
        const QString fileUrl = nodeList.at(i).toElement().attribute("href");

        if (list.contains(QUrl::fromPercentEncoding(fileUrl.toLatin1())) ||
            list.contains(QUrl(fileUrl).toEncoded(QUrl::FullyDecoded))) {
            qCDebug(logUtil) << "Removing bookmark at index" << i << ":" << fileUrl;
            rootEle.removeChild(nodeList.at(i));
        } else {
            ++i;
        }
    }

    qCDebug(logUtil) << "Writing updated document after removal";
    if (!file.open(QIODevice::WriteOnly)) {
        qCWarning(logUtil, "Failed to open file for writing after removal: %s", qPrintable(RECENT_PATH));
        return;
    }

    QTextStream out(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    out.setEncoding(QStringConverter::Utf8);
#else
    out.setCodec("UTF-8");
#endif
    out << doc.toString();
    out.flush();
    file.close();

    qCDebug(logUtil) << "Successfully removed items from recent list";
    return;
}

DCORE_END_NAMESPACE
