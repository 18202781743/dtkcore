// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dpathbuf.h"

#include <QLoggingCategory>

/*!
  \class Dtk::Core::DPathBuf
  \inmodule dtkcore
  \brief Dtk::Core::DPathBuf cat path friendly and supoort multiplatform.
  \brief Dtk::Core::DPathBuf是一个用于跨平台拼接路径的辅助类.

  它能够方便的写出链式结构的路径拼接代码。
  \code
  DPathBuf logPath(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first());
  logPath = logPath / ".cache" / "deepin" / "deepin-test-dtk" / "deepin-test-dtk.log";
  \endcode
 */

DCORE_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(logFileSystem, "dtk.core.filesystem")

/*!
  \fn DPathBuf DPathBuf::operator/(const QString &p) const
  \brief join path with operator /
  \a p is subpath
  \return a new DPathBuf with subpath p
 */

/*!
  \fn DPathBuf &DPathBuf::operator/=(const QString &p)
  \brief join path to self with operator /=
  \a p is subpath to join
  \return self object
 */

/*!
  \fn DPathBuf DPathBuf::operator/(const char *p) const
  \brief join path with operator /
  \a p is subpath
  \return a new DPathBuf with subpath p
  \sa Dtk::Core::DPathBuf::operator/(const QString &p)
 */

/*!
  \fn DPathBuf &DPathBuf::operator/=(const char *p)
  \brief join path to self with operator /=
  \a p is subpath to join
  \return self object
  \sa operator/=(const QString &p)
 */

/*!
  \fn DPathBuf &DPathBuf::join(const QString &p)
  \brief join add subpath p to self
  \a p is subpath to join
  \return slef object with subpath joined
 */

/*!
  \fn QString DPathBuf::toString() const
  \brief toString export native separators format string.
  \return string with native separators
 */

/*!
  \brief Create Dtk::Core::DPathBuf from string.
  \a path
 */
DPathBuf::DPathBuf(const QString &path)
{
    qCDebug(logFileSystem, "Creating DPathBuf with path: %s", qPrintable(path));
    m_path = QDir(path).absolutePath();
    qCDebug(logFileSystem, "Absolute path: %s", qPrintable(m_path));
}

DPathBuf::DPathBuf()
    : DPathBuf(QString())
{
    qCDebug(logFileSystem, "Creating empty DPathBuf");
}

DCORE_END_NAMESPACE
