#ifndef USERMANAGER_H
#define USERMANAGER_H

#include <QObject>
#include <QMap>
#include <QMutex>
#include <QPointer>

class ClientHandler;

class UserManager : public QObject
{
    Q_OBJECT

public:
    static UserManager& instance();
    
    UserManager(const UserManager&) = delete;
    UserManager& operator=(const UserManager&) = delete;

    // 用户上线/下线
    void userOnline(qint64 userId, ClientHandler *handler);
    void userOffline(qint64 userId);
    
    // 查询在线状态
    bool isOnline(qint64 userId) const;
    QPointer<ClientHandler> getHandler(qint64 userId) const;
    
private:
    UserManager(QObject *parent = nullptr);
    ~UserManager();
    
    // 存 QPointer 而非裸指针：handler 若先于下线流程被销毁，弱引用会自动置空，
    // 避免后面用悬垂指针去构造 QPointer（那是未定义行为）
    QMap<qint64, QPointer<ClientHandler>> m_onlineUsers;  // userId -> handler
    mutable QMutex m_mutex;
};

#endif // USERMANAGER_H
