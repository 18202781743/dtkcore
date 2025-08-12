// SPDX-FileCopyrightText: 2020 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "dthreadutils.h"

#include <QLoggingCategory>

DCORE_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(logUtil, "dtk.core.util")

#if DTK_VERSION < DTK_VERSION_CHECK(6, 0, 0, 0)
namespace DThreadUtil {
FunctionCallProxy::FunctionCallProxy(QThread *thread)
{
    qCDebug(logUtil) << "Creating FunctionCallProxy for thread:" << thread;
    qRegisterMetaType<QPointer<QObject>>();

    connect(this, &FunctionCallProxy::callInLiveThread, this, [] (QSemaphore *s, QPointer<QObject> target, FunctionType *func) {
        if (Q_LIKELY(target)) {
            qCDebug(logUtil) << "Executing function in target thread";
            (*func)();
        } else {
            qCWarning(logUtil) << "The target object is destroyed";
        }

        s->release();
    }, Qt::QueuedConnection);
    connect(thread, &QThread::finished, this, [this] {
        qCWarning(logUtil) << "Thread finished:" << sender();
    }, Qt::DirectConnection);
}

void FunctionCallProxy::proxyCall(QSemaphore *s, QThread *thread, QObject *target, FunctionType fun)
{
    qCDebug(logUtil) << "Proxy call: current thread=" << QThread::currentThread() << "target thread=" << thread << "target object=" << target;
    
    if (QThread::currentThread() == thread) {
        qCDebug(logUtil) << "Already in target thread, executing function directly";
        return fun();
    }

    FunctionCallProxy proxy(thread);
    proxy.moveToThread(thread);

    // 如果线程未开启事件循环，且不是主线程，则需要给出严重警告信息，因为可能会导致死锁
    if (thread->loopLevel() <= 0 && (!QCoreApplication::instance() || thread != QCoreApplication::instance()->thread())) {
        qCCritical(logUtil, "Thread %p has no event loop", thread);
    }

    qCDebug(logUtil) << "Calling function in target thread";
    proxy.callInLiveThread(s, target ? target : &proxy, &fun);
    s->acquire();
    qCDebug(logUtil) << "Function call completed";
}

}
#else
class Q_DECL_HIDDEN Caller : public QObject
{
public:
    explicit Caller()
        : QObject()
    {
        qCDebug(logUtil) << "Creating Caller object";
    }

    bool event(QEvent *event) override
    {
        if (event->type() == DThreadUtils::eventType) {
            qCDebug(logUtil) << "Handling DThreadUtils event";
            auto ev = static_cast<DThreadUtils::AbstractCallEvent *>(event);
            ev->call();
            return true;
        }

        return QObject::event(event);
    }
};

DThreadUtils::DThreadUtils(QThread *thread)
    : m_thread(thread)
    , threadContext(nullptr)
{
    qCDebug(logUtil) << "Creating DThreadUtils for thread:" << thread;
}

DThreadUtils::~DThreadUtils()
{
    qCDebug(logUtil) << "Destroying DThreadUtils";
    delete threadContext.loadRelaxed();
}

DThreadUtils &DThreadUtils::gui()
{
    qCDebug(logUtil) << "Getting GUI thread utils";
    static auto global = DThreadUtils(QCoreApplication::instance()->thread());
    return global;
}

QThread *DThreadUtils::thread() const noexcept
{
    qCDebug(logUtil) << "Getting thread:" << m_thread;
    return m_thread;
}

QObject *DThreadUtils::ensureThreadContextObject()
{
    qCDebug(logUtil) << "Ensuring thread context object for thread:" << m_thread;
    QObject *context;
    if (!threadContext.loadRelaxed()) {
        qCDebug(logUtil) << "Creating new thread context object";
        context = new Caller();
        context->moveToThread(m_thread);
        if (!threadContext.testAndSetRelaxed(nullptr, context)) {
            qCDebug(logUtil) << "Thread context already set by another thread, cleaning up";
            context->moveToThread(nullptr);
            delete context;
        }
    } else {
        qCDebug(logUtil) << "Using existing thread context object";
    }

    context = threadContext.loadRelaxed();
    Q_ASSERT(context);

    return context;
}
#endif
DCORE_END_NAMESPACE
