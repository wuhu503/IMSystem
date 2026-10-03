#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QMutex>
#include <QSqlDatabase>
#include <QString>

// 只负责“数据库本身”：连接、建表、结构迁移、启动时的状态清理。
// 业务表的读写分别由 UserRepository / FriendRepository / MessageRepository 承担。
class DbManager
{
public:
    static DbManager& instance();

    // 禁止拷贝和赋值
    DbManager(const DbManager&) = delete;
    DbManager& operator=(const DbManager&) = delete;

    // 数据库初始化（dbPath 为空时使用 DbConnectionHelper::databaseFilePath()）
    bool init(const QString &dbPath = QString());

    // 关闭数据库
    void close();

    // 启动时把所有用户置为离线：进程被强杀/崩溃时不会走断开回调，
    // 数据库里会残留 status=1，导致好友列表把离线用户显示成在线
    bool resetOnlineStatus();

private:
    DbManager();
    ~DbManager();

    bool createTables();
    bool ensureSchema();
    bool columnExists(const QString &table, const QString &column);

    QSqlDatabase m_db;
    bool m_initialized;
    mutable QMutex m_mutex;  // 保护启动阶段的同步调用
};

#endif // DBMANAGER_H
