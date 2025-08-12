// SPDX-FileCopyrightText: 2016 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dsettings.h"

#include <QMap>
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QThread>
#include <QDebug>
#include <QLoggingCategory>

#include "dsettingsoption.h"
#include "dsettingsgroup.h"
#include "dsettingsbackend.h"

DCORE_BEGIN_NAMESPACE

#ifdef QT_DEBUG
Q_LOGGING_CATEGORY(logSettings, "dtk.core.settings")
#else
Q_LOGGING_CATEGORY(logSettings, "dtk.core.settings", QtInfoMsg)
#endif

class DSettingsPrivate
{
public:
    DSettingsPrivate(DSettings *parent) : q_ptr(parent) {
        qCDebug(logSettings, "DSettingsPrivate created");
    }

    DSettingsBackend            *backend = nullptr;
    QJsonObject                 meta;
    QMap <QString, OptionPtr>   options;

    QMap<QString, GroupPtr>     childGroups;
    QList<QString>              childGroupKeys;

    DSettings *q_ptr;
    Q_DECLARE_PUBLIC(DSettings)
};


/*!
@~english
   @class Dtk::Core::DSettingsBackend
   \inmodule dtkcore
   @brief DSettingsBackend is interface of DSettings storage class.

   Simaple example:

@code
{
    "groups": [{
        "key": "base",
        "name": "Basic settings",
        "groups": [{
                "key": "open_action",
                "name": "Open Action",
                "options": [{
                        "key": "alway_open_on_new",
                        "type": "checkbox",
                        "text": "Always Open On New Windows",
                        "default": true
                    },
                    {
                        "key": "open_file_action",
                        "name": "Open File:",
                        "type": "combobox",
                        "default": ""
                    }
                ]
            },
            {
                "key": "new_tab_windows",
                "name": "New Tab & Window",
                "options": [{
                        "key": "new_window_path",
                        "name": "New Window Open:",
                        "type": "combobox",
                        "default": ""
                    },
                    {
                        "key": "new_tab_path",
                        "name": "New Tab Open:",
                        "type": "combobox",
                        "default": ""
                    }
                ]
            }
        ]
    }]
}
@endcode

    How to read/write key and value:

    @code
    // init a storage backend
    QTemporaryFile tmpFile;
    tmpFile.open();
    auto backend = new Dtk::Core::QSettingBackend(tmpFile.fileName());

    // read settings from json
    auto settings = Dtk::Core::DSettings::fromJsonFile(":/resources/data/dfm-settings.json");
    settings->setBackend(backend);

    // read value
    auto opt = settings->option("base.new_tab_windows.new_window_path");
    qDebug() << opt->value();

    // modify value
    opt->setValue("Test")
    qDebug() << opt->value();
   @endcode
   @sa Dtk::Core::DSettingsOption
   @sa Dtk::Core::DSettingsGroup
   @sa Dtk::Core::DSettingsBackend
   @sa Dtk::Widget::DSettingsWidgetFactory
   @sa Dtk::Widget::DSettingsDialog
 */

/*!
@~english
  @fn virtual QStringList DSettingsBackend::keys() const = 0;
  @brief return all key of storage.
 */
/*!
@~english
  @fn virtual QVariant DSettingsBackend::getOption(const QString &key) const = 0;
  @brief get value by \a key.
 */
/*!
@~english
  @fn virtual void DSettingsBackend::doSync() = 0;
  @brief do the real sync action.
 */
/*!
@~english
  @fn virtual void DSettingsBackend::doSetOption(const QString &key, const QVariant &value) = 0;
  @brief write \a key / \a value to storage.
 */
/*!
@~english
  @fn void DSettingsBackend::optionChanged(const QString &key, const QVariant &value);
  @brief emitted when option \a value changed.
 */
/*!
@~english
  @fn void DSettingsBackend::sync();
  @brief private signal, please do not use it.
 */
/*!
@~english
  @fn void DSettingsBackend::setOption(const QString &key, const QVariant &value);
  @brief private signal, please do not use it.

  \internal
  \a key \a value
 */

