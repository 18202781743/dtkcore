// SPDX-FileCopyrightText: 2016 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dsettingsgroup.h"

#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(logSettings)

class DSettingsGroupPrivate
{
public:
    DSettingsGroupPrivate(DSettingsGroup *parent) : q_ptr(parent) {
        qCDebug(logSettings) << "DSettingsGroupPrivate created";
    }

    QString key;
    QString name;
    bool    hide = false;

    QMap<QString, OptionPtr>    options;

    QPointer<DSettingsGroup>             parent;
    QMap<QString, OptionPtr>    childOptions;
    QList<QString>              childOptionKeys;

    QMap<QString, GroupPtr>     childGroups;
    QList<QString>              childGroupKeys;

    void parseJson(const QString &prefixKey, const QJsonObject &group);

    DSettingsGroup *q_ptr;
    Q_DECLARE_PUBLIC(DSettingsGroup)
};

/*!
@~english
  @class Dtk::Core::DSettingsGroup
  \inmodule dtkcore
  @brief A group of DSettingsOption and DSettingsGroup.
  DSettingsGroup can contain a lost option and subgroup.
 */


DSettingsGroup::DSettingsGroup(QObject *parent) :
    QObject(parent), dd_ptr(new DSettingsGroupPrivate(this))
{
    qCDebug(logSettings) << "DSettingsGroup created with parent";
}

DSettingsGroup::~DSettingsGroup()
{
    qCDebug(logSettings) << "DSettingsGroup destroyed";
}

/*!
@~english
  @brief Get direct parent group of this group.
  @return
 */
QPointer<DSettingsGroup> DSettingsGroup::parentGroup() const
{
    Q_D(const DSettingsGroup);
    qCDebug(logSettings) << "Getting parent group";
    return d->parent;
}

/*!
@~english
  @brief Change the direct \a parentGroup of this group.
 */
void DSettingsGroup::setParentGroup(QPointer<DSettingsGroup> parentGroup)
{
    Q_D(DSettingsGroup);
    qCDebug(logSettings) << "Setting parent group:" << parentGroup.data();
    d->parent = parentGroup;
}

/*!
@~english
  @brief Return the full key of this group, include all parent.
  @return eturn the full key of this group, include all parent.
 */
QString DSettingsGroup::key() const
{
    Q_D(const DSettingsGroup);
    qCDebug(logSettings) << "Getting key:" << d->key;
    return d->key;
}

/*!
@~english
  @brief Get display name of this group, it may be translated.
  @return Get display name of this group.
 */
QString DSettingsGroup::name() const
{
    Q_D(const DSettingsGroup);
    qCDebug(logSettings) << "Getting name:" << d->name;
    return d->name;
}

/*!
@~english
  @brief Check this group will show on DSettings dialog.
  @return true indicates that this option group will be displayed。
 */
bool DSettingsGroup::isHidden() const
{
    qCDebug(logSettings, "DSettingsGroup isHidden called");
    Q_D(const DSettingsGroup);
    bool hidden = d->hide;
    qCDebug(logSettings, "DSettingsGroup isHidden result: %s", hidden ? "true" : "false");
    return hidden;
}

/*!
@~english
  @brief Get the child group of groupKey
  \a groupKey is child group key

  @return Returns a pointer to a subgroup.
 */
QPointer<DSettingsGroup> DSettingsGroup::childGroup(const QString &groupKey) const
{
    qCDebug(logSettings, "DSettingsGroup childGroup called for key: %s", qPrintable(groupKey));
    Q_D(const DSettingsGroup);
    QPointer<DSettingsGroup> result = d->childGroups.value(groupKey);
    qCDebug(logSettings, "DSettingsGroup childGroup result: %s", result ? "found" : "not found");
    return result;
}

/*!
@~english
  @brief Get the child option of key
  \a key is child option key

  @return Returns a pointer to the child option of key.
 */
QPointer<DSettingsOption> DSettingsGroup::option(const QString &key) const
{
    qCDebug(logSettings, "DSettingsGroup option called for key: %s", qPrintable(key));
    Q_D(const DSettingsGroup);
    QPointer<DSettingsOption> result = d->childOptions.value(key);
    qCDebug(logSettings, "DSettingsGroup option result: %s", result ? "found" : "not found");
    return result;
}

/*!
@~english
  @brief Enum all direct child group of this group

  @return Returns a list of all subgroup Pointers.
 */
QList<QPointer<DSettingsGroup> > DSettingsGroup::childGroups() const
{
    qCDebug(logSettings, "DSettingsGroup childGroups called");
    Q_D(const DSettingsGroup);
    QList<QPointer<DSettingsGroup> > grouplist;
    for (auto groupKey : d->childGroupKeys) {
        grouplist << d->childGroups.value(groupKey);
    }
    qCDebug(logSettings, "DSettingsGroup childGroups returning %d groups", grouplist.size());
    return grouplist;
}

