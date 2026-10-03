#include "TaskRunner.h"
DbTask::DbTask(TaskFunc task, CallbackFunc callback, QPointer<QObject> receiver)
    : m_task(std::move(task))
    , m_callback(std::move(callback))
    , m_receiver(std::move(receiver))
{
    setAutoDelete(true);
}

void DbTask::run()
{
    QVariant result;
    if (m_task) {
        result = m_task();
    }

    CallbackFunc callback = std::move(m_callback);

    // m_receiver 自提交起就是弱引用，接收者若在任务排队/执行期间被销毁会自动置空
    if (m_receiver.isNull()) {
        qWarning() << "[TaskRunner] 接收者已销毁，丢弃任务结果";
        return;
    }

    QMetaObject::invokeMethod(
        m_receiver.data(),
        [receiver = m_receiver, callback, result]() {
            if (receiver && callback) {
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

