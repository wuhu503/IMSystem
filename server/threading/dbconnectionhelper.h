#ifndef DBCONNECTIONHELPER_H
#define DBCONNECTIONHELPER_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QThread>
#include <QString>
#include <QDebug>
#include <QCoreApplication>

class DbConnectionHelper
{
public:
    static QSqlDatabase threadLocalConnection()
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
            
            qInfo() << "[DbConnectionHelper] 线程" << QThread::currentThreadId()
                    << "创建数据库连接:" << connectionName;
        }

        return QSqlDatabase::database(connectionName);
    }

    static void cleanupCurrentThread()
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

private:
    DbConnectionHelper() = delete;
};

#endif // DBCONNECTIONHELPER_H
