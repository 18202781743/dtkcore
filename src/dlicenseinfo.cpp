// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dlicenseinfo.h"

#include <DObjectPrivate>

#include <QFile>
#include <QJsonParseError>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

DCORE_BEGIN_NAMESPACE

class DLicenseInfo::DComponentInfoPrivate : public DObjectPrivate
{
public:
    QString name;
    QString version;
    QString copyRight;
    QString licenseName;

protected:
    explicit DComponentInfoPrivate(DLicenseInfo::DComponentInfo *qq)
        : DObjectPrivate(qq)
    {
    }

private:
    Q_DECLARE_PUBLIC(DLicenseInfo::DComponentInfo)
    friend class DLicenseInfoPrivate;
};

DLicenseInfo::DComponentInfo::DComponentInfo(DObject * parent)
    : DObject(*new DLicenseInfo::DComponentInfoPrivate(this), parent)
{
    qCDebug(logLicenseInfo, "DComponentInfo constructor called");
}

DLicenseInfo::DComponentInfo::~DComponentInfo()
{
    qCDebug(logLicenseInfo, "DComponentInfo destructor called");
}

QString DLicenseInfo::DComponentInfo::name() const
{
    qCDebug(logLicenseInfo, "DComponentInfo name called");
    QString result = d_func()->name;
    qCDebug(logLicenseInfo, "DComponentInfo name returning: %s", qPrintable(result));
    return result;
}

QString DLicenseInfo::DComponentInfo::version() const
{
    qCDebug(logLicenseInfo, "DComponentInfo version called");
    QString result = d_func()->version;
    qCDebug(logLicenseInfo, "DComponentInfo version returning: %s", qPrintable(result));
    return result;
}

QString DLicenseInfo::DComponentInfo::copyRight() const
{
    qCDebug(logLicenseInfo, "DComponentInfo copyRight called");
    QString result = d_func()->copyRight;
    qCDebug(logLicenseInfo, "DComponentInfo copyRight returning: %s", qPrintable(result));
    return result;
}

QString DLicenseInfo::DComponentInfo::licenseName() const
{
    qCDebug(logLicenseInfo, "DComponentInfo licenseName called");
    QString result = d_func()->licenseName;
    qCDebug(logLicenseInfo, "DComponentInfo licenseName returning: %s", qPrintable(result));
    return result;
}

class Q_DECL_HIDDEN DLicenseInfoPrivate : public DObjectPrivate
{
public:
    explicit DLicenseInfoPrivate(DLicenseInfo *qq);
    ~DLicenseInfoPrivate() override;

    bool loadFile(const QString &file);
    bool loadContent(const QByteArray &content);
    QByteArray licenseContent(const QString &licenseName);
    void clear();

    QString licenseSearchPath;
    DLicenseInfo::DComponentInfos componentInfos;
};

DLicenseInfoPrivate::DLicenseInfoPrivate(DLicenseInfo *qq)
    : DObjectPrivate(qq)
{
    qCDebug(logLicenseInfo) << "DLicenseInfoPrivate constructor called";
}

DLicenseInfoPrivate::~DLicenseInfoPrivate()
{
    qCDebug(logLicenseInfo) << "DLicenseInfoPrivate destructor called";
    clear();
}

bool DLicenseInfoPrivate::loadFile(const QString &file)
{
    qCDebug(logLicenseInfo) << "DLicenseInfoPrivate loadFile called with file: " << file;
    QFile jsonFile(file);
    if (!jsonFile.open(QIODevice::ReadOnly)) {
        qWarning() << QString("Failed on open file: \"%1\", error message: \"%2\"").arg(
            qPrintable(jsonFile.fileName()), qPrintable(jsonFile.errorString()));
        return false;
    }
    return loadContent(jsonFile.readAll());
}

bool DLicenseInfoPrivate::loadContent(const QByteArray &content)
{
    qCDebug(logLicenseInfo, "DLicenseInfoPrivate loadContent called with %d bytes", content.size());
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(content, &error);
    if (error.error != QJsonParseError::NoError) {
        qCWarning(logLicenseInfo, "When loading the license, parseJson failed: %s", qPrintable(error.errorString()));
        return false;
    }
    if (!jsonDoc.isArray()) {
        qCWarning(logLicenseInfo, "When loading the license, parseJson failed: it is not a JSON array");
        return false;
    }

    qCDebug(logLicenseInfo, "DLicenseInfoPrivate clearing existing component infos");
    clear();
    QJsonArray array = jsonDoc.array();
    qCDebug(logLicenseInfo, "DLicenseInfoPrivate parsing %d components", array.size());
    
    for (const QJsonValue &value : array) {
        if (!value.isObject()) {
            qCWarning(logLicenseInfo, "When loading the license, parseJson failed: it is not a JSON object!");
            return false;
        }
        DLicenseInfo::DComponentInfo *componentInfo = new DLicenseInfo::DComponentInfo;
        QJsonObject obj = value.toObject();
        QJsonValue name = obj.value("name");
        QJsonValue version = obj.value("version");
        QJsonValue copyright = obj.value("copyright");
        QJsonValue license = obj.value("license");
        if (!name.isString() || !version.isString()
            || !copyright.isString() || !license.isString()) {
            qCWarning(logLicenseInfo, "When loading the license, parseJson failed: it is not a string!");
            return false;
        }
        componentInfo->d_func()->name = name.toString();
        componentInfo->d_func()->version = version.toString();
        componentInfo->d_func()->copyRight = copyright.toString();
        componentInfo->d_func()->licenseName = license.toString();
        componentInfos.append(componentInfo);
        qCDebug(logLicenseInfo, "DLicenseInfoPrivate added component: %s, version: %s, license: %s", 
                qPrintable(componentInfo->d_func()->name), 
                qPrintable(componentInfo->d_func()->version), 
                qPrintable(componentInfo->d_func()->licenseName));
    }
    qCDebug(logLicenseInfo, "DLicenseInfoPrivate loadContent completed successfully, loaded %d components", componentInfos.size());
    return true;
}

