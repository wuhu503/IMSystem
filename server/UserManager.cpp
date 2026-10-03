#include "UserManager.h"
#include "ClientHandler.h"

UserManager& UserManager::instance()
{
    static UserManager instance;
    return instance;
}

UserManager::UserManager(QObject *parent)
    : QObject(parent)
{
}

UserManager::~UserManager()
{
}

void UserManager::userOnline(qint64 userId, ClientHandler *handler)
{
    QMutexLocker locker(&m_mutex);
    m_onlineUsers.insert(userId, QPointer<ClientHandler>(handler));
    qInfo() << "[UserManager] 用户上线:" << userId << "，当前在线:" << m_onlineUsers.size();
}

void UserManager::userOffline(qint64 userId)
{
    QMutexLocker locker(&m_mutex);
    m_onlineUsers.remove(userId);
    qInfo() << "[UserManager] 用户下线:" << userId << "，当前在线:" << m_onlineUsers.size();
}

bool UserManager::isOnline(qint64 userId) const
{
    QMutexLocker locker(&m_mutex);
    // QPointer 在对象销毁后自动置空，这里顺带把“已销毁但还没移除”的条目视为离线
    return m_onlineUsers.contains(userId) && !m_onlineUsers.value(userId).isNull();
}

QPointer<ClientHandler> UserManager::getHandler(qint64 userId) const
{
    QMutexLocker locker(&m_mutex);
    return m_onlineUsers.value(userId);
}

