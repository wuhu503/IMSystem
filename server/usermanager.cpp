#include "usermanager.h"
#include "clienthandler.h"

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
    m_onlineUsers[userId] = handler;
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
    return m_onlineUsers.contains(userId);
}

ClientHandler* UserManager::getHandler(qint64 userId) const
{
    QMutexLocker locker(&m_mutex);
    return m_onlineUsers.value(userId, nullptr);
}

int UserManager::onlineCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_onlineUsers.size();
}
