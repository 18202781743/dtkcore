// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dconfig.h"
#ifndef D_DISABLE_DCONFIG
#include "dconfigfile.h"
#ifndef D_DISABLE_DBUS_CONFIG
#include "configmanager_interface.h"
#include "manager_interface.h"
#endif
#else
#include <QSettings>
#endif
#include "dobject_p.h"
#include <DSGApplication>

#include <QLoggingCategory>
#include <QCoreApplication>
#include <unistd.h>

// https://gitlabwh.uniontech.com/wuhan/se/deepin-specifications/-/issues/3

DCORE_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(cfLog)
static QString NoAppId;

/*!
@~english
    @class Dtk::Core::DConfigBackend
    \inmodule dtkcore

    @brief Configure the abstract interface of the backend.

    All configuration backends used by DConfig inherit this class, and users can inherit this class to implement their own configuration backends.
 */

/*!
@~english
    @fn bool DConfigBackend::load(const QString &) = 0

    @brief Initialize the backend

    \a appId Managed configuration information key value, the default is the application name.
  */

/*!
@~english
    @fn bool DConfigBackend::isValid() const = 0

    @sa DConfig::isValid().

 */

/*!
@~english
    @fn QStringList DConfigBackend::keyList() const = 0

    @sa DConfig::keyList()

 */

/*!
@~english
    @fn QVariant DConfigBackend::value(const QString &key, const QVariant &fallback = QVariant()) const = 0

    @sa DConfig::value()
 */

/*!
@~english
    @fn void DConfigBackend::setValue(const QString &key, const QVariant &value) = 0

    @sa DConfig::setValue()
 */

/*!
@~english
    @fn void DConfigBackend::reset(const QString &key)

    @sa DConfig::reset()
 */

/*!
@~english
    @fn QString DConfigBackend::name() const = 0

    @brief The unique identity of the backend configuration
 */

/*!
@~english
 @fn bool DConfigBackend::isDefaultValue(const QString &key) const = 0

 @sa DConfig::isDefaultValue()

 */

DConfigBackend::~DConfigBackend()
{
    qCDebug(cfLog, "DConfigBackend destructor called");
}

static QString _globalAppId;
class Q_DECL_HIDDEN DConfigPrivate : public DObjectPrivate
{
public:
    explicit DConfigPrivate(DConfig *qq,
                            const QString &appId,
                            const QString &name,
                            const QString &subpath)
        : DObjectPrivate(qq)
        , appId(appId)
        , name(name)
        , subpath(subpath)
    {
        qCDebug(cfLog) << "DConfigPrivate created:" << "appId=" << appId << "name=" << name << "subpath=" << subpath;
    }

    virtual ~DConfigPrivate() override
    {
        qCDebug(cfLog) << "DConfigPrivate destructor called for appId=" << appId;
    };

    inline bool invalid() const
    {
        const bool valid = backend && backend->isValid();
        if (!valid)
            qCWarning(cfLog, "DConfig is invalid of appid=%s name=%s, subpath=%s",
                      qPrintable(appId), qPrintable(name), qPrintable(subpath));

        return !valid;
    }

    DConfigBackend *getOrCreateBackend();
    DConfigBackend *createBackendByEnv();

    QString appId;
    QString name;
    QString subpath;
    QScopedPointer<DConfigBackend> backend;

    D_DECLARE_PUBLIC(DConfig)
};

namespace {

#ifndef D_DISABLE_DCONFIG
class Q_DECL_HIDDEN FileBackend : public DConfigBackend
{
public:
    explicit FileBackend(DConfigPrivate *o)
        : owner(o)
    {
        qCDebug(cfLog) << "FileBackend created for appId=" << owner->appId;
    }

    virtual ~FileBackend() override
    {
        qCDebug(cfLog) << "FileBackend destructor called";
    };

    virtual bool isValid() const override
    {
        const auto &valid = configFile && configFile->isValid();
        qCDebug(cfLog) << "FileBackend isValid:" << valid;
        return valid;
    }

