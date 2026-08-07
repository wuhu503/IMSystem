#include "taskrunner.h"
#include "dbconnectionhelper.h"

DbTask::DbTask(TaskFunc task, CallbackFunc callback, QObject *receiver)
    : m_task(std::move(task))
    , m_callback(std::move(callback))
    , m_receiver(receiver)
{
    setAutoDelete(true);
}

void DbTask::run()
{
    QVariant result;
    if (m_task) {
        result = m_task();
    }

    // 任务执行完毕，清理线程本地数据库连接，避免连接只建不清理
    DbConnectionHelper::cleanupCurrentThread();

    QPointer<QObject> safeReceiver = m_receiver;
    CallbackFunc callback = std::move(m_callback);

    // 接收者可能已被销毁，投递前判空，避免悬垂指针
    if (!safeReceiver) {
        qWarning() << "[TaskRunner] 接收者已销毁，丢弃任务结果";
        return;
    }

    QMetaObject::invokeMethod(
        safeReceiver.data(),
        [safeReceiver, callback, result]() {
            if (safeReceiver && callback) {
                callback(result);
            }
        },
        Qt::QueuedConnection
    );
}

TaskRunner& TaskRunner::instance()
{
    static TaskRunner instance;
    return instance;
}

TaskRunner::TaskRunner(QObject *parent)
    : QObject(parent)
{
    int maxThreads = QThreadPool::globalInstance()->maxThreadCount();
    QThreadPool::globalInstance()->setMaxThreadCount(qMax(maxThreads, 4));
    qInfo() << "[TaskRunner] 线程池初始化，最大线程数:"
            << QThreadPool::globalInstance()->maxThreadCount();
}

TaskRunner::~TaskRunner()
{
    QThreadPool::globalInstance()->waitForDone();
}

int TaskRunner::activeThreadCount() const
{
    return QThreadPool::globalInstance()->activeThreadCount();
}

int TaskRunner::maxThreadCount() const
{
    return QThreadPool::globalInstance()->maxThreadCount();
}
