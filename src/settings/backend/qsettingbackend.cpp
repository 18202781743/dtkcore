// SPDX-FileCopyrightText: 2016 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "settings/backend/qsettingbackend.h"

#include <QDebug>
#include <QMutex>
#include <QSettings>
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(logSettings)

class QSettingBackendPrivate
{
public:
    QSettingBackendPrivate(QSettingBackend *parent) : q_ptr(parent) {
        qCDebug(logSettings, "QSettingBackendPrivate created");
    }

    QSettings       *settings   = nullptr;
    QMutex          writeLock;

    QSettingBackend *q_ptr;
    Q_DECLARE_PUBLIC(QSettingBackend)
};

/*!
@~english
  @class Dtk::Core::QSettingBackend
  \inmodule dtkcore
  @brief Storage DSetttings to an QSettings.
 */

/*!
@~english
  @brief Save data to filepath with QSettings::NativeFormat format.
  \a filepath is path to storage data.
  \a parent
 */
QSettingBackend::QSettingBackend(const QString &filepath, QObject *parent) :
    DSettingsBackend(parent), d_ptr(new QSettingBackendPrivate(this))
{
    qCDebug(logSettings, "QSettingBackend created with filepath: %s", qPrintable(filepath));
    Q_D(QSettingBackend);

    d->settings = new QSettings(filepath, QSettings::NativeFormat, this);
    qCDebug(logSettings, "QSettings created with filename: %s", qPrintable(d->settings->fileName()));
    qDebug() << "create config" <<  d->settings->fileName();
}

QSettingBackend::~QSettingBackend()
{
    qCDebug(logSettings, "QSettingBackend destroyed");
}

/*!
@~english
  @brief List all keys of QSettings
  @return
 */
QStringList QSettingBackend::keys() const
{
    Q_D(const QSettingBackend);
    QStringList result = d->settings->childGroups();
    qCDebug(logSettings, "Getting QSettings keys, count: %d", result.size());
    return result;
}

/*!
@~english
  @brief Get value of key from QSettings
  \a key
  @return
 */
QVariant QSettingBackend::getOption(const QString &key) const
{
    Q_D(const QSettingBackend);
    qCDebug(logSettings, "Getting QSettings option: %s", qPrintable(key));
    d->settings->beginGroup(key);
    auto value = d->settings->value("value");
    d->settings->endGroup();
    qCDebug(logSettings, "QSettings option value: %s", qPrintable(value.toString()));
    return value;
}

/*!
@~english
  @brief Set value of key to QSettings
  \a key
  \a value
 */
void QSettingBackend::doSetOption(const QString &key, const QVariant &value)
{
    Q_D(QSettingBackend);
    qCDebug(logSettings, "Setting QSettings option: %s = %s", qPrintable(key), qPrintable(value.toString()));
    d->writeLock.lock();
    d->settings->beginGroup(key);
    d->settings->setValue("value", value);
    d->settings->endGroup();
    d->writeLock.unlock();
    qCDebug(logSettings, "QSettings option set successfully");
}

/*!
@~english
  @brief Sync data to QSettings
 */
void QSettingBackend::doSync()
{
    Q_D(QSettingBackend);
    qCDebug(logSettings, "Syncing QSettings data");
    d->writeLock.lock();
    d->settings->sync();
    d->writeLock.unlock();
    qCDebug(logSettings, "QSettings sync completed");
}


DCORE_END_NAMESPACE
