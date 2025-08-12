// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "settings/backend/gsettingsbackend.h"

//#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariant>
#include <QLoggingCategory>

#if QT_HAS_INCLUDE(<QGSettings/QGSettings>)
#include <QGSettings/QGSettings>
#else
#include <QGSettings>
#endif

#include <DSettings>

DCORE_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(logSettings)

QString unqtifyName(const QString &name)
{
    qCDebug(logSettings, "Unqtifying name: %s", qPrintable(name));
    QString ret;
    for (auto p : name) {
        const QChar c(p);
        if (c.isUpper()) {
            ret.append("-");
            ret.append(c.toLower().toLatin1());
        } else {
            ret.append(c);
        }
    }
    qCDebug(logSettings, "Unqtified result: %s", qPrintable(ret));
    return ret;
}

QString qtifyName(const QString &key)
{
    qCDebug(logSettings, "Qtifying key: %s", qPrintable(key));
    QString result = QString(key).replace(".", "-").replace("_", "-");
    qCDebug(logSettings, "Qtified result: %s", qPrintable(result));
    return result;
}


class GSettingsBackendPrivate
{
public:
    GSettingsBackendPrivate(GSettingsBackend *parent) : q_ptr(parent) {
        qCDebug(logSettings, "GSettingsBackendPrivate created");
    }

    QGSettings *gsettings;
    QMap<QString, QString> keyMap;

    GSettingsBackend *q_ptr;
    Q_DECLARE_PUBLIC(GSettingsBackend)
};

/*!
@~english
  @class Dtk::Core::GSettingsBackend
  \inmodule dtkcore
  @brief Storage backend of DSettings use gsettings.
  
  You should generate gsetting schema with /usr/lib/x86_64-linux-gnu/libdtk-$$VERSION/DCore/bin/dtk-settings.
  
  You can find this tool from libdtkcore-bin. use /usr/lib/x86_64-linux-gnu/libdtk-$$VERSION/DCore/bin/dtk-settings -h for help.
 */

GSettingsBackend::GSettingsBackend(DSettings *settings, QObject *parent) :
    DSettingsBackend(parent), d_ptr(new GSettingsBackendPrivate(this))
{
    qCDebug(logSettings, "GSettingsBackend created");
    Q_D(GSettingsBackend);

    QJsonObject settingsMeta = settings->meta();
    auto gsettingsMeta = settingsMeta.value("gsettings").toObject();
    auto id = gsettingsMeta.value("id").toString();
    auto path = gsettingsMeta.value("path").toString();

    qCDebug(logSettings, "GSettings ID: %s, path: %s", qPrintable(id), qPrintable(path));

    for (QString key : settings->keys()) {
        auto gsettingsKey = QString(key).replace(".", "-").replace("_", "-");
        d->keyMap.insert(gsettingsKey, key);
        qCDebug(logSettings, "Mapped key: %s -> %s", qPrintable(gsettingsKey), qPrintable(key));
    }

    d->gsettings = new QGSettings(id.toUtf8(), path.toUtf8(), this);
    qCDebug(logSettings, "QGSettings created successfully");

    connect(d->gsettings, &QGSettings::changed, this, [ = ](const QString & key) {
        auto dk = d->keyMap.value(unqtifyName(key));
        qCDebug(logSettings, "GSettings key changed: %s, mapped to: %s", qPrintable(key), qPrintable(dk));
        if (!dk.isEmpty()) {
            Q_EMIT optionChanged(dk, getOption(dk));
        }
    });
}

GSettingsBackend::~GSettingsBackend()
{
    qCDebug(logSettings, "GSettingsBackend destroyed");
}

/*!
@~english
  @brief List all gsettings keys.
  @return Return all gsettings keys.
 */
QStringList GSettingsBackend::keys() const
{
    Q_D(const GSettingsBackend);
    QStringList result = d->gsettings->keys();
    qCDebug(logSettings, "Getting GSettings keys, count: %d", result.size());
    return result;
}

/*!
@~english
  @brief Get value of key.
  @return Return the value of the given \a key.
 */
QVariant GSettingsBackend::getOption(const QString &key) const
{
    Q_D(const GSettingsBackend);
    QString gsettingsKey = qtifyName(key);
    QVariant result = d->gsettings->get(gsettingsKey);
    qCDebug(logSettings, "Getting GSettings option: %s (key: %s), value: %s", 
            qPrintable(key), qPrintable(gsettingsKey), qPrintable(result.toString()));
    return result;
}

/*!
@~english
  @brief Set value to gsettings
  Use the \a key to save the \a value.
 */
void GSettingsBackend::doSetOption(const QString &key, const QVariant &value)
{
    Q_D(GSettingsBackend);
    QString gsettingsKey = qtifyName(key);
    qCDebug(logSettings, "Setting GSettings option: %s (key: %s) = %s", 
            qPrintable(key), qPrintable(gsettingsKey), qPrintable(value.toString()));
    d->gsettings->set(gsettingsKey, value);
}

/*!
@~english
  @brief Trigger DSettings to sync option to storage.
 */
void GSettingsBackend::doSync()
{
    qCDebug(logSettings, "Triggering GSettings sync");
    Q_EMIT sync();
}

DCORE_END_NAMESPACE