/*!
@~english
 @class Dtk::Core::DSettings
 \inmodule dtkcore
 @brief DSettings is the basic library that provides unified configuration storage and interface generation tools for Dtk applications.

 DSetting uses json as the description file for the application configuration. The configuration of application query is divided into two basic levels: group / key value.
 The standard Dtk configuration control generally contains only three levels of group / subgroup / key values. For keys with more than three levels, you can use the.
 DSettings's API interface reads and writes, but cannot be displayed on standard DSettingsDialogs.

 
 Simple configuration file：
@code
{
    "groups": [{
        "key": "base",
        "name": "Basic settings",
        "groups": [{
                "key": "open_action",
                "name": "Open Action",
                "options": [{
                        "key": "alway_open_on_new",
                        "type": "checkbox",
                        "text": "Always Open On New Windows",
                        "default": true
                    },
                    {
                        "key": "open_file_action",
                        "name": "Open File:",
                        "type": "combobox",
                        "default": ""
                    }
                ]
            },
            {
                "key": "new_tab_windows",
                "name": "New Tab & Window",
                "options": [{
                        "key": "new_window_path",
                        "name": "New Window Open:",
                        "type": "combobox",
                        "default": ""
                    },
                    {
                        "key": "new_tab_path",
                        "name": "New Tab Open:",
                        "type": "combobox",
                        "default": ""
                    }
                ]
            }
        ]
    }]
}
@endcode

The group contains a root group of base with two subgroups: open_action/new_tab_windows, each of which contains several options.
The complete access id for the configuration "New Window Open:" is base.new_tab_windows.new_window_path.

How to read/write key and value:
@code
    // init a storage backend
    QTemporaryFile tmpFile;
    tmpFile.open();
    auto backend = new Dtk::Core::QSettingBackend(tmpFile.fileName());

    // read settings from json
    auto settings = Dtk::Core::DSettings::fromJsonFile(":/resources/data/dfm-settings.json");
    settings->setBackend(backend);

    // read value
    auto opt = settings->option("base.new_tab_windows.new_window_path");
    qDebug() << opt->value();

    // modify value
    opt->setValue("Test")
    qDebug() << opt->value();
@endcode
@sa Dtk::Core::DSettingsOption
@sa Dtk::Core::DSettingsGroup
@sa Dtk::Core::DSettingsBackend
@sa Dtk::Widget::DSettingsWidgetFactory
@sa Dtk::Widget::DSettingsDialog
*/

DSettings::DSettings(QObject *parent) :
    QObject(parent), dd_ptr(new DSettingsPrivate(this))
{
    qCDebug(logSettings, "DSettings created with parent");
}

DSettings::~DSettings()
{
    qCDebug(logSettings, "DSettings destroyed");
}

void DSettings::setBackend(DSettingsBackend *backend)
{
    qCDebug(logSettings, "Setting backend: %p", backend);
    Q_D(DSettings);
    if (nullptr == backend) {
        qCWarning(logSettings, "Backend is null, ignoring setBackend call");
        return;
    }

    if (d->backend != nullptr) {
        qCWarning(logSettings, "set backend to exist %p", d->backend);
    }

    d->backend = backend;

    auto backendWriteThread = new QThread;
    d->backend->moveToThread(backendWriteThread);

    connect(d->backend, &DSettingsBackend::optionChanged,
    this, [ = ](const QString & key, const QVariant & value) {
        qCDebug(logSettings, "Backend option changed: %s = %s", qPrintable(key), qPrintable(value.toString()));
        option(key)->setValue(value);
    });
    // exit and delete thread
    connect(this, &DSettings::destroyed, this, [backendWriteThread](){
        qCDebug(logSettings, "DSettings destroyed, cleaning up backend thread");
        if (backendWriteThread->isRunning()) {
            backendWriteThread->quit();
            backendWriteThread->wait();
        }
        backendWriteThread->deleteLater();
    });

    backendWriteThread->start();
    qCDebug(logSettings, "Backend thread started");

    // load form backend
    loadValue();
}

/*!
@~english
   @brief Get DSettings from \a json. The returned data needs to be manually released after use.
 */
QPointer<DSettings> DSettings::fromJson(const QByteArray &json)
{
    qCDebug(logSettings, "Creating DSettings from JSON, size: %d", json.size());
    auto settingsPtr = QPointer<DSettings>(new DSettings);
    settingsPtr->parseJson(json);
    return settingsPtr;
}

QPointer<DSettings> DSettings::fromJsonFile(const QString &filepath)
{
    qCDebug(logSettings, "Creating DSettings from JSON file: %s", qPrintable(filepath));
    QFile jsonFile(filepath);
    jsonFile.open(QIODevice::ReadOnly);
    auto jsonData = jsonFile.readAll();
    jsonFile.close();

    return DSettings::fromJson(jsonData);
}

QJsonObject DSettings::meta() const
{
    Q_D(const DSettings);
    return d->meta;
}

QStringList DSettings::keys() const
{
    Q_D(const DSettings);
    qCDebug(logSettings, "Getting keys, count: %d", d->options.size());
    return d->options.keys();
}

QPointer<DSettingsOption> DSettings::option(const QString &key) const
{
    Q_D(const DSettings);
    qCDebug(logSettings, "Getting option for key: %s", qPrintable(key));
    return d->options.value(key);
}

QVariant DSettings::value(const QString &key) const
{
    Q_D(const DSettings);
    qCDebug(logSettings, "Getting value for key: %s", qPrintable(key));
    auto opt = d->options.value(key);
    if (opt.isNull()) {
        qCWarning(logSettings, "Option not found for key: %s", qPrintable(key));
        return QVariant();
    }

    return opt->value();
}

