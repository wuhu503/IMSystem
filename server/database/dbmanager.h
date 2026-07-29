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
    //插入用户
    bool insertUser(const QString &username, const QString &passwordHash, 
                    const QString &salt);
    
    //登录
    bool verifyUser(const QString &username, const QString &passwordHash);
    
    //获取用户id
    qint64 getUserId(const QString &username);

    //获取用户信息
    QVariantMap getUserInfo(qint64 userId);
    
    //检查用户是否存在
    bool isUsernameExists(const QString &username);

    //更新用户状态
    void updateUserStatus(qint64 userId, int status);
    
    // ========== 好友操作 ==========
    
    // 添加好友申请
    bool addFriendRequest(qint64 userId, qint64 friendId);
    
    // 接受好友请求
    bool acceptFriendRequest(qint64 userId, qint64 friendId);
    
    // 拒绝好友请求
    bool rejectFriendRequest(qint64 userId, qint64 friendId);
    
    // 删除好友
    bool deleteFriend(qint64 userId, qint64 friendId);
    
    // 检查是否是好友
    bool isFriend(qint64 userId, qint64 friendId);
    
    // 检查是否有待处理的好友请求
    bool hasPendingFriendRequest(qint64 userId, qint64 friendId);
    
    // 获取好友列表
    QJsonArray getFriendList(qint64 userId);
    
    // 获取待处理的好友请求
    QJsonArray getPendingFriendRequests(qint64 userId);
    
    // 搜索用户
    QJsonArray searchUsers(const QString &keyword, qint64 excludeUserId);

private:
    DbManager();   // 私有构造函数
    ~DbManager();
    
    //sql脚本
    bool createTables();
    
    QSqlDatabase m_db;  // 数据库连接
    bool m_initialized; // 是否已初始化
};

#endif // DBMANAGER_H
