// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dtkcore_global.h"
#include <QDebug>
#include <QFileInfo>
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(logUtil, "dtk.core.util")

#if (!defined DTK_VERSION) || (!defined DTK_VERSION_STR)
#error "DTK_VERSION or DTK_VERSION_STR not defined!"
#endif

int dtkVersion()
{
    qCDebug(logUtil) << "dtkVersion called";
    qCDebug(logUtil) << "dtkVersion returning:" << DTK_VERSION;
    return DTK_VERSION;
}

const char *dtkVersionString()
{
    qCDebug(logUtil) << "dtkVersionString called";
#ifdef QT_DEBUG
    qWarning() << "Use DTK_VERSION_STR instead.";
#endif
    qCDebug(logUtil) << "dtkVersionString returning empty string";
    return "";  // DTK_VERSION_STR;
}