QStringList DSettings::groupKeys() const
{
    Q_D(const DSettings);
    qCDebug(logSettings, "Getting group keys, count: %d", d->childGroupKeys.size());
    return d->childGroupKeys;
}

QList<QPointer<DSettingsGroup> > DSettings::groups() const
{
    Q_D(const DSettings);
    qCDebug(logSettings, "Getting groups, count: %d", d->childGroups.size());
    return d->childGroups.values();
}
/*!
@~english
  @brief DSettings::group will recurrence find childGroup
  \a key
  @return
 */
QPointer<DSettingsGroup> DSettings::group(const QString &key) const
{
    Q_D(const DSettings);
    qCDebug(logSettings, "Getting group for key: %s", qPrintable(key));
    auto childKeylist = key.split(".");
    if (0 >= childKeylist.length()) {
        qCWarning(logSettings, "Invalid key format: %s", qPrintable(key));
        return nullptr;
    }

    auto mainGroupKey = childKeylist.value(0);
    if (1 >= childKeylist.length()) {
        return d->childGroups.value(mainGroupKey);
    }

    return d->childGroups.value(mainGroupKey)->childGroup(key);
}

QList<QPointer<DSettingsOption> > DSettings::options() const
{
    Q_D(const DSettings);
    qCDebug(logSettings, "Getting all options, count: %d", d->options.size());
    return d->options.values();
}

QVariant DSettings::getOption(const QString &key) const
{
    qCDebug(logSettings, "Getting option value for key: %s", qPrintable(key));
    QPointer<DSettingsOption> optionPointer = option(key);
    if (optionPointer) {
        return optionPointer->value();
    }
    qCWarning(logSettings, "Option not found for key: %s", qPrintable(key));
    return QVariant();
}

void DSettings::setOption(const QString &key, const QVariant &value)
{
    qCDebug(logSettings, "Setting option: %s = %s", qPrintable(key), qPrintable(value.toString()));
    option(key)->setValue(value);
}

void DSettings::sync()
{
    Q_D(DSettings);
    qCDebug(logSettings, "Syncing settings");
    if (!d->backend) {
        qCWarning(logSettings, "backend was not setted..!");
        return;
    }

    d->backend->doSync();
}

void DSettings::reset()
{
    Q_D(DSettings);
    qCDebug(logSettings, "Resetting settings");

    for (auto option : d->options) {
        if (option->canReset()) {
            qCDebug(logSettings, "Resetting option: %s", qPrintable(option->key()));
            setOption(option->key(), option->defaultValue());
        }
    }

    if (!d->backend) {
        qCWarning(logSettings, "backend was not setted..!");
        return;
    }

    d->backend->sync();
}

void DSettings::parseJson(const QByteArray &json)
{
    Q_D(DSettings);
    qCDebug(logSettings, "Parsing JSON, size: %d", json.size());

    auto jsonDoc = QJsonDocument::fromJson(json);
    d->meta = jsonDoc.object();
    auto mainGroups = d->meta.value("groups");
    qCDebug(logSettings, "Found %d main groups", mainGroups.toArray().size());
    
    for (auto groupJson : mainGroups.toArray()) {
        auto group = DSettingsGroup::fromJson("", groupJson.toObject());
        group->setParent(this);
        for (auto option : group->options()) {
            d->options.insert(option->key(), option);
        }
        d->childGroupKeys << group->key();
        d->childGroups.insert(group->key(), group);
    }

    for (auto option :  d->options.values()) {
        d->options.insert(option->key(), option);
        connect(option.data(), &DSettingsOption::valueChanged,
        this, [ = ](QVariant value) {
            qCDebug(logSettings, "Option value changed: %s = %s", qPrintable(option->key()), qPrintable(value.toString()));
            if (d->backend) {
                Q_EMIT d->backend->setOption(option->key(), value);
            } else {
                qCWarning(logSettings, "backend was not setted..!");
            }
            Q_EMIT valueChanged(option->key(), value);
        });
    }
}

void DSettings::loadValue()
{
    Q_D(DSettings);
    qCDebug(logSettings, "Loading values from backend");
    if (!d->backend) {
        qCWarning(logSettings, "backend was not setted..!");
        return;
    }

    for (auto key : d->backend->keys()) {
        auto value = d->backend->getOption(key);
        auto opt = option(key);
        if (!value.isValid() || opt.isNull()) {
            qCDebug(logSettings, "Skipping invalid option: %s", qPrintable(key));
            continue;
        }

        qCDebug(logSettings, "Loading option: %s = %s", qPrintable(key), qPrintable(value.toString()));
        opt->blockSignals(true);
        opt->setValue(value);
        opt->blockSignals(false);
    }
}

DCORE_END_NAMESPACE
