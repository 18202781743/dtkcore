// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dtimedloop.h"
#include <DObject>
#include <DObjectPrivate>
#include <dthreadutils.h>

#include <QTime>
#include <QTimer>
#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

#ifdef QT_DEBUG
Q_LOGGING_CATEGORY(logTimedLoop, "dtk.dtimedloop")
#else
Q_LOGGING_CATEGORY(logTimedLoop, "dtk.dtimedloop", QtInfoMsg)
#endif

class DTimedLoopPrivate : public DObjectPrivate
{
    D_DECLARE_PUBLIC(DTimedLoop)
public:
    DTimedLoopPrivate(DTimedLoop *qq = nullptr);
    ~DTimedLoopPrivate();

    int m_returnCode = 0;
    QTime m_startTime;
    QTime m_stopTime;
    bool m_timeDumpFlag = false;
    char __padding[3];
    QString m_exectionName;

    void setExecutionName(const QString &executionName);

    class LoopGuard {
        DTimedLoopPrivate *m_p = nullptr;

    public:
        LoopGuard(DTimedLoopPrivate *p)
            : m_p (p)
        {
            m_p->m_startTime = QTime::currentTime();
        }
        ~LoopGuard() {
            m_p->m_stopTime = QTime::currentTime();
            if (!m_p->m_timeDumpFlag) {
                return;
            }
            if (Q_UNLIKELY(m_p->m_exectionName.isEmpty())) {
                qCDebug(logTimedLoop(),
                        "The execution time is %-5d ms",
                        m_p->m_startTime.msecsTo(QTime::currentTime()));
            } else {
                qCDebug(logTimedLoop(),
                        "The execution time is %-5d ms for \"%s\"",
                        m_p->m_startTime.msecsTo(QTime::currentTime()),
                        m_p->m_exectionName.toLocal8Bit().data());

                m_p->m_exectionName.clear();
            }
        }
    };
};

DTimedLoopPrivate::DTimedLoopPrivate(DTimedLoop *qq)
    : DObjectPrivate (qq)
{
    qCDebug(logTimedLoop, "DTimedLoopPrivate created");
}

DTimedLoopPrivate::~DTimedLoopPrivate()
{
    qCDebug(logTimedLoop, "DTimedLoopPrivate destroyed");
}

void DTimedLoopPrivate::setExecutionName(const QString &executionName)
{
    qCDebug(logTimedLoop, "Setting execution name: %s", qPrintable(executionName));
    m_exectionName = executionName;
}

DTimedLoop::DTimedLoop(QObject *parent) noexcept
    : QEventLoop (parent)
    , DObject (*new DTimedLoopPrivate(this))
{
    qCDebug(logTimedLoop, "DTimedLoop created with parent");
}

DTimedLoop::DTimedLoop() noexcept
    : QEventLoop ()
    , DObject (*new DTimedLoopPrivate(this))
{
    qCDebug(logTimedLoop, "DTimedLoop created without parent");
}

DTimedLoop::~DTimedLoop()
{
    qCDebug(logTimedLoop, "DTimedLoop destroyed");
}

void DTimedLoop::setTimeDump(bool flag)
{
    qCDebug(logTimedLoop, "Setting time dump flag: %s", flag ? "true" : "false");
    D_D(DTimedLoop);
    d->m_timeDumpFlag = flag;
}

void DTimedLoop::exit(int returnCode)
{
    qCDebug(logTimedLoop, "Exiting with return code: %d", returnCode);
    // 避免在子线程中提前被执行
    DThreadUtil::runInMainThread([this, returnCode]{
        qCDebug(logTimedLoop, "Executing exit in main thread with return code: %d", returnCode);
        QEventLoop::exit(returnCode);
    });
}

int DTimedLoop::exec(QEventLoop::ProcessEventsFlags flags)
{
    qCDebug(logTimedLoop, "Executing with flags: %d", flags);
    D_D(DTimedLoop);
    DTimedLoopPrivate::LoopGuard guard(d);
    return QEventLoop::exec(flags);
}

int DTimedLoop::exec(int durationTimeMs, QEventLoop::ProcessEventsFlags flags)
{
    qCDebug(logTimedLoop, "Executing for duration: %d ms with flags: %d", durationTimeMs, flags);
    Q_D(DTimedLoop);
    int runningTime = durationTimeMs < 0 ? 0 : durationTimeMs;
    qCDebug(logTimedLoop, "Setting timer for %d ms", runningTime);
    QTimer::singleShot(runningTime, [this] {
        qCDebug(logTimedLoop, "Timer expired, exiting event loop");
        QEventLoop::exit(0);
    });
    DTimedLoopPrivate::LoopGuard guard(d);
    return QEventLoop::exec(flags);
}

int DTimedLoop::exec(const QString &executionName, QEventLoop::ProcessEventsFlags flags)
{
    qCDebug(logTimedLoop, "Executing with name: %s, flags: %d", qPrintable(executionName), flags);
    Q_D(DTimedLoop);
    d->setExecutionName(executionName);
    qCDebug(logTimedLoop, "Calling exec with flags: %d", flags);
    return exec(flags);
}

int DTimedLoop::exec(int durationMs, const QString &executionName, QEventLoop::ProcessEventsFlags flags)
{
    qCDebug(logTimedLoop, "Executing for duration: %d ms with name: %s, flags: %d", durationMs, qPrintable(executionName), flags);
    Q_D(DTimedLoop);
    d->setExecutionName(executionName);
    qCDebug(logTimedLoop, "Calling exec with duration: %d ms", durationMs);
    return exec(durationMs, flags);
}

int DTimedLoop::runningTime() {
    qCDebug(logTimedLoop, "Getting running time");
    Q_D(DTimedLoop);
    if (QEventLoop::isRunning()) {
        int time = d->m_startTime.msecsTo(QTime::currentTime());
        qCDebug(logTimedLoop, "Loop is running, time: %d ms", time);
        return time;
    }
    int time = d->m_startTime.msecsTo(d->m_stopTime);
    qCDebug(logTimedLoop, "Loop is not running, time: %d ms", time);
    return time;
}

DCORE_END_NAMESPACE
