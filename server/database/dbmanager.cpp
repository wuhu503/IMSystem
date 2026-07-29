#include "dbmanager.h"
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QRegularExpression>

// ========== 单例实现 ==========

DbManager& DbManager::instance()
{
    static DbManager instance;
    return instance;
}

// ========== 构造函数/析构函数 ==========

DbManager::DbManager() 
    : m_initialized(false)
{
}

DbManager::~DbManager()
{
    close();
}

// ========== 初始化和关闭 ==========

bool DbManager::init(const QString &dbPath)
{
    if (m_initialized) {
        return true;
    }
    
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    
    QString absolutePath = dbPath;
    if (!QDir::isAbsolutePath(dbPath)) {
        absolutePath = QCoreApplication::applicationDirPath() + "/" + dbPath;
    }
    m_db.setDatabaseName(absolutePath);
    
    if (!m_db.open()) {
        qCritical() << "无法打开数据库:" << m_db.lastError().text();
        return false;
    }
    
    qInfo() << "数据库已打开:" << absolutePath;
    
    if (!createTables()) {
        qCritical() << "创建表失败";
        return false;
    }
    
    m_initialized = true;
    return true;
}

void DbManager::close()
{
    if (m_db.isOpen()) {
        m_db.close();
        qInfo() << "数据库已关闭";
    }
}

bool DbManager::createTables()
{
    QString sqlPath = QCoreApplication::applicationDirPath() + "/init.sql";
    QFile sqlFile(sqlPath);
    
    if (!sqlFile.exists()) {
        sqlPath = ":/database/init.sql";
        sqlFile.setFileName(sqlPath);
    }
    
    if (!sqlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开 init.sql，将使用内联 SQL 创建表";
        QSqlQuery query;
        return query.exec(
            "CREATE TABLE IF NOT EXISTS users ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  username TEXT NOT NULL UNIQUE,"
            "  password_hash TEXT NOT NULL,"
            "  salt TEXT NOT NULL,"
            "  nickname TEXT DEFAULT '',"
            "  avatar TEXT DEFAULT '',"
            "  status INTEGER DEFAULT 0,"
            "  created_at INTEGER NOT NULL,"
            "  updated_at INTEGER NOT NULL"
            ")"
        );
    }
    
    QString sql = QString::fromUtf8(sqlFile.readAll());
    sqlFile.close();
    
    // 移除SQL注释
    QRegularExpression commentRegex("--[^\\n]*");
    sql.replace(commentRegex, "");
    
    // 按分号拆分，逐条执行
    QStringList statements = sql.split(';', Qt::SkipEmptyParts);
    qInfo() << "SQL脚本拆分后语句数:" << statements.size();
    
    QSqlQuery query;
    for (const QString &stmt : statements) {
        QString trimmed = stmt.simplified();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (!query.exec(trimmed)) {
            // 忽略 "already exists" 类的错误
            QString errText = query.lastError().text();
            if (!errText.contains("already exists", Qt::CaseInsensitive)) {
                qCritical() << "执行建表脚本失败:" << errText;
                qCritical() << "SQL:" << trimmed;
                return false;
            }
        }
    }
    
    qInfo() << "数据库表创建成功";
    return true;
}

// ========== 用户操作 ==========

bool DbManager::insertUser(const QString &username, const QString &passwordHash, 
                           const QString &salt)
{
    if (isUsernameExists(username)) {
        qWarning() << "用户名已存在:" << username;
        return false;
    }
    
    QSqlQuery query;
    query.prepare(
        "INSERT INTO users (username, password_hash, salt, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?)"
    );
    
    qint64 now = QDateTime::currentSecsSinceEpoch();
    query.addBindValue(username);
    query.addBindValue(passwordHash);
    query.addBindValue(salt);
    query.addBindValue(now);
    query.addBindValue(now);
    
    if (!query.exec()) {
        qCritical() << "插入用户失败:" << query.lastError().text();
        return false;
    }
    
    qInfo() << "用户注册成功:" << username;
    return true;
}

bool DbManager::verifyUser(const QString &username, const QString &passwordHash)
{
    QSqlQuery query;
    query.prepare("SELECT password_hash FROM users WHERE username = ?");
    query.addBindValue(username);
    
    if (!query.exec() || !query.next()) {
        return false;
    }
    
    QString storedHash = query.value("password_hash").toString();
    return (passwordHash == storedHash);
}

