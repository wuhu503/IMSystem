#include "dbmanager.h"
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QRegularExpression>
#include <QJsonDocument>

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
        bool ok = query.exec(
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
        ok = query.exec(
            "CREATE TABLE IF NOT EXISTS friendships ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  user_id INTEGER NOT NULL,"
            "  friend_id INTEGER NOT NULL,"
            "  status INTEGER DEFAULT 0,"
            "  created_at INTEGER NOT NULL,"
            "  FOREIGN KEY (user_id) REFERENCES users(id),"
            "  FOREIGN KEY (friend_id) REFERENCES users(id),"
            "  UNIQUE(user_id, friend_id)"
            ")"
        ) && ok;
        ok = query.exec(
            "CREATE TABLE IF NOT EXISTS messages ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  msg_id TEXT NOT NULL UNIQUE,"
            "  sender_id INTEGER NOT NULL,"
            "  receiver_id INTEGER NOT NULL,"
            "  type INTEGER NOT NULL,"
            "  content TEXT NOT NULL,"
            "  timestamp INTEGER NOT NULL,"
            "  is_read INTEGER DEFAULT 0,"
            "  FOREIGN KEY (sender_id) REFERENCES users(id),"
            "  FOREIGN KEY (receiver_id) REFERENCES users(id)"
            ")"
        ) && ok;
        ok = query.exec("CREATE INDEX IF NOT EXISTS idx_users_username ON users(username)") && ok;
        ok = query.exec("CREATE INDEX IF NOT EXISTS idx_friendships_user ON friendships(user_id)") && ok;
        ok = query.exec("CREATE INDEX IF NOT EXISTS idx_friendships_friend ON friendships(friend_id)") && ok;
        ok = query.exec("CREATE INDEX IF NOT EXISTS idx_messages_sender ON messages(sender_id)") && ok;
        ok = query.exec("CREATE INDEX IF NOT EXISTS idx_messages_receiver ON messages(receiver_id)") && ok;
        ok = query.exec("CREATE INDEX IF NOT EXISTS idx_messages_timestamp ON messages(timestamp)") && ok;
        return ok;
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
    QMutexLocker locker(&m_mutex);
    if (isUsernameExistsInternal(username)) {
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

qint64 DbManager::getUserId(const QString &username)
{
    QMutexLocker locker(&m_mutex);
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
    QMutexLocker locker(&m_mutex);
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
    QMutexLocker locker(&m_mutex);
    return isUsernameExistsInternal(username);
}

bool DbManager::isUsernameExistsInternal(const QString &username)
{
    // 无锁版本，供 insertUser 等已持锁方法调用，避免死锁
    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM users WHERE username = ?");
    query.addBindValue(username);
    
    if (!query.exec() || !query.next()) {
        return false;
    }
    
    return (query.value(0).toInt() > 0);
}

// ============================================================================
// 异步方法实现 —— 使用 TaskRunner 将数据库操作提交到线程池
// ============================================================================

#include "../threading/taskrunner.h"
#include "../threading/dbconnectionhelper.h"

void DbManager::getUserIdAsync(const QString &username, 
                                std::function<void(qint64)> callback, QObject *receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [username]() -> QVariant {
            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            QSqlQuery query(db);
            query.prepare("SELECT id FROM users WHERE username = ?");
            query.addBindValue(username);
            if (!query.exec() || !query.next()) return QVariant(-1);
            return QVariant(query.value(0).toLongLong());
        },
        [callback](QVariant result) {
            if (callback) callback(result.toLongLong());
        }
    );
}

void DbManager::getUserInfoAsync(qint64 userId, 
                                  std::function<void(QVariantMap)> callback, QObject *receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [userId]() -> QVariant {
            QVariantMap info;
            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            QSqlQuery query(db);
            query.prepare(
                "SELECT id, username, nickname, avatar, status "
                "FROM users WHERE id = ?"
            );
            query.addBindValue(userId);
            
            if (query.exec() && query.next()) {
                info["id"] = query.value("id").toLongLong();
                info["username"] = query.value("username").toString();
                info["nickname"] = query.value("nickname").toString();
                info["avatar"] = query.value("avatar").toString();
                info["status"] = query.value("status").toInt();
            }
            
            // QVariantMap 不能直接放入 QVariant，需要转换
            QVariant v;
            v.setValue(info);
            return v;
        },
        [callback](QVariant result) {
            if (callback) callback(result.value<QVariantMap>());
        }
    );
}

