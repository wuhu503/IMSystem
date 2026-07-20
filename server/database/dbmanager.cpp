#include "dbmanager.h"
#include <QFile>
#include <QDir>
#include <QStandardPaths>

//单例实现
DbManager& DbManager::instance()
{
    static DbManager instance;
    return instance;
}

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
    // 如果已经初始化，直接返回
    if (m_initialized) {
        return true;
    }
    
    //添加 SQLite 数据库连接
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    
    // 步骤2：设置数据库文件路径
    // 如果路径不是绝对路径，则相对于程序所在目录
    QString absolutePath = dbPath;
    if (!QDir::isAbsolutePath(dbPath)) {
        absolutePath = QCoreApplication::applicationDirPath() + "/" + dbPath;
    }
    m_db.setDatabaseName(absolutePath);
    
    // 步骤3：打开数据库
    if (!m_db.open()) {
        qCritical() << "无法打开数据库:" << m_db.lastError().text();
        return false;
    }
    
    qInfo() << "数据库已打开:" << absolutePath;
    
    // 步骤4：执行建表脚本
    if (!createTables()) {
        qCritical() << "创建表失败";
        return false;
    }
    
    m_initialized = true;
    return true;
}

//关闭数据库
void DbManager::close()
{
    if (m_db.isOpen()) {
        m_db.close();
        qInfo() << "数据库已关闭";
    }
}

//执行建表
bool DbManager::createTables()
{

    QString sqlPath = QCoreApplication::applicationDirPath() + "/init.sql";
    QFile sqlFile(sqlPath);
    
    // 如果文件不存在，尝试从资源文件读取
    if (!sqlFile.exists()) {
        sqlPath = ":/database/init.sql";  // Qt 资源路径
        sqlFile.setFileName(sqlPath);
    }
    
    if (!sqlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开 init.sql，将使用内联 SQL 创建表";
        // 如果找不到文件，使用内联的建表语句
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
    
    // 读取文件内容
    QString sql = QString::fromUtf8(sqlFile.readAll());
    sqlFile.close();
    
    // 执行 SQL
    QSqlQuery query;
    if (!query.exec(sql)) {
        qCritical() << "执行建表脚本失败:" << query.lastError().text();
        return false;
    }
    
    qInfo() << "数据库表创建成功";
    return true;
}

//用户操作


bool DbManager::insertUser(const QString &username, const QString &passwordHash, 
                           const QString &salt)
{
    //检查用户名是否已存在
    if (isUsernameExists(username)) {
        qWarning() << "用户名已存在:" << username;
        return false;
    }
    
    //准备SQL语句
    QSqlQuery query;
    query.prepare(
        "INSERT INTO users (username, password_hash, salt, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?)"
    );
    
    //绑定参数
    qint64 now = QDateTime::currentSecsSinceEpoch();
    query.addBindValue(username);
    query.addBindValue(passwordHash);
    query.addBindValue(salt);
    query.addBindValue(now);
    query.addBindValue(now);
    
    //执行插入
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
    query.prepare(
        "SELECT password_hash FROM users WHERE username = ?"
    );
    query.addBindValue(username);
    
    if (!query.exec() || !query.next()) {
        return false;  // 用户不存在
    }
    
    //获取存储的密码哈希
    QString storedHash = query.value("password_hash").toString();
    
    //比较哈希值
    return (passwordHash == storedHash);
}

qint64 DbManager::getUserId(const QString &username)
{
    QSqlQuery query;
    query.prepare("SELECT id FROM users WHERE username = ?");
    query.addBindValue(username);
    
    if (!query.exec() || !query.next()) {
        return -1;  //用户不存在
    }
    
    return query.value("id").toLongLong();
}

QVariantMap DbManager::getUserInfo(qint64 userId)
{
    QVariantMap info;
    
    QSqlQuery query;
    query.prepare(
        "SELECT id, username, nickname, avatar, status, created_at "
        "FROM users WHERE id = ?"
    );
    query.addBindValue(userId);
    
    if (!query.exec() || !query.next()) {
        return info;  //返回空 Map
    }
    
    info["id"] = query.value("id").toLongLong();
    info["username"] = query.value("username").toString();
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
