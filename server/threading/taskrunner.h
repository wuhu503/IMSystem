#ifndef TASKRUNNER_H
#define TASKRUNNER_H

#include <QObject>
#include <QRunnable>
#include <QThreadPool>
#include <QPointer>
#include <functional>
#include <QDebug>
#include <QVariant>

class DbTask : public QRunnable
{
public:
    using TaskFunc = std::function<QVariant()>;
    using CallbackFunc = std::function<void(QVariant)>;

    DbTask(TaskFunc task, CallbackFunc callback, QObject *receiver)
        : m_task(std::move(task))
        , m_callback(std::move(callback))
        , m_receiver(receiver)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        QVariant result;
        if (m_task) {
            result = m_task();
        }

        QPointer<QObject> safeReceiver = m_receiver;
        CallbackFunc callback = std::move(m_callback);
        
        QMetaObject::invokeMethod(
            m_receiver,
            [safeReceiver, callback, result]() {
                if (safeReceiver && callback) {
                    callback(result);
                }
            },
            Qt::QueuedConnection
        );
    }

private:
    TaskFunc m_task;
    CallbackFunc m_callback;
    QObject *m_receiver;
};

class TaskRunner : public QObject
{
    Q_OBJECT

public:
    static TaskRunner& instance()
    {
        static TaskRunner instance;
        return instance;
    }

    TaskRunner(const TaskRunner&) = delete;
    TaskRunner& operator=(const TaskRunner&) = delete;

    template<typename Task, typename Callback>
    void runDbTask(QObject *receiver, Task&& task, Callback&& callback)
    {
        if (!receiver) {
            qWarning() << "[TaskRunner] receiver 为空，跳过任务";
            return;
        }

        auto wrappedTask = [task = std::forward<Task>(task)]() -> QVariant {
            return task();
        };

        auto wrappedCallback = [callback = std::forward<Callback>(callback)](QVariant result) {
            callback(result);
        };

        DbTask *dbTask = new DbTask(wrappedTask, wrappedCallback, receiver);
        QThreadPool::globalInstance()->start(dbTask);
    }

    int activeThreadCount() const { return QThreadPool::globalInstance()->activeThreadCount(); }
    int maxThreadCount() const { return QThreadPool::globalInstance()->maxThreadCount(); }

private:
    TaskRunner(QObject *parent = nullptr)
        : QObject(parent)
    {
        int maxThreads = QThreadPool::globalInstance()->maxThreadCount();
        QThreadPool::globalInstance()->setMaxThreadCount(qMax(maxThreads, 4));
        qInfo() << "[TaskRunner] 线程池初始化，最大线程数:" 
                << QThreadPool::globalInstance()->maxThreadCount();
    }

    ~TaskRunner() 
    {
        QThreadPool::globalInstance()->waitForDone();
    }
};

#endif // TASKRUNNER_H