    virtual bool load(const QString &/*appId*/) override
    {
        qCDebug(cfLog) << "FileBackend load called for appId=" << owner->appId;
        
        if (configFile) {
            qCDebug(cfLog) << "FileBackend configFile already exists, returning true";
            return true;
        }

        configFile.reset(new DConfigFile(owner->appId,owner->name, owner->subpath));
        configCache.reset(configFile->createUserCache(getuid()));
        const QString &prefix = localPrefix();

        qCDebug(cfLog) << "FileBackend loading config with prefix=" << prefix;

        if (!configFile->load(prefix) || !configCache->load(prefix)) {
            qCWarning(cfLog) << "FileBackend failed to load config file or cache";
            return false;
        }

        // generic config doesn't need to fallback to generic configration.
        if (owner->appId == NoAppId) {
            qCDebug(cfLog) << "FileBackend NoAppId case, returning true";
            return true;
        }

        std::unique_ptr<DConfigFile> file(new DConfigFile(NoAppId, owner->name, owner->subpath));
        const auto &canFallbackToGeneric = !file->meta()->metaPath(prefix).isEmpty();
        if (canFallbackToGeneric) {
            qCDebug(cfLog) << "FileBackend attempting to fallback to generic config";
            std::unique_ptr<DConfigCache> cache(file->createUserCache(getuid()));
            if (file->load(prefix) && cache->load(prefix)) {
                genericConfigFile.reset(file.release());
                genericConfigCache.reset(cache.release());
                qCDebug(cfLog) << "FileBackend successfully loaded generic config";
            } else {
                qCDebug(cfLog) << "FileBackend failed to load generic config";
            }
        } else {
            qCDebug(cfLog) << "FileBackend no generic config fallback available";
        }
        return true;
    }

    virtual QStringList keyList() const override
    {
        const auto &keys = configFile->meta()->keyList();
        qCDebug(cfLog) << "FileBackend keyList returned" << keys.size() << "keys";
        return keys;
    }

    virtual QVariant value(const QString &key, const QVariant &fallback) const override
    {
        qCDebug(cfLog) << "FileBackend value called for key=" << key;
        
        const auto &vc = configFile->cacheValue(configCache.get(), key);
        if (vc.isValid()) {
            qCDebug(cfLog) << "FileBackend found value in cache for key=" << key;
            return vc;
        }

        // fallback to generic configuration, and use itself's configuration if generic isn't set.
        if (genericConfigFile) {
            qCDebug(cfLog) << "FileBackend checking generic config for key=" << key;
            const auto &vg = genericConfigFile->cacheValue(genericConfigCache.get(), key);
            if (vg.isValid()) {
                qCDebug(cfLog) << "FileBackend found value in generic config for key=" << key;
                return vg;
            }
        }

        qCDebug(cfLog) << "FileBackend using fallback value for key=" << key;
        return fallback;
    }

    virtual bool isDefaultValue(const QString &key) const override
    {
        const auto &isDefault = configFile->isDefaultValue(configCache.get(), key);
        qCDebug(cfLog) << "FileBackend isDefaultValue for key=" << key << ":" << isDefault;
        return isDefault;
    }

    virtual void setValue(const QString &key, const QVariant &value) override
    {
        qCDebug(cfLog) << "FileBackend setValue called for key=" << key;
        configFile->setValue(configCache.get(), key, value);
        Q_EMIT owner->q_func()->valueChanged(key);
        qCDebug(cfLog) << "FileBackend setValue completed for key=" << key;
    }

    virtual void reset(const QString &key) override
    {
        qCDebug(cfLog) << "FileBackend reset called for key=" << key;
        configFile->reset(configCache.get(), key);
    }

    virtual QString name() const override
    {
        return QStringLiteral("file");
    }

    QString localPrefix() const
    {
        if (!envLocalPrefix.isEmpty()) {
            qCDebug(cfLog) << "FileBackend using envLocalPrefix:" << QString::fromLocal8Bit(envLocalPrefix);
            return QString::fromLocal8Bit(envLocalPrefix);
        }
        qCDebug(cfLog) << "FileBackend using default prefix";
        return QString();
    }

private:
    QString localPrefix() const
    {
        if (!envLocalPrefix.isEmpty()) {
            return QString::fromLocal8Bit(envLocalPrefix);
        }
        return QString();
    }

private:
    QScopedPointer<DConfigFile> configFile;
    QScopedPointer<DConfigCache> configCache;
    QScopedPointer<DConfigFile> genericConfigFile;
    QScopedPointer<DConfigCache> genericConfigCache;
    DConfigPrivate* owner;
    const QByteArray envLocalPrefix = qgetenv("DSG_DCONFIG_FILE_BACKEND_LOCAL_PREFIX");
};

FileBackend::~FileBackend()
{
    const QString &prefix = localPrefix();
    if (configCache) {
        configCache->save(prefix);
        configCache.reset();
    }
    if (configFile) {
        configFile->save(prefix);
        configFile.reset();
    }
    if (genericConfigCache) {
        genericConfigCache->save(prefix);
        genericConfigCache.reset();
    }
    if (genericConfigFile) {
        genericConfigFile->save(prefix);
        genericConfigFile.reset();
    }
}

#ifndef D_DISABLE_DBUS_CONFIG

#define DSG_CONFIG "org.desktopspec.ConfigManager"
#define DSG_CONFIG_MANAGER "org.desktopspec.ConfigManager"

class Q_DECL_HIDDEN DBusBackend : public DConfigBackend
{
public:
    explicit DBusBackend(DConfigPrivate* o):
        owner(o)
    {
        qCDebug(cfLog) << "DBusBackend created for appId=" << owner->appId;
    }

