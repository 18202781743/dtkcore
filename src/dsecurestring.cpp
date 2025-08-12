// SPDX-FileCopyrightText: 2019 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dsecurestring.h"
#include "dutil.h"
#include <cstring>
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(logUtil, "dtk.core.util")

DSecureString::DSecureString(const QString &other) noexcept
    : QString(other)
{
    qCDebug(logUtil) << "DSecureString constructor called with string length:" << other.length();
}

DSecureString::~DSecureString()
{
    qCDebug(logUtil) << "DSecureString destructor called, performing secure erase";
    DUtil::SecureErase(*this);
    qCDebug(logUtil) << "DSecureString secure erase completed";
}

DCORE_END_NAMESPACE
