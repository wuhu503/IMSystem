#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QList>
#include <QVariantMap>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <QCoreApplication>

class DbManager
{
public:
    static DbManager& instance();
    
    // 禁止拷贝和赋值
    DbManager(const DbManager&) = delete;
    DbManager& operator=(const DbManager&) = delete;
    
    //数据库初始化
    bool init(const QString &dbPath = "imsystem.db");
    
    //关闭数据库
    void close();
    
    //用户操作
    bool insertUser(const QString &username, const QString &passwordHash, 
                    const QString &salt);
    bool verifyUser(const QString &username, const QString &passwordHash);
    qint64 getUserId(const QString &username);
    QVariantMap getUserInfo(qint64 userId);
    bool isUsernameExists(const QString &username);
    void updateUserStatus(qint64 userId, int status);
    
    // ========== 好友操作 ==========
    bool addFriendRequest(qint64 userId, qint64 friendId);
    bool acceptFriendRequest(qint64 userId, qint64 friendId);
    bool rejectFriendRequest(qint64 userId, qint64 friendId);
    bool deleteFriend(qint64 userId, qint64 friendId);
    bool isFriend(qint64 userId, qint64 friendId);
    bool hasPendingFriendRequest(qint64 userId, qint64 friendId);
    QJsonArray getFriendList(qint64 userId);
    QJsonArray getPendingFriendRequests(qint64 userId);
    QJsonArray searchUsers(const QString &keyword, qint64 excludeUserId);
    
    // ========== 消息操作 ==========
    bool saveMessage(const QString &msgId, qint64 senderId, qint64 receiverId,
                     int type, const QString &content);
    QJsonArray getChatHistory(qint64 userId, qint64 friendId, 
                              int limit = 50, int offset = 0);
    bool markMessageAsRead(const QString &msgId);
    int getUnreadMessageCount(qint64 userId, qint64 senderId);

private:
    DbManager();
    ~DbManager();
    
    bool createTables();
    
    QSqlDatabase m_db;
    bool m_initialized;
};

#endif // DBMANAGER_H