    virtual ~DBusBackend() override
    {
        qCDebug(cfLog) << "DBusBackend destructor called";
    };

    static bool isServiceRegistered()
    {
        const auto &registered = QDBusConnection::systemBus().interface()->isServiceRegistered(DSG_CONFIG);
        qCDebug(cfLog) << "DBusBackend isServiceRegistered:" << registered;
        return registered;
    }

    static bool isServiceActivatable()
    {
         const QDBusReply<QStringList> activatableNames = QDBusConnection::systemBus().interface()->
                 callWithArgumentList(QDBus::AutoDetect,
                 QLatin1String("ListActivatableNames"),
                 QList<QVariant>());
//         qInfo() << activatableNames.value() << activatableNames.value().contains(DSG_CONFIG);

         const auto &activatable = activatableNames.value().contains(DSG_CONFIG);
         qCDebug(cfLog) << "DBusBackend isServiceActivatable:" << activatable;
         return activatable;
    }

    virtual bool isValid() const override
    {
        const auto &valid = config && config->isValid();
        qCDebug(cfLog) << "DBusBackend isValid:" << valid;
        return valid;
    }

    /*!
    @~english
      \internal

        Initialize the DBus connection, the call acquireManager dynamically obtains a configuration connection,
        The configuration file is then accessed through this configuration connection.
     */
    virtual bool load(const QString &/*appId*/) override
    {
        qCDebug(cfLog) << "DBusBackend load called for appId=" << owner->appId;
        
        if (config) {
            qCDebug(cfLog) << "DBusBackend config already exists, returning true";
            return true;
        }

        qCDebug(cfLog) << "Try acquire config manager object form DBus";
        DSGConfig dsg_config(DSG_CONFIG, "/", QDBusConnection::systemBus());
        QDBusPendingReply<QDBusObjectPath> dbus_reply = dsg_config.acquireManager(owner->appId, owner->name, owner->subpath);
        const auto &dbus_path = dbus_reply.value();
        if (dbus_reply.isError() || dbus_path.path().isEmpty()) {
            qCWarning(cfLog) << "Can't acquire config manager. error:" << dbus_reply.error().message();
            return false;
        } else {
            qCDebug(cfLog) << "dbus path=" << dbus_path.path();
            config.reset(new DSGConfigManager(DSG_CONFIG_MANAGER, dbus_path.path(),
                                                QDBusConnection::systemBus(), owner->q_func()));
            if (!config->isValid()) {
                qCWarning(cfLog) << "Can't acquire config path=" << dbus_path.path();
                config.reset();
                return false;
            } else {
                QObject::connect(config.data(), &DSGConfigManager::valueChanged, owner->q_func(), &DConfig::valueChanged);
                qCDebug(cfLog) << "DBusBackend successfully connected to config manager";
            }
        }
        return true;
    }

    virtual QStringList keyList() const override
    {
        const auto &keys = config->keyList();
        qCDebug(cfLog) << "DBusBackend keyList returned" << keys.size() << "keys";
        return keys;
    }