void DbManager::saveMessageAsync(const QString &msgId, qint64 senderId, qint64 receiverId,
                                  int type, const QString &content,
                                  std::function<void(bool)> callback, QObject *receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [msgId, senderId, receiverId, type, content]() -> QVariant {
            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            QSqlQuery query(db);
            query.prepare(
                "INSERT INTO messages (msg_id, sender_id, receiver_id, type, content, timestamp, is_read) "
                "VALUES (?, ?, ?, ?, ?, ?, 0)"
            );
            
            query.addBindValue(msgId);
            query.addBindValue(senderId);
            query.addBindValue(receiverId);
            query.addBindValue(type);
            query.addBindValue(content);
            query.addBindValue(QDateTime::currentSecsSinceEpoch());
            
            bool success = query.exec();
            if (!success) {
                qCritical() << "[Async] 保存消息失败:" << query.lastError().text();
            }
            return QVariant(success);
        },
        [callback](QVariant result) {
            if (callback) callback(result.toBool());
        }
    );
}

void DbManager::getFriendListAsync(qint64 userId, 
                                    std::function<void(QJsonArray)> callback, QObject *receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [userId]() -> QVariant {
            QJsonArray friends;
            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            QSqlQuery query(db);
            query.prepare(
                "SELECT u.id, u.username, u.nickname, u.avatar, u.status "
                "FROM users u "
                "JOIN friendships f ON (f.friend_id = u.id) "
                "WHERE f.user_id = ? AND f.status = 1 "
                "UNION "
                "SELECT u.id, u.username, u.nickname, u.avatar, u.status "
                "FROM users u "
                "JOIN friendships f ON (f.user_id = u.id) "
                "WHERE f.friend_id = ? AND f.status = 1"
            );
            query.addBindValue(userId);
            query.addBindValue(userId);
            
            if (query.exec()) {
                while (query.next()) {
                    QJsonObject friendObj;
                    friendObj["user_id"] = query.value("id").toLongLong();
                    friendObj["username"] = query.value("username").toString();
                    friendObj["nickname"] = query.value("nickname").toString();
                    friendObj["avatar"] = query.value("avatar").toString();
                    friendObj["status"] = query.value("status").toInt();
                    friends.append(friendObj);
                }
            }
            
            // QJsonArray 不能直接放入 QVariant，需要转换为 QJsonDocument
            QJsonDocument doc(friends);
            return QVariant(doc.toJson(QJsonDocument::Compact));
        },
        [callback](QVariant result) {
            if (callback) {
                QJsonDocument doc = QJsonDocument::fromJson(result.toByteArray());
                callback(doc.array());
            }
        }
    );
}

void DbManager::isFriendAsync(qint64 userId, qint64 friendId,
                               std::function<void(bool)> callback, QObject *receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [userId, friendId]() -> QVariant {
            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            QSqlQuery query(db);
            query.prepare(
                "SELECT COUNT(*) FROM friendships "
                "WHERE (user_id = ? AND friend_id = ? AND status = 1) "
                "OR (user_id = ? AND friend_id = ? AND status = 1)"
            );
            query.addBindValue(userId);
            query.addBindValue(friendId);
            query.addBindValue(friendId);
            query.addBindValue(userId);
            
            if (query.exec() && query.next()) {
                return QVariant(query.value(0).toInt() > 0);
            }
            return QVariant(false);
        },
        [callback](QVariant result) {
            if (callback) callback(result.toBool());
        }
    );
}

void DbManager::getChatHistoryAsync(qint64 userId, qint64 friendId, int limit, int offset,
                                     std::function<void(QJsonArray)> callback, QObject *receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [userId, friendId, limit, offset]() -> QVariant {
            QJsonArray messages;
            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            QSqlQuery query(db);
            query.prepare(
                "SELECT m.msg_id, m.sender_id, m.receiver_id, m.type, m.content, "
                "m.timestamp, m.is_read, "
                "u1.username as sender_name, u2.username as receiver_name "
                "FROM messages m "
                "LEFT JOIN users u1 ON m.sender_id = u1.id "
                "LEFT JOIN users u2 ON m.receiver_id = u2.id "
                "WHERE (m.sender_id = ? AND m.receiver_id = ?) "
                "OR (m.sender_id = ? AND m.receiver_id = ?) "
                "ORDER BY m.timestamp DESC "
                "LIMIT ? OFFSET ?"
            );
            query.addBindValue(userId);
            query.addBindValue(friendId);
            query.addBindValue(friendId);
            query.addBindValue(userId);
            query.addBindValue(limit);
            query.addBindValue(offset);
            
            if (query.exec()) {
                while (query.next()) {
                    QJsonObject msgObj;
                    msgObj["msg_id"] = query.value("msg_id").toString();
                    msgObj["sender_id"] = query.value("sender_id").toLongLong();
                    msgObj["receiver_id"] = query.value("receiver_id").toLongLong();
                    msgObj["sender_name"] = query.value("sender_name").toString();
                    msgObj["receiver_name"] = query.value("receiver_name").toString();
                    msgObj["type"] = query.value("type").toInt();
                    msgObj["content"] = query.value("content").toString();
                    msgObj["timestamp"] = query.value("timestamp").toLongLong();
                    msgObj["is_read"] = query.value("is_read").toInt();
                    messages.append(msgObj);
                }
            }
            
            QJsonDocument doc(messages);
            return QVariant(doc.toJson(QJsonDocument::Compact));
        },
        [callback](QVariant result) {
            if (callback) {
                QJsonDocument doc = QJsonDocument::fromJson(result.toByteArray());
                callback(doc.array());
            }
        }
    );
}

