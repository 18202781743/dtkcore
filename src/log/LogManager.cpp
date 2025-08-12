// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <QtCore>
#include "LogManager.h"
#include <DSGApplication>
#include <Logger.h>
#include <ConsoleAppender.h>
#include <RollingFileAppender.h>
#include <JournalAppender.h>
#include <QLoggingCategory>

#include "dstandardpaths.h"
#include "dconfig_org_deepin_dtk_preference.hpp"

DCORE_BEGIN_NAMESPACE

#ifdef QT_DEBUG
Q_LOGGING_CATEGORY(logLogManager, "dtk.core.logmanager")
#else
Q_LOGGING_CATEGORY(logLogManager, "dtk.core.logmanager", QtInfoMsg)
#endif

#define RULES_KEY ("rules")
// Courtesy qstandardpaths_unix.cpp
static void appendOrganizationAndApp(QString &path)
{
    qCDebug(logLogManager) << "Appending organization and app to path:" << path;
#ifndef QT_BOOTSTRAPPED
    const QString org = QCoreApplication::organizationName();
    if (!org.isEmpty())
        path += QLatin1Char('/') + org;
    const QString appName = QCoreApplication::applicationName();
    if (!appName.isEmpty())
        path += QLatin1Char('/') + appName;
#else
    Q_UNUSED(path);
#endif
    qCDebug(logLogManager) << "Path after appending:" << path;
}

#define DEFAULT_FMT "%{time}{yyyy-MM-dd, HH:mm:ss.zzz} [%{type:-7}] [%{file:-20} %{function:-35} %{line}] %{message}"

class DLogManagerPrivate {
public:
    explicit DLogManagerPrivate(DLogManager *q)
        : m_format(DEFAULT_FMT)
        , q_ptr(q)
    {
        qCDebug(logLogManager) << "DLogManagerPrivate created";
    }

    dconfig_org_deepin_dtk_preference *createDConfig(const QString &appId);
    void initLoggingRules();
    void updateLoggingRules();

    QString m_format;
    QString m_logPath;
    ConsoleAppender* m_consoleAppender = nullptr;
    RollingFileAppender* m_rollingFileAppender = nullptr;
    JournalAppender* m_journalAppender = nullptr;
    QScopedPointer<dconfig_org_deepin_dtk_preference> m_dsgConfig;
    QScopedPointer<dconfig_org_deepin_dtk_preference> m_fallbackConfig;

    DLogManager *q_ptr = nullptr;
    Q_DECLARE_PUBLIC(DLogManager)

};

dconfig_org_deepin_dtk_preference *DLogManagerPrivate::createDConfig(const QString &appId)
{
    qCDebug(logLogManager) << "Creating DConfig for appId:" << appId;
    if (appId.isEmpty()) {
        qCWarning(logLogManager) << "AppId is empty, cannot create DConfig";
        return nullptr;
    }

    auto config = dconfig_org_deepin_dtk_preference::create(appId);
    QObject::connect(config, &dconfig_org_deepin_dtk_preference::rulesChanged,
                     config, [this](){ updateLoggingRules(); });

    qCDebug(logLogManager) << "DConfig created successfully for appId:" << appId;
    return config;
}

void DLogManagerPrivate::initLoggingRules()
{
    qCDebug(logLogManager) << "Initializing logging rules";
    if (qEnvironmentVariableIsSet("DTK_DISABLED_LOGGING_RULES") || qEnvironmentVariableIsSet("QT_LOGGING_RULES")) {
        qCDebug(logLogManager) << "Logging rules disabled by environment variables";
        return;
    }

    // 1. 未指定 fallbackId 时，以 dsgAppId 为准
    QString dsgAppId = DSGApplication::id();
    qCDebug(logLogManager) << "DSG Application ID:" << dsgAppId;
    m_dsgConfig.reset(createDConfig(dsgAppId));

    if (m_dsgConfig) {
        qCDebug(logLogManager) << "DSG config created successfully";
        QObject::connect(m_dsgConfig.data(), &dconfig_org_deepin_dtk_preference::configInitializeSucceed,
                         m_dsgConfig.data(), [this](){ updateLoggingRules(); });
        QObject::connect(m_dsgConfig.data(), &dconfig_org_deepin_dtk_preference::configInitializeFailed,
                         m_dsgConfig.data(), [this, dsgAppId] {
                             m_dsgConfig.reset();
                             qCWarning(logLogManager) << "Logging rules config is invalid, please check `appId` [" << dsgAppId << "] arg is correct";
                         });
    } else {
        qCWarning(logLogManager) << "Failed to create DSG config for appId:" << dsgAppId;
    }

    QString fallbackId = qgetenv("DTK_LOGGING_FALLBACK_APPID");
    qCDebug(logLogManager) << "Fallback app ID:" << fallbackId;
    // 2. fallbackId 和 dsgAppId 非空且不等时，都创建和监听变化
    if (!fallbackId.isEmpty() && fallbackId != dsgAppId) {
        qCDebug(logLogManager) << "Creating fallback config";
        m_fallbackConfig.reset(createDConfig(fallbackId));
    }

    // 3. 默认值和非默认值时，非默认值优先
    if (m_fallbackConfig) {
        qCDebug(logLogManager) << "Fallback config created successfully";
        QObject::connect(m_fallbackConfig.data(), &dconfig_org_deepin_dtk_preference::configInitializeSucceed,
                         m_fallbackConfig.data(), [this](){ updateLoggingRules(); });
        QObject::connect(m_fallbackConfig.data(), &dconfig_org_deepin_dtk_preference::configInitializeFailed,
                         m_fallbackConfig.data(), [this, fallbackId] {
                             m_fallbackConfig.reset();
                             qCWarning(logLogManager) << "Logging rules config is invalid, please check `appId` [" << fallbackId << "] arg is correct";
                         });
    }
}