    static QVariant decodeQDBusArgument(const QVariant &v)
    {
        if (v.canConvert<QDBusArgument>()) {
            // we use QJsonValue to resolve all data type in DConfigInfo class, so it's type is equal QJsonValue::Type,
            // now we parse Map and Array type to QVariant explicitly.
            const QDBusArgument &complexType = v.value<QDBusArgument>();
            switch (complexType.currentType()) {
            case QDBusArgument::MapType: {
                qCDebug(cfLog) << "DBusBackend decoding MapType";
                QVariantMap list;
                complexType >> list;
                QVariantMap res;
                for (auto iter = list.begin(); iter != list.end(); iter++) {
                    res[iter.key()] = decodeQDBusArgument(iter.value());
                }
                return res;
            }
            case QDBusArgument::ArrayType: {
                qCDebug(cfLog) << "DBusBackend decoding ArrayType";
                QVariantList list;
                complexType >> list;
                QVariantList res;
                res.reserve(list.size());
                for (const auto &item : std::as_const(list)) {
                    res << decodeQDBusArgument(item);
                }
                return res;
            }
            default:
                qCWarning(cfLog) << "Can't parse the type, it maybe need user to do it, QDBusArgument::ElementType:" << complexType.currentType();
            }
        }
        return v;
    }

    virtual QVariant value(const QString &key, const QVariant &fallback) const override
    {
        qCDebug(cfLog) << "DBusBackend value called for key=" << key;
        
        auto reply = config->value(key);
        reply.waitForFinished();
        if (reply.isError()) {
            qCWarning(cfLog) << "value error key:" << key << "error message:" << reply.error().message();
            return fallback;
        }
        
        const auto &result = decodeQDBusArgument(reply.value().variant());
        qCDebug(cfLog) << "DBusBackend value completed for key=" << key;
        return result;
    }

    virtual bool isDefaultValue(const QString &key) const override
    {
        qCDebug(cfLog) << "DBusBackend isDefaultValue called for key=" << key;
        
        auto reply = config->isDefaultValue(key);
        reply.waitForFinished();
        if (reply.isError()) {
            qCWarning(cfLog) << "Failed to call `isDefaultValue`, key:" << key << "error message:" << reply.error().message();
            return false;
        }
        
        const auto &isDefault = reply.value();
        qCDebug(cfLog) << "DBusBackend isDefaultValue for key=" << key << ":" << isDefault;
        return isDefault;
    }

    virtual void setValue(const QString &key, const QVariant &value) override
    {
        qCDebug(cfLog) << "DBusBackend setValue called for key=" << key;
        
        auto reply = config->setValue(key, QDBusVariant(value));
        reply.waitForFinished();
        if (reply.isError()) {
            qCWarning(cfLog) << "setValue error key:" << key << "error message:" << reply.error().message();
        } else {
            qCDebug(cfLog) << "DBusBackend setValue completed for key=" << key;
        }
    }

    virtual void reset(const QString &key) override
    {
        qCDebug(cfLog) << "DBusBackend reset called for key=" << key;
        
        auto reply = config->reset(key);
        reply.waitForFinished();
        if (reply.isError()) {
            qCWarning(cfLog) << "reset error key:" << key << "error message:" << reply.error().message();
        } else {
            qCDebug(cfLog) << "DBusBackend reset completed for key=" << key;
        }
    }

    virtual QString name() const override
    {
        return QString("DBusBackend");
    }

private:
    QScopedPointer<DSGConfigManager> config;
    DConfigPrivate* owner;
};

DBusBackend::~DBusBackend()
{
    qCDebug(cfLog) << "DBusBackend destructor called";
    if (config) {
        config->release();
    }
}
#endif //D_DISABLE_DBUS_CONFIG
#else

class Q_DECL_HIDDEN QSettingBackend : public DConfigBackend
{
public:
    explicit QSettingBackend(DConfigPrivate* o):
        owner(o)
    {
        qCDebug(cfLog) << "QSettingBackend created for appId=" << owner->appId;
    }

    virtual ~QSettingBackend() override
    {
        qCDebug(cfLog) << "QSettingBackend destructor called";
    };

    virtual bool isValid() const override
    {
        const auto &valid = settings && settings->isValid();
        qCDebug(cfLog) << "QSettingBackend isValid:" << valid;
        return valid;
    }

    virtual bool load(const QString &appid) override
    {
        qCDebug(cfLog) << "QSettingBackend load called for appId=" << appid;
        
        if (settings) {
            qCDebug(cfLog) << "QSettingBackend settings already exists, returning true";
            return true;
        }

        settings.reset(new QSettings(owner->name, QSettings::IniFormat));
        settings->beginGroup(owner->subpath);
        const auto &valid = settings->isValid();
        qCDebug(cfLog) << "QSettingBackend load completed, isValid:" << valid;
        return valid;
    }

