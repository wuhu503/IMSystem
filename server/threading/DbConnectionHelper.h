#ifndef DBCONNECTIONHELPER_H
#define DBCONNECTIONHELPER_H

#include <QSqlDatabase>
#include <QString>

class DbConnectionHelper
{
public:
    // 数据库文件的统一位置：优先取环境变量 IM_DB_PATH（便于测试与多实例部署），
    // 否则落在用户数据目录；首次使用时会从旧的“可执行文件同目录”位置迁移一次
    static QString databaseFilePath();

    static QSqlDatabase threadLocalConnection();
    static void cleanupCurrentThread();

    // 事务辅助：SQLite 默认的 BEGIN 是 DEFERRED，先读后写时锁升级会立刻返回
    // SQLITE_BUSY（不等待 busy_timeout），所以统一用 BEGIN IMMEDIATE 先拿写锁
    static bool beginImmediate(QSqlDatabase &db);
    static bool commit(QSqlDatabase &db);
    static bool rollback(QSqlDatabase &db);

private:
    DbConnectionHelper() = delete;
};

#endif // DBCONNECTIONHELPER_H