void DLogManagerPrivate::updateLoggingRules()
{
    qCDebug(logLogManager) << "DLogManagerPrivate updateLoggingRules called";
    QVariant var;
    // 4. 优先看 dsgConfig 是否默认值，其次 fallback 是否默认值
    if (m_dsgConfig && m_dsgConfig->isInitializeSucceed() && !m_dsgConfig->rulesIsDefaultValue()) {
        qCDebug(logLogManager) << "Using DSG config rules (non-default)";
        var = m_dsgConfig->rules();
    } else if (m_fallbackConfig && m_dsgConfig->isInitializeSucceed() && !m_fallbackConfig->rulesIsDefaultValue()) {
        qCDebug(logLogManager) << "Using fallback config rules (non-default)";
        var = m_fallbackConfig->rules();
    } else if (m_dsgConfig && m_dsgConfig->isInitializeSucceed()) {
        qCDebug(logLogManager) << "Using DSG config rules (default)";
        var = m_dsgConfig->rules();
    } else {
        qCDebug(logLogManager) << "No valid config found for logging rules";
    }

    if (var.isValid()) {
        QString rules = var.toString().replace(";", "\n");
        qCDebug(logLogManager) << "Setting logging rules:" << rules;
        QLoggingCategory::setFilterRules(rules);
    } else {
        qCDebug(logLogManager) << "No valid logging rules to set";
    }
}
/*!
@~english
  \class Dtk::Core::DLogManager
  \inmodule dtkcore

  \brief DLogManager is the deepin user application log manager.
 */

DLogManager::DLogManager()
    :d_ptr(new DLogManagerPrivate(this))
{
    d_ptr->initLoggingRules();
}

void DLogManager::initConsoleAppender(){
    qCDebug(logLogManager) << "DLogManager initConsoleAppender called";
    Q_D(DLogManager);
    d->m_consoleAppender = new ConsoleAppender;
    d->m_consoleAppender->setFormat(d->m_format);
    dlogger->registerAppender(d->m_consoleAppender);
    qCDebug(logLogManager) << "DLogManager console appender initialized successfully";
}

void DLogManager::initRollingFileAppender(){
    qCDebug(logLogManager) << "DLogManager initRollingFileAppender called";
    Q_D(DLogManager);
    QString logFilePath = getlogFilePath();
    qCDebug(logLogManager) << "DLogManager using log file path:" << logFilePath;
    d->m_rollingFileAppender = new RollingFileAppender(logFilePath);
    d->m_rollingFileAppender->setFormat(d->m_format);
    d->m_rollingFileAppender->setLogFilesLimit(5);
    d->m_rollingFileAppender->setDatePattern(RollingFileAppender::DailyRollover);
    dlogger->registerAppender(d->m_rollingFileAppender);
    qCDebug(logLogManager) << "DLogManager rolling file appender initialized successfully";
}

void DLogManager::initJournalAppender()
{
    qCDebug(logLogManager) << "DLogManager initJournalAppender called";
#if (defined BUILD_WITH_SYSTEMD && defined Q_OS_LINUX)
    Q_D(DLogManager);
    d->m_journalAppender = new JournalAppender();
    dlogger->registerAppender(d->m_journalAppender);
    qCDebug(logLogManager) << "DLogManager journal appender initialized successfully";
#else
    qCWarning(logLogManager) << "BUILD_WITH_SYSTEMD not defined or OS not support!!";
#endif
}

