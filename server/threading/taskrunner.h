#ifndef TASKRUNNER_H
#define TASKRUNNER_H

#include <QObject>
#include <QRunnable>
#include <QThreadPool>
#include <QPointer>
#include <functional>
#include <QDebug>
#include <QVariant>
#include "dbconnectionhelper.h"

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



