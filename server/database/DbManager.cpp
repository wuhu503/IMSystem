#include "DbManager.h"

#include "DbConnectionHelper.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

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

    QString absolutePath = dbPath;
    if (absolutePath.isEmpty()) {
        // 与工作线程使用同一个路径来源，避免主连接和线程连接指向不同文件
        absolutePath = DbConnectionHelper::databaseFilePath();
    } else if (!QDir::isAbsolutePath(absolutePath)) {
        absolutePath = QCoreApplication::applicationDirPath() + "/" + absolutePath;
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(absolutePath);

    if (!m_db.open()) {
        qCritical() << "无法打开数据库:" << m_db.lastError().text();
        return false;
    }

    qInfo() << "数据库已打开:" << absolutePath;

    // 主连接同样启用 WAL / 忙等待 / 外键约束
    QSqlQuery pragmaQuery(m_db);
    pragmaQuery.exec("PRAGMA journal_mode=WAL");
    pragmaQuery.exec("PRAGMA busy_timeout=5000");
    pragmaQuery.exec("PRAGMA foreign_keys=ON");

    if (!createTables()) {
        qCritical() << "创建表失败";
        return false;
    }

    if (!ensureSchema()) {
        qCritical() << "数据库结构迁移失败";
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
    const QString sqlPath = QCoreApplication::applicationDirPath() + "/init.sql";
    QFile sqlFile(sqlPath);

    // 打开失败时用内联语句兜底，保证没有脚本也能建表
    if (!sqlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开 init.sql，改用内联建表语句";
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
            "  delivered INTEGER DEFAULT 0,"
            "  FOREIGN KEY (sender_id) REFERENCES users(id),"
            "  FOREIGN KEY (receiver_id) REFERENCES users(id)"
            ")"
        ) && ok;
        ok = query.exec("CREATE INDEX IF NOT EXISTS idx_users_username ON users(username)") && ok;
        ok = query.exec("CREATE INDEX IF NOT EXISTS idx_friendships_user ON friendships(user_id)") && ok;
        ok = query.exec("CREATE INDEX IF NOT EXISTS idx_friendships_friend ON friendships(friend_id)") && ok;
        ok = query.exec("CREATE INDEX IF NOT EXISTS idx_messages_pair "
                        "ON messages(sender_id, receiver_id, timestamp)") && ok;
        return ok;
    }

    QString sql = QString::fromUtf8(sqlFile.readAll());
    sqlFile.close();

    // 移除 SQL 注释后按分号逐条执行
    QRegularExpression commentRegex("--[^\\n]*");
    sql.replace(commentRegex, "");

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

// 结构迁移：init.sql 里的 CREATE TABLE IF NOT EXISTS 不会给已存在的表补列，
// 所以老库需要在这里显式补齐（幂等）
bool DbManager::ensureSchema()
{
    bool ok = true;

    if (!columnExists("messages", "delivered")) {
        QSqlQuery alter;
        if (alter.exec("ALTER TABLE messages ADD COLUMN delivered INTEGER DEFAULT 0")) {
            qInfo() << "数据库迁移：messages 表新增 delivered 列";
        } else {
            qCritical() << "数据库迁移失败(新增 delivered 列):" << alter.lastError().text();
            ok = false;
        }
    }

    QSqlQuery indexQuery;
    if (!indexQuery.exec("CREATE INDEX IF NOT EXISTS idx_messages_pair "
                         "ON messages(sender_id, receiver_id, timestamp)")) {
        qCritical() << "数据库迁移失败(创建复合索引):" << indexQuery.lastError().text();
        ok = false;
    }

    return ok;
}

bool DbManager::columnExists(const QString &table, const QString &column)
{
    QSqlQuery query(m_db);
    if (!query.exec("PRAGMA table_info(" + table + ")")) {
        return false;
    }

    while (query.next()) {
        if (query.value("name").toString() == column) {
            return true;
        }
    }
    return false;
}

bool DbManager::resetOnlineStatus()
{
    QMutexLocker locker(&m_mutex);

    QSqlQuery query(m_db);
    if (!query.exec("UPDATE users SET status = 0 WHERE status <> 0")) {
        qWarning() << "重置残留在线状态失败:" << query.lastError().text();
        return false;
    }

    const int affected = query.numRowsAffected();
    if (affected > 0) {
        qInfo() << "启动时重置残留的在线状态, 影响行数:" << affected;
    }
    return true;
}
