#include "DbConnectionHelper.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QThread>

namespace {

// 每个线程只注册一次清理钩子（QThread::finished 的仿函数连接不支持 UniqueConnection）
thread_local bool g_cleanupHookRegistered = false;

QString connectionNameForCurrentThread()
{
    return QStringLiteral("worker_%1")
        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
}

// 旧版本把数据库放在可执行文件同目录；首次运行时迁移一次，避免用户数据“凭空消失”。
// WAL 文件一并复制，保证未 checkpoint 的数据不丢。
void migrateLegacyDatabaseIfNeeded(const QString &targetPath)
{
    if (QFileInfo::exists(targetPath)) {
        return;
    }

    const QString legacyPath = QCoreApplication::applicationDirPath() + QStringLiteral("/imsystem.db");
    if (!QFileInfo::exists(legacyPath)) {
        return;
    }

    QDir().mkpath(QFileInfo(targetPath).absolutePath());
    if (!QFile::copy(legacyPath, targetPath)) {
        qWarning() << "[DbConnectionHelper] 迁移旧数据库失败，将在新位置新建:" << targetPath;
        return;
    }

    if (QFileInfo::exists(legacyPath + QStringLiteral("-wal"))) {
        QFile::copy(legacyPath + QStringLiteral("-wal"), targetPath + QStringLiteral("-wal"));
    }

    qInfo() << "[DbConnectionHelper] 已迁移旧数据库:" << legacyPath << "->" << targetPath;
}

} // namespace

QString DbConnectionHelper::databaseFilePath()
{
    // 允许用环境变量覆盖，便于测试和一台机器跑多个实例
    const QByteArray override = qgetenv("IM_DB_PATH");
    if (!override.isEmpty()) {
        return QString::fromLocal8Bit(override);
    }

    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) {
        dir = QCoreApplication::applicationDirPath();
    }

    const QString path = dir + QStringLiteral("/imsystem.db");
    migrateLegacyDatabaseIfNeeded(path);
    return path;
}

QSqlDatabase DbConnectionHelper::threadLocalConnection()
{
    const QString connectionName = connectionNameForCurrentThread();

    if (!QSqlDatabase::contains(connectionName)) {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(databaseFilePath());

        if (!db.open()) {
            qCritical() << "[DbConnectionHelper] 线程" << QThread::currentThreadId()
                        << "无法打开数据库:" << db.lastError().text();
            return QSqlDatabase();
        }

        QSqlQuery pragmaQuery(db);
        pragmaQuery.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
        pragmaQuery.exec(QStringLiteral("PRAGMA busy_timeout=5000"));
        pragmaQuery.exec(QStringLiteral("PRAGMA foreign_keys=ON"));

        // 连接改为“线程退出时”销毁：既保证连接始终在创建它的线程里关闭，
        // 又不必每个任务都重开一次 SQLite 连接（否则线程本地复用形同虚设）
        if (!g_cleanupHookRegistered) {
            g_cleanupHookRegistered = true;
            QThread *thread = QThread::currentThread();
            QObject::connect(thread, &QThread::finished, thread, []() {
                DbConnectionHelper::cleanupCurrentThread();
            }, Qt::DirectConnection);
        }

        qInfo() << "[DbConnectionHelper] 线程" << QThread::currentThreadId()
                << "创建数据库连接:" << connectionName;
    }

    return QSqlDatabase::database(connectionName);
}

void DbConnectionHelper::cleanupCurrentThread()
{
    const QString connectionName = connectionNameForCurrentThread();

    if (!QSqlDatabase::contains(connectionName)) {
        return;
    }

    {
        QSqlDatabase db = QSqlDatabase::database(connectionName);
        if (db.isOpen()) {
            db.close();
        }
    }

    QSqlDatabase::removeDatabase(connectionName);
    qInfo() << "[DbConnectionHelper] 线程" << QThread::currentThreadId()
            << "清理数据库连接:" << connectionName;
}

bool DbConnectionHelper::beginImmediate(QSqlDatabase &db)
{
    QSqlQuery query(db);
    return query.exec(QStringLiteral("BEGIN IMMEDIATE"));
}

bool DbConnectionHelper::commit(QSqlDatabase &db)
{
    QSqlQuery query(db);
    return query.exec(QStringLiteral("COMMIT"));
}

bool DbConnectionHelper::rollback(QSqlDatabase &db)
{
    QSqlQuery query(db);
    return query.exec(QStringLiteral("ROLLBACK"));
}
