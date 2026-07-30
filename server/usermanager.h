#ifndef USERMANAGER_H
#define USERMANAGER_H

#include <QObject>
#include <QMap>
#include <QMutex>

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
    ClientHandler* getHandler(qint64 userId) const;
    
    // 获取在线用户数
    int onlineCount() const;

private:
    UserManager(QObject *parent = nullptr);
    ~UserManager();
    
    QMap<qint64, ClientHandler*> m_onlineUsers;  // userId -> handler
    mutable QMutex m_mutex;
};

#endif // USERMANAGER_H