    virtual QStringList keyList() const override
    {
        const auto &keys = settings->childKeys();
        qCDebug(cfLog) << "QSettingBackend keyList returned" << keys.size() << "keys";
        return keys;
    }

    virtual QVariant value(const QString &key, const QVariant &fallback) const override
    {
        const auto &result = settings->value(key, fallback);
        qCDebug(cfLog) << "QSettingBackend value completed for key=" << key;
        return result;
    }

    virtual void setValue(const QString &key, const QVariant &value) override
    {
        settings->setValue(key, value);
        settings->sync();
        qCDebug(cfLog) << "QSettingBackend setValue completed for key=" << key;
    }

    virtual QString name() const override
    {
        return QString("QSettingBackend");
    }

private:
    QScopedPointer<QSettings> settings;
    DConfigPrivate* owner;
};

QSettingBackend::~QSettingBackend()
{
    qCDebug(cfLog, "QSettingBackend destructor called");
}

#endif //D_DISABLE_DCONFIG
}

DConfigPrivate::~DConfigPrivate()
{
    backend.reset();
}

/*!
@~english
  \internal

    @brief Create a configuration backend

    The default configuration backend preferentially selects the D-Bus interface in the configuration center or the file configuration backend interface based on environment variables.
    If this environment variable is not configured, the configuration center service or file configuration backend interface will be selected according to whether the configuration center provides D-Bus services
 */
DConfigBackend *DConfigPrivate::getOrCreateBackend()
{
    qCDebug(cfLog) << "Getting or creating backend for appId=" << appId;
    if (backend) {
        qCDebug(cfLog) << "Backend already exists, returning existing backend";
        return backend.data();
    }
    if (auto backendEnv = createBackendByEnv()) {
        qCDebug(cfLog) << "Created backend from environment";
        backend.reset(backendEnv);
        return backend.data();
    }
#ifndef D_DISABLE_DCONFIG
#ifndef D_DISABLE_DBUS_CONFIG
    if (DBusBackend::isServiceRegistered() || DBusBackend::isServiceActivatable()) {
        qCDebug(cfLog) << "Fallback to DBus mode";
        backend.reset(new DBusBackend(this));
    }
    if (!backend) {
        qCDebug(cfLog) << "Can't use DBus config service, fallback to DConfigFile mode";
        backend.reset(new FileBackend(this));
    }
#else
    qCDebug(cfLog) << "Creating FileBackend";
    backend.reset(new FileBackend(this));
#endif //D_DISABLE_DBUS_CONFIG
#else
    qCDebug(cfLog) << "Creating QSettingBackend";
    backend.reset(new QSettingBackend(this));
#endif //D_DISABLE_DCONFIG
    qCDebug(cfLog) << "Backend created successfully";
    return backend.data();
}

/*!
@~english
  \internal

    @brief Create a configuration backend

    Try to choose between configuring the D-Bus interface in the center or the file configuration backend interface based on the environment variables. 
 */
DConfigBackend *DConfigPrivate::createBackendByEnv()
{
    qCDebug(cfLog) << "Creating backend by environment";
    const auto &envBackend = qgetenv("DSG_DCONFIG_BACKEND_TYPE");
    if (!envBackend.isEmpty()) {
        qCDebug(cfLog) << "Environment backend type:" << envBackend.constData();
        if (envBackend == "DBusBackend") {

#ifndef D_DISABLE_DCONFIG
#ifndef D_DISABLE_DBUS_CONFIG
            if (DBusBackend::isServiceRegistered() || DBusBackend::isServiceActivatable()) {
                qCDebug(cfLog) << "Fallback to DBus mode";
                return new DBusBackend(this);
            }
#endif //D_DISABLE_DBUS_CONFIG
#endif //D_DISABLE_DCONFIG
        } else if (envBackend == "FileBackend") {

#ifndef D_DISABLE_DCONFIG
            qCDebug(cfLog) << "Fallback to DConfigFile mode";
            return new FileBackend(this);
#endif //D_DISABLE_DCONFIG
        } else {

#ifndef D_DISABLE_DCONFIG
#else
            qCDebug(cfLog) << "Fallback to QSettings mode";
            return new QSettingBackend(this);
#endif //D_DISABLE_DCONFIG
        }
    } else {
        qCDebug(cfLog) << "No environment backend type specified";
    }
    qCDebug(cfLog) << "No backend created from environment";
    return nullptr;
}