/*!
@~english
  @brief Enum all direct child option with the raw order in json description file.
  @return Returns a list of all suboption Pointers.
 */
QList<QPointer<DSettingsOption> > DSettingsGroup::childOptions() const
{
    qCDebug(logSettings, "DSettingsGroup childOptions called");
    Q_D(const DSettingsGroup);
    QList<QPointer<DSettingsOption> > optionlist;
    for (auto optionKey : d->childOptionKeys) {
        optionlist << d->childOptions.value(optionKey);
    }
    qCDebug(logSettings, "DSettingsGroup childOptions returning %d options", optionlist.size());
    return optionlist;
}

/*!
@~english
  @brief Enum all direct child option of this group.
  @return Returns a list of all option Pointers.
 */
QList<QPointer<DSettingsOption> > DSettingsGroup::options() const
{
    qCDebug(logSettings, "DSettingsGroup options called");
    Q_D(const DSettingsGroup);
    QList<QPointer<DSettingsOption> > result = d->options.values();
    qCDebug(logSettings, "DSettingsGroup options returning %d options", result.size());
    return result;
}

/*!
@~english
  @brief Convert QJsonObject to DSettingsGroup.
  \a prefixKey instead parse prefix key from parent.
  \a group is an QJsonObejct instance.
  @return Returns a group pointer after parsing json.

  @sa QPointer Dtk::Core::DSettingsOption
 */
QPointer<DSettingsGroup> DSettingsGroup::fromJson(const QString &prefixKey, const QJsonObject &group)
{
    qCDebug(logSettings, "DSettingsGroup fromJson called with prefixKey: %s", qPrintable(prefixKey));
    auto groupPtr = QPointer<DSettingsGroup>(new DSettingsGroup);
    groupPtr->parseJson(prefixKey, group);
    qCDebug(logSettings, "DSettingsGroup fromJson created group successfully");
    return groupPtr;
}

/*!
@~english
  @brief Parse QJsonObject to DSettingsGroup.
  \a prefixKey instead parse prefix key from parent.
  \a json is an QJsonObejct instance.
  @sa QPointer<DSettingsOption> Dtk::Core::DSettingsGroup::fromJson(const QString &prefixKey, const QJsonObject &json)
 */
void DSettingsGroup::parseJson(const QString &prefixKey, const QJsonObject &group)
{
    qCDebug(logSettings, "DSettingsGroup parseJson called with prefixKey: %s", qPrintable(prefixKey));
    Q_D(DSettingsGroup);
    d->parseJson(prefixKey, group);
    qCDebug(logSettings, "DSettingsGroup parseJson completed");
}

void DSettingsGroupPrivate::parseJson(const QString &prefixKey, const QJsonObject &group)
{
    qCDebug(logSettings, "DSettingsGroupPrivate parseJson called with prefixKey: %s", qPrintable(prefixKey));
    Q_Q(DSettingsGroup);
    key = group.value("key").toString();
    Q_ASSERT(!key.isEmpty());
    key = prefixKey.isEmpty() ? key : prefixKey + "." + key;
    name = group.value("name").toString();
    hide = group.value("hide").toBool();
    qCDebug(logSettings, "DSettingsGroupPrivate parseJson key: %s, name: %s, hide: %s", 
            qPrintable(key), qPrintable(name), hide ? "true" : "false");

    qCDebug(logSettings, "DSettingsGroupPrivate parsing options");
    for (auto optionJson :  group.value("options").toArray()) {
        auto optionObject = optionJson.toObject();
        auto option = DSettingsOption::fromJson(key, optionObject);
        option->setParent(q);
        options.insert(option->key(), option);
        childOptions.insert(option->key(), option);
        childOptionKeys << option->key();
        qCDebug(logSettings, "DSettingsGroupPrivate added option: %s", qPrintable(option->key()));
    }

    qCDebug(logSettings, "DSettingsGroupPrivate parsing subgroups");
    auto subGroups = group.value("groups").toArray();
    for (auto subGroup : subGroups) {
        auto child = DSettingsGroup::fromJson(key, subGroup.toObject());
        child->setParent(q);
        child->setParentGroup(q);
        childGroups.insert(child->key(), child);
        childGroupKeys << child->key();
        qCDebug(logSettings, "DSettingsGroupPrivate added subgroup: %s", qPrintable(child->key()));

        for (auto option : child->options()) {
            options.insert(option->key(), option);
            qCDebug(logSettings, "DSettingsGroupPrivate added option from subgroup: %s", qPrintable(option->key()));
        }
    }

    qCDebug(logSettings, "DSettingsGroupPrivate parseJson completed, total options: %d, total subgroups: %d", 
            options.size(), childGroups.size());
}

DCORE_END_NAMESPACE
