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
    QRegularExpression commentRegex("--[^\n]*");
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