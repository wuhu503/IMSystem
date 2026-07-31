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
#include <QMutex>
#include <functional>

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
    
    // ========== 同步接口（保留，authservice 继续使用） ==========
    
    //用户操作
    bool insertUser(const QString &username, const QString &passwordHash, 
                    const QString &salt);
    bool verifyUser(const QString &username, const QString &passwordHash);
    qint64 getUserId(const QString &username);
    QVariantMap getUserInfo(qint64 userId);
    bool isUsernameExists(const QString &username);
    void updateUserStatus(qint64 userId, int status);
    
    // 好友操作
    bool addFriendRequest(qint64 userId, qint64 friendId);
    bool acceptFriendRequest(qint64 userId, qint64 friendId);
    bool rejectFriendRequest(qint64 userId, qint64 friendId);
    bool deleteFriend(qint64 userId, qint64 friendId);
    bool isFriend(qint64 userId, qint64 friendId);
    bool hasPendingFriendRequest(qint64 userId, qint64 friendId);
    QJsonArray getFriendList(qint64 userId);
    QJsonArray getPendingFriendRequests(qint64 userId);
    QJsonArray searchUsers(const QString &keyword, qint64 excludeUserId);
    
    // 消息操作
    bool saveMessage(const QString &msgId, qint64 senderId, qint64 receiverId,
                     int type, const QString &content);
    QJsonArray getChatHistory(qint64 userId, qint64 friendId, 
                              int limit = 50, int offset = 0);
    bool markMessageAsRead(const QString &msgId);
    int getUnreadMessageCount(qint64 userId, qint64 senderId);

    // ========== 异步接口（供 chatservice / friendservice 使用） ==========
    
    // 异步查询用户ID
    void getUserIdAsync(const QString &username, 
                        std::function<void(qint64)> callback, QObject *receiver);
    
    // 异步查询用户信息
    void getUserInfoAsync(qint64 userId, 
                          std::function<void(QVariantMap)> callback, QObject *receiver);
    
    // 异步保存消息
    void saveMessageAsync(const QString &msgId, qint64 senderId, qint64 receiverId,
                          int type, const QString &content,
                          std::function<void(bool)> callback, QObject *receiver);
    
    // 异步获取好友列表
    void getFriendListAsync(qint64 userId, 
                            std::function<void(QJsonArray)> callback, QObject *receiver);
    
    // 异步检查好友关系
    void isFriendAsync(qint64 userId, qint64 friendId,
                       std::function<void(bool)> callback, QObject *receiver);
    
    // 异步获取聊天历史
    void getChatHistoryAsync(qint64 userId, qint64 friendId, int limit, int offset,
                             std::function<void(QJsonArray)> callback, QObject *receiver);
    
    // 异步搜索用户
    void searchUsersAsync(const QString &keyword, qint64 excludeUserId,
                          std::function<void(QJsonArray)> callback, QObject *receiver);
    
    // 异步获取待处理好友请求
    void getPendingFriendRequestsAsync(qint64 userId,
                                       std::function<void(QJsonArray)> callback, QObject *receiver);
    
    // 异步检查好友请求是否存在
    void hasPendingFriendRequestAsync(qint64 userId, qint64 friendId,
                                      std::function<void(bool)> callback, QObject *receiver);
    
    // 异步添加好友请求
    void addFriendRequestAsync(qint64 userId, qint64 friendId,
                               std::function<void(bool)> callback, QObject *receiver);
    
    // 异步接受好友请求
    void acceptFriendRequestAsync(qint64 userId, qint64 friendId,
                                  std::function<void(bool)> callback, QObject *receiver);
    
    // 异步拒绝好友请求
    void rejectFriendRequestAsync(qint64 userId, qint64 friendId,
                                  std::function<void(bool)> callback, QObject *receiver);
    
    // 异步删除好友
    void deleteFriendAsync(qint64 userId, qint64 friendId,
                           std::function<void(bool)> callback, QObject *receiver);
    
    // 异步更新用户状态
    void updateUserStatusAsync(qint64 userId, int status,
                              std::function<void(bool)> callback, QObject *receiver);

private:
    DbManager();
    ~DbManager();
    
    bool createTables();
    
    QSqlDatabase m_db;
    bool m_initialized;
    mutable QMutex m_mutex;  // 保护同步方法（主线程的少量同步调用）
};

#endif // DBMANAGER_H


