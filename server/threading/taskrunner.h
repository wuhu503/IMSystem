#ifndef TASKRUNNER_H
#define TASKRUNNER_H

#include <QObject>
#include <QRunnable>
#include <QThreadPool>
#include <QPointer>
#include <QDebug>
#include <QVariant>
#include <functional>
#include <utility>

class DbTask : public QRunnable
{
public:
    using TaskFunc = std::function<QVariant()>;
    using CallbackFunc = std::function<void(QVariant)>;

    DbTask(TaskFunc task, CallbackFunc callback, QObject *receiver);
    void run() override;

private:
    TaskFunc m_task;
    CallbackFunc m_callback;
    QObject *m_receiver;
};

class TaskRunner : public QObject
{
    Q_OBJECT

public:
    static TaskRunner& instance();

    TaskRunner(const TaskRunner&) = delete;
    TaskRunner& operator=(const TaskRunner&) = delete;

    // 模板方法：实现必须留在头文件中（模板在调用点实例化）
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

    int activeThreadCount() const;
    int maxThreadCount() const;

private:
    TaskRunner(QObject *parent = nullptr);
    ~TaskRunner();
};

#endif // TASKRUNNER_H
