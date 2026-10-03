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

    DbTask(TaskFunc task, CallbackFunc callback, QPointer<QObject> receiver);
    void run() override;

private:
    TaskFunc m_task;
    CallbackFunc m_callback;
    QPointer<QObject> m_receiver;  // 弱引用：接收者被销毁后自动置空
};

class TaskRunner : public QObject
{
    Q_OBJECT

public:
    static TaskRunner& instance();

    TaskRunner(const TaskRunner&) = delete;
    TaskRunner& operator=(const TaskRunner&) = delete;

    // 模板方法：实现必须留在头文件中（模板在调用点实例化）
    // receiver 用 QPointer 而不是裸指针：任务在进入执行前接收者可能已经被销毁，
    // 用悬垂的裸指针构造 QPointer 是未定义行为，所以弱引用必须在提交时就建立
    template<typename Task, typename Callback>
    void runDbTask(const QPointer<QObject> &receiver, Task&& task, Callback&& callback)
    {
        if (receiver.isNull()) {
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

private:
    TaskRunner(QObject *parent = nullptr);
    ~TaskRunner();
};

#endif // TASKRUNNER_H