/*!
@~english
    @class Dtk::Core::DConfig
    \inmodule dtkcore

    @brief Configure the interface class provided by the policy

    
    This interface specification defines the relevant interfaces provided by the development library for reading and writing configuration files,
    If the application uses a development library that implements this specification, the application should use the interfaces provided by the development library first.
 */


/*!
@~english
 * @brief Constructs the objects provided by the configuration policy
 * \a name Configuration File Name
 * \a subpath Subdirectory corresponding to the configuration file
 * \a parent Parent object
 */
DConfig::DConfig(const QString &name, const QString &subpath, QObject *parent)
    : DConfig(nullptr, name, subpath, parent)
{
}

DConfig::DConfig(DConfigBackend *backend, const QString &name, const QString &subpath, QObject *parent)
    : DConfig(backend, _globalAppId.isEmpty() ? DSGApplication::id() : _globalAppId, name, subpath, parent)
{

}

/*!
@~english
 * @brief Constructs the object provided by the configuration policy, specifying the application Id to which the configuration belongs.
 * \a appId
 * \a name
 * \a subpath
 * \a parent
 * @return The constructed configuration policy object, which is released by the caller
 * @note \a appId is not empty.
 */
DConfig *DConfig::create(const QString &appId, const QString &name, const QString &subpath, QObject *parent)
{
    Q_ASSERT(appId != NoAppId);
    return new DConfig(nullptr, appId, name, subpath, parent);
}

DConfig *DConfig::create(DConfigBackend *backend, const QString &appId, const QString &name, const QString &subpath, QObject *parent)
{
    Q_ASSERT(appId != NoAppId);
    return new DConfig(backend, appId, name, subpath, parent);
}

/*!
 * \brief Constructs the object, and which is application.
 * \param name
 * \param subpath
 * \param parent
 * \return Dconfig object, which is released by the caller
 * @note It's usually used for application independent, we should use DConfig::create if the configuration is a specific application.
 */
DConfig *DConfig::createGeneric(const QString &name, const QString &subpath, QObject *parent)
{
    return new DConfig(nullptr, NoAppId, name, subpath, parent);
}

DConfig *DConfig::createGeneric(DConfigBackend *backend, const QString &name, const QString &subpath, QObject *parent)
{
    return new DConfig(backend, NoAppId, name, subpath, parent);
}

/*!
 * \brief Explicitly specify application Id for config.
 * \param appId
 * @note It's should be called before QCoreApplication constructed.
 */
void DConfig::setAppId(const QString &appId)
{
    if (!_globalAppId.isEmpty()) {
        qCWarning(cfLog, "`setAppId`should only be called once.");
    }
    _globalAppId = appId;
    qCDebug(cfLog, "Explicitly specify application Id as appId=%s for config.", qPrintable(appId));
}

class DConfigThread : public QThread
{
public:
    DConfigThread() {
        setObjectName("DConfigGlobalThread");
        start();
    }

    ~DConfigThread() override {
        if (isRunning()) {
            quit();
            wait();
        }
    }
};

Q_GLOBAL_STATIC(DConfigThread, _globalThread)

QThread *DConfig::globalThread()
{
    return _globalThread;
}

/*!
@~english
 * @brief Use custom configuration policy backend to construct objects
 * \a backend The caller inherits the configuration policy backend of DConfigBackend
 * \a appId The application Id of the configuration file. If it is blank, it will be the application Id by default
 * \a name Configuration File Name
 * \a subpath Subdirectory corresponding to the configuration file
 * \a parent Parent object
 * @note The caller only constructs backend, which is released by DConfig.
 */
DConfig::DConfig(DConfigBackend *backend, const QString &appId, const QString &name, const QString &subpath, QObject *parent)
    : QObject(parent)
    , DObject(*new DConfigPrivate(this, appId, name, subpath))
{
    D_D(DConfig);

    qCDebug(cfLog) << "Load config of appid=" << d->appId << "name=" << d->name << "subpath=" << d->subpath;

    if (backend) {
        d->backend.reset(backend);
    }

    if (auto backend = d->getOrCreateBackend()) {
        backend->load(d->appId);
    }
}