QByteArray DLicenseInfoPrivate::licenseContent(const QString &licenseName)
{
    qCDebug(logLicenseInfo, "DLicenseInfoPrivate licenseContent called for license: %s", qPrintable(licenseName));
    QByteArray content;
    QStringList dirs{"/usr/share/spdx-license"};
    if (!licenseSearchPath.isEmpty()) {
        qCDebug(logLicenseInfo, "DLicenseInfoPrivate prepending license search path: %s", qPrintable(licenseSearchPath));
        dirs.prepend(licenseSearchPath);
    }
    
    qCDebug(logLicenseInfo, "DLicenseInfoPrivate searching in %d directories", dirs.size());
    for (const QString &dir : dirs) {
        QString filePath = QString("%1/%2.txt").arg(dir).arg(licenseName);
        qCDebug(logLicenseInfo, "DLicenseInfoPrivate checking file: %s", qPrintable(filePath));
        QFile file(filePath);
        if (!file.exists()) {
            qCDebug(logLicenseInfo, "DLicenseInfoPrivate file does not exist: %s", qPrintable(filePath));
            continue;
        }
        if (file.open(QIODevice::ReadOnly)) {
            content = file.readAll();
            file.close();
            qCDebug(logLicenseInfo, "DLicenseInfoPrivate found license content, size: %d bytes", content.size());
            break;
        } else {
            qCDebug(logLicenseInfo, "DLicenseInfoPrivate failed to open file: %s", qPrintable(filePath));
        }
    }
    if (content.isEmpty()) {
        qCWarning(logLicenseInfo, "License content is empty when getting license content!");
    }
    return content;
}

void DLicenseInfoPrivate::clear()
{
    qCDebug(logLicenseInfo, "DLicenseInfoPrivate clear called, deleting %d component infos", componentInfos.size());
    qDeleteAll(componentInfos);
    componentInfos.clear();
    qCDebug(logLicenseInfo, "DLicenseInfoPrivate clear completed");
}

DLicenseInfo::DLicenseInfo(DObject *parent)
    : DObject(*new DLicenseInfoPrivate(this), parent)
{
    qCDebug(logLicenseInfo, "DLicenseInfo constructor called");
}

bool DLicenseInfo::loadContent(const QByteArray &content)
{
    qCDebug(logLicenseInfo, "DLicenseInfo loadContent called with %d bytes", content.size());
    D_D(DLicenseInfo);
    bool result = d->loadContent(content);
    qCDebug(logLicenseInfo, "DLicenseInfo loadContent result: %s", result ? "success" : "failed");
    return result;
}

bool DLicenseInfo::loadFile(const QString &file)
{
    qCDebug(logLicenseInfo, "DLicenseInfo loadFile called with: %s", qPrintable(file));
    D_D(DLicenseInfo);
    bool result = d->loadFile(file);
    qCDebug(logLicenseInfo, "DLicenseInfo loadFile result: %s", result ? "success" : "failed");
    return result;
}

void DLicenseInfo::setLicenseSearchPath(const QString &path)
{
    qCDebug(logLicenseInfo, "DLicenseInfo setLicenseSearchPath called with: %s", qPrintable(path));
    D_D(DLicenseInfo);
    d->licenseSearchPath = path;
    qCDebug(logLicenseInfo, "DLicenseInfo license search path set successfully");
}

QByteArray DLicenseInfo::licenseContent(const QString &licenseName)
{
    qCDebug(logLicenseInfo, "DLicenseInfo licenseContent called for license: %s", qPrintable(licenseName));
    D_D(DLicenseInfo);
    QByteArray content = d->licenseContent(licenseName);
    qCDebug(logLicenseInfo, "DLicenseInfo licenseContent returned %d bytes", content.size());
    return content;
}

DLicenseInfo::DComponentInfos DLicenseInfo::componentInfos() const
{
    qCDebug(logLicenseInfo, "DLicenseInfo componentInfos called");
    D_DC(DLicenseInfo);
    qCDebug(logLicenseInfo, "DLicenseInfo componentInfos returning %d components", d->componentInfos.size());
    return d->componentInfos;
}

DCORE_END_NAMESPACE
