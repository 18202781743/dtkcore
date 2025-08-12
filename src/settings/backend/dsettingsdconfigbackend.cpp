// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "settings/backend/dsettingsdconfigbackend.h"

#include <QDebug>
#include <QMutex>
#include <DConfig>
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(logSettings)

class DSettingsDConfigBackendPrivate
{
public:
    explicit DSettingsDConfigBackendPrivate(DSettingsDConfigBackend *parent) : q_ptr(parent) {
        qCDebug(logSettings, "DSettingsDConfigBackendPrivate created");
    }

    DConfig       *dConfig   = nullptr;
    QMutex          writeLock;

    DSettingsDConfigBackend *q_ptr;
    Q_DECLARE_PUBLIC(DSettingsDConfigBackend)
};

/*!
@~english
  @class Dtk::Core::DSettingsDConfigBackend
  \inmodule dtkcore
  @brief Storage DSetttings to an DConfig.
 */

/*!
@~english
  @brief Save data to configure file name with DConfig.
  \a name configure file name.
  \a subpath subdirectory of configure file name.
  \a parent
 */
DSettingsDConfigBackend::DSettingsDConfigBackend(const QString &name, const QString &subpath, QObject *parent) :
    DSettingsBackend(parent), d_ptr(new DSettingsDConfigBackendPrivate(this))
{
    qCDebug(logSettings, "DSettingsDConfigBackend created with name: %s, subpath: %s", qPrintable(name), qPrintable(subpath));
    Q_D(DSettingsDConfigBackend);

    d->dConfig = new DConfig(name, subpath, this);
    qCDebug(logSettings, "DConfig created successfully");
}

DSettingsDConfigBackend::~DSettingsDConfigBackend()
{
    qCDebug(logSettings, "DSettingsDConfigBackend destroyed");
}

/*!
@~english
  @brief List all keys of DConfig
  @return
 */
QStringList DSettingsDConfigBackend::keys() const
{
    Q_D(const DSettingsDConfigBackend);
    QStringList result = d->dConfig->keyList();
    qCDebug(logSettings, "Getting keys, count: %d", result.size());
    return result;
}

/*!
@~english
  @brief Get value of key from DConfig
  \a key
  @return
 */
QVariant DSettingsDConfigBackend::getOption(const QString &key) const
{
    Q_D(const DSettingsDConfigBackend);
    QVariant result = d->dConfig->value(key);
    qCDebug(logSettings, "Getting option: %s, value: %s", qPrintable(key), qPrintable(result.toString()));
    return result;
}

/*!
@~english
  @brief Set value of key to DConfig
  \a key
  \a value
 */
void DSettingsDConfigBackend::doSetOption(const QString &key, const QVariant &value)
{
    Q_D(DSettingsDConfigBackend);
    qCDebug(logSettings, "Setting option: %s = %s", qPrintable(key), qPrintable(value.toString()));
    d->writeLock.lock();
    d->dConfig->setValue(key, value);
    d->writeLock.unlock();
}

/*!
@~english
  @brief Trigger DSettings to save option value to DConfig
 */
void DSettingsDConfigBackend::doSync()
{
    Q_D(DSettingsDConfigBackend);

    // TODO
}


DCORE_END_NAMESPACE
