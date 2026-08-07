#include "dbconnectionhelper.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QThread>
#include <QString>
#include <QDebug>
#include <QCoreApplication>

QSqlDatabase DbConnectionHelper::threadLocalConnection()
{
    QString connectionName = QString("worker_%1")
        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));

    if (!QSqlDatabase::contains(connectionName)) {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);

        QString dbPath = QCoreApplication::applicationDirPath() + "/imsystem.db";
        db.setDatabaseName(dbPath);

        if (!db.open()) {
            qCritical() << "[DbConnectionHelper] 线程" << QThread::currentThreadId()
                        << "无法打开数据库:" << db.lastError().text();
            return QSqlDatabase();
        }

        QSqlQuery pragmaQuery(db);
        pragmaQuery.exec("PRAGMA journal_mode=WAL");
        pragmaQuery.exec("PRAGMA busy_timeout=5000");
        pragmaQuery.exec("PRAGMA foreign_keys=ON");

        qInfo() << "[DbConnectionHelper] 线程" << QThread::currentThreadId()
                << "创建数据库连接:" << connectionName;
    }

    return QSqlDatabase::database(connectionName);
}

void DbConnectionHelper::cleanupCurrentThread()
{
    QString connectionName = QString("worker_%1")
        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));

    if (QSqlDatabase::contains(connectionName)) {
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
}