qint64 DbManager::getUserId(const QString &username)
{
    QSqlQuery query;
    query.prepare("SELECT id FROM users WHERE username = ?");
    query.addBindValue(username);
    
    if (!query.exec() || !query.next()) {
        return -1;
    }
    
    return query.value("id").toLongLong();
}


QVariantMap DbManager::getUserInfo(qint64 userId)
{
    QVariantMap info;
    
    QSqlQuery query;
    query.prepare(
        "SELECT id, username, password_hash, salt, nickname, avatar, status, created_at "
        "FROM users WHERE id = ?"
    );
    query.addBindValue(userId);
    
    if (!query.exec() || !query.next()) {
        return info;
    }
    
    info["id"] = query.value("id").toLongLong();
    info["username"] = query.value("username").toString();
    info["password_hash"] = query.value("password_hash").toString();
    info["salt"] = query.value("salt").toString();
    info["nickname"] = query.value("nickname").toString();
    info["avatar"] = query.value("avatar").toString();
    info["status"] = query.value("status").toInt();
    info["created_at"] = query.value("created_at").toLongLong();
    
    return info;
}

bool DbManager::isUsernameExists(const QString &username)
{
    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM users WHERE username = ?");
    query.addBindValue(username);
    
    if (!query.exec() || !query.next()) {
        return false;
    }
    
    return (query.value(0).toInt() > 0);
}

void DbManager::updateUserStatus(qint64 userId, int status)
{
    QSqlQuery query;
    query.prepare("UPDATE users SET status = ?, updated_at = ? WHERE id = ?");
    query.addBindValue(status);
    query.addBindValue(QDateTime::currentSecsSinceEpoch());
    query.addBindValue(userId);
    
    if (!query.exec()) {
        qWarning() << "更新用户状态失败:" << query.lastError().text();
    }
}

// ========== 好友操作 ==========

bool DbManager::addFriendRequest(qint64 userId, qint64 friendId)
{
    QSqlQuery query;
    query.prepare(
        "INSERT INTO friendships (user_id, friend_id, status, created_at) "
        "VALUES (?, ?, 0, ?)"
    );
    
    query.addBindValue(userId);
    query.addBindValue(friendId);
    query.addBindValue(QDateTime::currentSecsSinceEpoch());
    
    if (!query.exec()) {
        qCritical() << "添加好友申请失败:" << query.lastError().text();
        return false;
    }
    
    qInfo() << "好友申请已添加:" << userId << "->" << friendId;
    return true;
}

bool DbManager::acceptFriendRequest(qint64 userId, qint64 friendId)
{
    // 更新状态为已接受
    QSqlQuery query;
    query.prepare(
        "UPDATE friendships SET status = 1 WHERE user_id = ? AND friend_id = ? AND status = 0"
    );
    query.addBindValue(userId);
    query.addBindValue(friendId);
    
    if (!query.exec()) {
        qCritical() << "接受好友请求失败:" << query.lastError().text();
        return false;
    }
    
    // 添加反向好友关系
    QSqlQuery insertQuery;
    insertQuery.prepare(
        "INSERT OR IGNORE INTO friendships (user_id, friend_id, status, created_at) "
        "VALUES (?, ?, 1, ?)"
    );
    insertQuery.addBindValue(friendId);
    insertQuery.addBindValue(userId);
    insertQuery.addBindValue(QDateTime::currentSecsSinceEpoch());
    
    if (!insertQuery.exec()) {
        qCritical() << "添加反向好友关系失败:" << insertQuery.lastError().text();
        return false;
    }
    
    qInfo() << "好友请求已接受:" << userId << "接受了" << friendId;
    return true;
}

bool DbManager::rejectFriendRequest(qint64 userId, qint64 friendId)
{
    QSqlQuery query;
    query.prepare(
        "UPDATE friendships SET status = 2 WHERE user_id = ? AND friend_id = ? AND status = 0"
    );
    query.addBindValue(userId);
    query.addBindValue(friendId);
    
    if (!query.exec()) {
        qCritical() << "拒绝好友请求失败:" << query.lastError().text();
        return false;
    }
    
    qInfo() << "好友请求已拒绝:" << userId << "拒绝了" << friendId;
    return true;
}