void DbManager::searchUsersAsync(const QString &keyword, qint64 excludeUserId,
                                  std::function<void(QJsonArray)> callback, QObject *receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [keyword, excludeUserId]() -> QVariant {
            QJsonArray users;
            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            QSqlQuery query(db);
            query.prepare(
                "SELECT id, username, nickname, avatar, status "
                "FROM users "
                "WHERE (username LIKE ? OR nickname LIKE ?) AND id != ? "
                "LIMIT 20"
            );
            query.addBindValue("%" + keyword + "%");
            query.addBindValue("%" + keyword + "%");
            query.addBindValue(excludeUserId);
            
            if (query.exec()) {
                while (query.next()) {
                    QJsonObject userObj;
                    userObj["user_id"] = query.value("id").toLongLong();
                    userObj["username"] = query.value("username").toString();
                    userObj["nickname"] = query.value("nickname").toString();
                    userObj["avatar"] = query.value("avatar").toString();
                    userObj["status"] = query.value("status").toInt();
                    users.append(userObj);
                }
            }
            
            QJsonDocument doc(users);
            return QVariant(doc.toJson(QJsonDocument::Compact));
        },
        [callback](QVariant result) {
            if (callback) {
                QJsonDocument doc = QJsonDocument::fromJson(result.toByteArray());
                callback(doc.array());
            }
        }
    );
}

void DbManager::getPendingFriendRequestsAsync(qint64 userId,
                                               std::function<void(QJsonArray)> callback, QObject *receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [userId]() -> QVariant {
            QJsonArray requests;
            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            QSqlQuery query(db);
            query.prepare(
                "SELECT u.id, u.username, u.nickname, u.avatar "
                "FROM users u "
                "JOIN friendships f ON f.user_id = u.id "
                "WHERE f.friend_id = ? AND f.status = 0 "
                "ORDER BY f.created_at DESC"
            );
            query.addBindValue(userId);
            
            if (query.exec()) {
                while (query.next()) {
                    QJsonObject requestObj;
                    requestObj["user_id"] = query.value("id").toLongLong();
                    requestObj["username"] = query.value("username").toString();
                    requestObj["nickname"] = query.value("nickname").toString();
                    requestObj["avatar"] = query.value("avatar").toString();
                    requests.append(requestObj);
                }
            }
            
            QJsonDocument doc(requests);
            return QVariant(doc.toJson(QJsonDocument::Compact));
        },
        [callback](QVariant result) {
            if (callback) {
                QJsonDocument doc = QJsonDocument::fromJson(result.toByteArray());
                callback(doc.array());
            }
        }
    );
}

void DbManager::hasPendingFriendRequestAsync(qint64 userId, qint64 friendId,
                                              std::function<void(bool)> callback, QObject *receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [userId, friendId]() -> QVariant {
            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            QSqlQuery query(db);
            query.prepare(
                "SELECT COUNT(*) FROM friendships "
                "WHERE user_id = ? AND friend_id = ? AND status = 0"
            );
            query.addBindValue(userId);
            query.addBindValue(friendId);
            
            if (query.exec() && query.next()) {
                return QVariant(query.value(0).toInt() > 0);
            }
            return QVariant(false);
        },
        [callback](QVariant result) {
            if (callback) callback(result.toBool());
        }
    );
}

void DbManager::addFriendRequestAsync(qint64 userId, qint64 friendId,
                                       std::function<void(bool)> callback, QObject *receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [userId, friendId]() -> QVariant {
            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            QSqlQuery query(db);
            query.prepare(
                "INSERT INTO friendships (user_id, friend_id, status, created_at) "
                "VALUES (?, ?, 0, ?)"
            );
            qint64 now = QDateTime::currentSecsSinceEpoch();
            query.addBindValue(userId);
            query.addBindValue(friendId);
            query.addBindValue(now);
            
            bool success = query.exec();
            if (!success) {
                qCritical() << "[Async] 添加好友请求失败:" << query.lastError().text();
            }
            return QVariant(success);
        },
        [callback](QVariant result) {
            if (callback) callback(result.toBool());
        }
    );
}