/*!
@~english
  \brief Registers the appender to write the log records to the Console.

  \sa registerFileAppender
 */
void DLogManager::registerConsoleAppender(){
    qCDebug(logLogManager) << "DLogManager registerConsoleAppender called";
    DLogManager::instance()->initConsoleAppender();
}

/*!
@~english
  \brief Registers the appender to write the log records to the file.

  \sa getlogFilePath
  \sa registerConsoleAppender
 */
void DLogManager::registerFileAppender() {
    qCDebug(logLogManager) << "DLogManager registerFileAppender called";
    DLogManager::instance()->initRollingFileAppender();
}

void DLogManager::registerJournalAppender()
{
    qCDebug(logLogManager) << "DLogManager registerJournalAppender called";
    DLogManager::instance()->initJournalAppender();
}

/*!
@~english
  \brief Return the path file log storage.

  \brief DLogManager::getlogFilePath Get the log file path
  \brief The default log path is ~/.cache/<OrganizationName>/<ApplicationName>.log
  \brief If the environment variable $HOME cannot be acquired, DLogManager will not log anything
  \sa registerFileAppender
 */
QString DLogManager::getlogFilePath()
{
    qCDebug(logLogManager) << "DLogManager getlogFilePath called";
    //No longer set the default log path (and mkdir) when constructing now, instead set the default value if it's empty when getlogFilePath is called.
    //Fix the problem that the log file is still created in the default path when the log path is set manually.
    if (DLogManager::instance()->d_func()->m_logPath.isEmpty()) {
        qCDebug(logLogManager) << "DLogManager log path is empty, setting default path";
        if (DStandardPaths::homePath().isEmpty()) {
            qCWarning(logLogManager) << "Unable to locate the cache directory, cannot acquire home directory, and the log will not be written to file..";
            return QString();
        }

        QString cachePath(DStandardPaths::path(DStandardPaths::XDG::CacheHome));
        qCDebug(logLogManager) << "DLogManager cache path:" << cachePath;
        appendOrganizationAndApp(cachePath);
        qCDebug(logLogManager) << "DLogManager cache path after append:" << cachePath;

        if (!QDir(cachePath).exists()) {
            qCDebug(logLogManager) << "DLogManager creating cache directory:" << cachePath;
            QDir(cachePath).mkpath(cachePath);
        }
        QString logPath = instance()->joinPath(cachePath, QString("%1.log").arg(QCoreApplication::applicationName()));
        qCDebug(logLogManager) << "DLogManager setting log path:" << logPath;
        instance()->d_func()->m_logPath = logPath;
    } else {
        qCDebug(logLogManager) << "DLogManager using existing log path:" << DLogManager::instance()->d_func()->m_logPath;
    }

    QString result = QDir::toNativeSeparators(DLogManager::instance()->d_func()->m_logPath);
    qCDebug(logLogManager) << "DLogManager getlogFilePath result:" << result;
    return result;
}

/*!
@~english
  \brief DLogManager::setlogFilePath Set the log file path
  \a logFilePath Log file path
  \brief If the file path set is not the file path, nothing will do, and an output warning
 */
void DLogManager::setlogFilePath(const QString &logFilePath)
{
    qCDebug(logLogManager) << "DLogManager setlogFilePath called with:" << logFilePath;
    QFileInfo info(logFilePath);
    if (info.exists() && !info.isFile()) {
        qCWarning(logLogManager) << "Invalid file path:" << logFilePath << "is not a file";
    } else {
        qCDebug(logLogManager) << "DLogManager setting log path to:" << logFilePath;
        DLogManager::instance()->d_func()->m_logPath = logFilePath;
    }
}

void DLogManager::setLogFormat(const QString &format)
{
    qCDebug(logLogManager) << "DLogManager setLogFormat called with:" << format;
    //m_format = "%{time}{yyyy-MM-dd, HH:mm:ss.zzz} [%{type:-7}] [%{file:-20} %{function:-35} %{line}] %{message}\n";
    DLogManager::instance()->d_func()->m_format = format;
    qCDebug(logLogManager) << "DLogManager log format set successfully";
}

QString DLogManager::joinPath(const QString &path, const QString &fileName){
    qCDebug(logLogManager) << "DLogManager joinPath called with path:" << path << ", fileName:" << fileName;
    QString separator(QDir::separator());
    QString result = QString("%1%2%3").arg(path, separator, fileName);
    qCDebug(logLogManager) << "DLogManager joinPath result:" << result;
    return result;
}

DLogManager::~DLogManager()
{
    qCDebug(logLogManager) << "DLogManager destructor called";
}

DCORE_END_NAMESPACE