bool DbManager::deleteFriend(qint64 userId, qint64 friendId)
{
    // 删除双向好友关系
    QSqlQuery query;
    query.prepare(
        "DELETE FROM friendships "
        "WHERE (user_id = ? AND friend_id = ?) OR (user_id = ? AND friend_id = ?)"
    );
    query.addBindValue(userId);
    query.addBindValue(friendId);
    query.addBindValue(friendId);
    query.addBindValue(userId);
    
    if (!query.exec()) {
        qCritical() << "删除好友失败:" << query.lastError().text();
        return false;
    }
    
    qInfo() << "好友已删除:" << userId << "和" << friendId;
    return true;
}

bool DbManager::isFriend(qint64 userId, qint64 friendId)
{
    QSqlQuery query;
    query.prepare(
        "SELECT COUNT(*) FROM friendships "
        "WHERE ((user_id = ? AND friend_id = ?) OR (user_id = ? AND friend_id = ?)) "
        "AND status = 1"
    );
    query.addBindValue(userId);
    query.addBindValue(friendId);
    query.addBindValue(friendId);
    query.addBindValue(userId);
    
    if (!query.exec() || !query.next()) {
        return false;
    }
    
    return (query.value(0).toInt() > 0);
}

bool DbManager::hasPendingFriendRequest(qint64 userId, qint64 friendId)
{
    QSqlQuery query;
    query.prepare(
        "SELECT COUNT(*) FROM friendships "
        "WHERE user_id = ? AND friend_id = ? AND status = 0"
    );
    query.addBindValue(userId);
    query.addBindValue(friendId);
    
    if (!query.exec() || !query.next()) {
        return false;
    }
    
    return (query.value(0).toInt() > 0);
}

QJsonArray DbManager::getFriendList(qint64 userId)
{
    QJsonArray friends;
    
    QSqlQuery query;
    query.prepare(
        "SELECT u.id, u.username, u.nickname, u.avatar, u.status "
        "FROM users u "
        "INNER JOIN friendships f ON (f.friend_id = u.id) "
        "WHERE f.user_id = ? AND f.status = 1 "
        "ORDER BY u.status DESC, u.username"
    );
    query.addBindValue(userId);
    
    if (!query.exec()) {
        qCritical() << "获取好友列表失败:" << query.lastError().text();
        return friends;
    }
    
    while (query.next()) {
        QJsonObject friendObj;
        friendObj["user_id"] = query.value("id").toLongLong();
        friendObj["username"] = query.value("username").toString();
        friendObj["nickname"] = query.value("nickname").toString();
        friendObj["avatar"] = query.value("avatar").toString();
        friendObj["status"] = query.value("status").toInt();
        friends.append(friendObj);
    }
    
    return friends;
}

QJsonArray DbManager::getPendingFriendRequests(qint64 userId)
{
    QJsonArray requests;
    
    QSqlQuery query;
    query.prepare(
        "SELECT u.id, u.username, u.nickname, u.avatar "
        "FROM users u "
        "INNER JOIN friendships f ON (f.user_id = u.id) "
        "WHERE f.friend_id = ? AND f.status = 0 "
        "ORDER BY f.created_at DESC"
    );
    query.addBindValue(userId);
    
    if (!query.exec()) {
        qCritical() << "获取待处理好友请求失败:" << query.lastError().text();
        return requests;
    }
    
    while (query.next()) {
        QJsonObject requestObj;
        requestObj["user_id"] = query.value("id").toLongLong();
        requestObj["username"] = query.value("username").toString();
        requestObj["nickname"] = query.value("nickname").toString();
        requestObj["avatar"] = query.value("avatar").toString();
        requests.append(requestObj);
    }
    
    return requests;
}

QJsonArray DbManager::searchUsers(const QString &keyword, qint64 excludeUserId)
{
    QJsonArray users;
    
    QSqlQuery query;
    query.prepare(
        "SELECT id, username, nickname, avatar, status "
        "FROM users "
        "WHERE (username LIKE ? OR nickname LIKE ?) AND id != ? "
        "LIMIT 20"
    );
    query.addBindValue("%" + keyword + "%");
    query.addBindValue("%" + keyword + "%");
    query.addBindValue(excludeUserId);
    
    if (!query.exec()) {
        qCritical() << "搜索用户失败:" << query.lastError().text();
        return users;
    }
    
    while (query.next()) {
        QJsonObject userObj;
        userObj["user_id"] = query.value("id").toLongLong();
        userObj["username"] = query.value("username").toString();
        userObj["nickname"] = query.value("nickname").toString();
        userObj["avatar"] = query.value("avatar").toString();
        userObj["status"] = query.value("status").toInt();
        users.append(userObj);
    }
    
    return users;
}