void DbManager::acceptFriendRequestAsync(qint64 userId, qint64 friendId,
                                          std::function<void(bool)> callback, QObject *receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [userId, friendId]() -> QVariant {
            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            bool success = false;

            if (db.transaction()) {
                QSqlQuery query(db);
                query.prepare(
                    "UPDATE friendships SET status = 1 "
                    "WHERE user_id = ? AND friend_id = ? AND status = 0"
                );
                query.addBindValue(userId);
                query.addBindValue(friendId);
                success = query.exec() && query.numRowsAffected() > 0;

                // 补写反向好友关系，与同步版行为保持一致
                if (success) {
                    QSqlQuery insertQuery(db);
                    insertQuery.prepare(
                        "INSERT OR IGNORE INTO friendships (user_id, friend_id, status, created_at) "
                        "VALUES (?, ?, 1, ?)"
                    );
                    insertQuery.addBindValue(friendId);
                    insertQuery.addBindValue(userId);
                    insertQuery.addBindValue(QDateTime::currentSecsSinceEpoch());
                    success = insertQuery.exec();
                }

                if (success) {
                    db.commit();
                } else {
                    db.rollback();
                }
            }

            if (!success) {
                qCritical() << "[Async] 接受好友请求失败";
            }
            return QVariant(success);
        },
        [callback](QVariant result) {
            if (callback) callback(result.toBool());
        }
    );
}

void DbManager::rejectFriendRequestAsync(qint64 userId, qint64 friendId,
                                          std::function<void(bool)> callback, QObject *receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [userId, friendId]() -> QVariant {
            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            QSqlQuery query(db);
            query.prepare(
                "DELETE FROM friendships "
                "WHERE user_id = ? AND friend_id = ? AND status = 0"
            );
            query.addBindValue(userId);
            query.addBindValue(friendId);
            
            bool success = query.exec() && query.numRowsAffected() > 0;
            if (!success) {
                qCritical() << "[Async] 拒绝好友请求失败:" << query.lastError().text();
            }
            return QVariant(success);
        },
        [callback](QVariant result) {
            if (callback) callback(result.toBool());
        }
    );
}

void DbManager::deleteFriendAsync(qint64 userId, qint64 friendId,
                                   std::function<void(bool)> callback, QObject *receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [userId, friendId]() -> QVariant {
            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            bool success = false;

            if (db.transaction()) {
                QSqlQuery query(db);
                query.prepare(
                    "DELETE FROM friendships "
                    "WHERE (user_id = ? AND friend_id = ?) OR (user_id = ? AND friend_id = ?)"
                );
                query.addBindValue(userId);
                query.addBindValue(friendId);
                query.addBindValue(friendId);
                query.addBindValue(userId);
                success = query.exec();

                // 与同步版一致：同时删除双方聊天记录（幂等删除，0 行也视为成功）
                if (success) {
                    QSqlQuery deleteMsgQuery(db);
                    deleteMsgQuery.prepare(
                        "DELETE FROM messages "
                        "WHERE (sender_id = ? AND receiver_id = ?) OR (sender_id = ? AND receiver_id = ?)"
                    );
                    deleteMsgQuery.addBindValue(userId);
                    deleteMsgQuery.addBindValue(friendId);
                    deleteMsgQuery.addBindValue(friendId);
                    deleteMsgQuery.addBindValue(userId);
                    success = deleteMsgQuery.exec();
                }

                if (success) {
                    db.commit();
                } else {
                    db.rollback();
                }
            }

            if (!success) {
                qCritical() << "[Async] 删除好友失败";
            }
            return QVariant(success);
        },
        [callback](QVariant result) {
            if (callback) callback(result.toBool());
        }
    );
}


void DbManager::updateUserStatusAsync(qint64 userId, int status,
                                       std::function<void(bool)> callback, QObject *receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [userId, status]() -> QVariant {
            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            QSqlQuery query(db);
            query.prepare("UPDATE users SET status = ?, updated_at = ? WHERE id = ?");
            query.addBindValue(status);
            query.addBindValue(QDateTime::currentSecsSinceEpoch());
            query.addBindValue(userId);
            
            bool success = query.exec();
            return QVariant(success);
        },
        [callback](QVariant result) {
            if (callback) callback(result.toBool());
        }
    );
}