/*!
@~english
 * @brief DConfig::backendName
 * @return Configure policy backend name
 * @note The caller can only access the DConfigBackend object with DConfig, so the DConfigBackend object is not returned.
 */
QString DConfig::backendName() const
{
    D_DC(DConfig);
    qCDebug(cfLog) << "Getting backend name";
    if (d->invalid()) {
        qCDebug(cfLog) << "DConfig is invalid, returning empty string";
        return QString();
    }

    const auto &name = d->backend->name();
    qCDebug(cfLog) << "Backend name:" << name;
    return name;
}

/*!
@~english
 * @brief Get all available configuration item names
 * @return Configuration item name collection
 */
QStringList DConfig::keyList() const
{
    D_DC(DConfig);
    qCDebug(cfLog) << "Getting key list";
    if (d->invalid()) {
        qCDebug(cfLog) << "DConfig is invalid, returning empty list";
        return QStringList();
    }

    const auto &keys = d->backend->keyList();
    qCDebug(cfLog) << "Key list returned" << keys.size() << "keys";
    return keys;
}

/*!
@~english
 * @brief Check whether the backend is available
 * @return
 */
bool DConfig::isValid() const
{
    D_DC(DConfig);
    const auto &valid = !d->invalid();
    qCDebug(cfLog) << "DConfig isValid check:" << valid;
    return valid;
}

/*!
@~english
 * @brief Check whether the value is default according to the configuration item name
 * @param key Configuration Item Name
 * @return Return `true` if the value isn't been set, otherwise return `false`
 */
bool DConfig::isDefaultValue(const QString &key) const
{
    D_DC(DConfig);
    qCDebug(cfLog) << "Checking isDefaultValue for key:" << key;
    if (d->invalid()) {
        qCDebug(cfLog) << "DConfig is invalid, returning false";
        return false;
    }
    const auto &isDefault = d->backend->isDefaultValue(key);
    qCDebug(cfLog) << "isDefaultValue for key" << key << ":" << isDefault;
    return isDefault;
}

/*!
@~english
 * @brief Get the corresponding value according to the configuration item name
 * @param key Configuration Item Name
 * @param fallback The default value provided after the configuration item value is not obtained
 * @return
 */
QVariant DConfig::value(const QString &key, const QVariant &fallback) const
{
    D_DC(DConfig);
    qCDebug(cfLog, "Getting value for key: %s", qPrintable(key));
    if (d->invalid()) {
        qCDebug(cfLog, "DConfig is invalid, returning fallback value");
        return fallback;
    }

    const auto &result = d->backend->value(key, fallback);
    qCDebug(cfLog) << "Value for the key:" << key << ", value : " << result;
    return result;
}

/*!
@~english
 * @brief Set the value according to the configuration item name
 * @param key Configuration Item Name
 * @param value Values that need to be updated
 */
void DConfig::setValue(const QString &key, const QVariant &value)
{
    D_D(DConfig);
    qCDebug(cfLog, "Setting value for key: %s", qPrintable(key));
    if (d->invalid()) {
        qCWarning(cfLog, "DConfig is invalid, cannot set value");
        return;
    }

    d->backend->setValue(key, value);
    qCDebug(cfLog, "Value set successfully for key: %s", qPrintable(key));
}

/*!
@~english
 * @brief Set the default value corresponding to its configuration item. This value is overridden by the override mechanism. It is not necessarily the value defined in the meta in this configuration file
 * @param key Configuration Item Name
 */
void DConfig::reset(const QString &key)
{
    D_D(DConfig);
    qCDebug(cfLog, "Resetting value for key: %s", qPrintable(key));
    if (d->invalid()) {
        qCWarning(cfLog, "DConfig is invalid, cannot reset value");
        return;
    }

    d->backend->reset(key);
    qCDebug(cfLog, "Value reset successfully for key: %s", qPrintable(key));
}

/*!
@~english
 * @brief Return configuration file name
 * @return
 */
QString DConfig::name() const
{
    D_DC(DConfig);
    qCDebug(cfLog, "Getting config name: %s", qPrintable(d->name));
    return d->name;
}

/*!
@~english
 * @brief Return the subdirectory corresponding to the configuration file
 * @return
 */
QString DConfig::subpath() const
{
    D_DC(DConfig);
    qCDebug(cfLog, "Getting config subpath: %s", qPrintable(d->subpath));
    return d->subpath;
}

DCORE_END_NAMESPACE
