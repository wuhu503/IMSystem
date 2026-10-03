#include "UserRepository.h"

#include "DbConnectionHelper.h"
#include "TaskRunner.h"
#include "Utils.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

UserRepository& UserRepository::instance()
{
    static UserRepository instance;
    return instance;
}

void UserRepository::registerUserAsync(const QString &username, const QString &password,
                                       std::function<void(RegisterResult)> callback,
                                       QPointer<QObject> receiver)
{
    // 查重、加盐、哈希、插入全部放在同一个数据库任务里，避免“先查后插”的竞态；
    // 并发同名注册由 users.username 的 UNIQUE 约束兜底
    TaskRunner::instance().runDbTask(
        receiver,
        [username, password]() -> QVariant {
            RegisterResult result;

            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            if (!db.isValid() || !db.isOpen()) {
                result.errorMessage = QStringLiteral("数据库暂时不可用，请稍后重试");
                return QVariant::fromValue(result);
            }

            QSqlQuery existsQuery(db);
            existsQuery.prepare("SELECT 1 FROM users WHERE username = ? LIMIT 1");
            existsQuery.addBindValue(username);
            if (existsQuery.exec() && existsQuery.next()) {
                result.errorMessage = QStringLiteral("用户名已存在");
                return QVariant::fromValue(result);
            }

            const QString salt = Utils::generateSalt();
            const QString hash = Utils::hashPassword(password, salt);
            const qint64 now = QDateTime::currentSecsSinceEpoch();

            QSqlQuery insertQuery(db);
            insertQuery.prepare(
                "INSERT INTO users (username, password_hash, salt, created_at, updated_at) "
                "VALUES (?, ?, ?, ?, ?)"
            );
            insertQuery.addBindValue(username);
            insertQuery.addBindValue(hash);
            insertQuery.addBindValue(salt);
            insertQuery.addBindValue(now);
            insertQuery.addBindValue(now);

            if (!insertQuery.exec()) {
                qWarning() << "[UserRepository] 插入用户失败:" << insertQuery.lastError().text();
                result.errorMessage = QStringLiteral("注册失败，请稍后重试");
                return QVariant::fromValue(result);
            }

            result.success = true;
            result.userId = insertQuery.lastInsertId().toLongLong();
            return QVariant::fromValue(result);
        },
        [callback](QVariant result) {
            if (callback) callback(result.value<RegisterResult>());
        }
    );
}

void UserRepository::verifyUserAsync(const QString &username, const QString &password,
                                     std::function<void(LoginCheckResult)> callback,
                                     QPointer<QObject> receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [username, password]() -> QVariant {
            LoginCheckResult result;

            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            if (!db.isValid() || !db.isOpen()) {
                return QVariant::fromValue(result);
            }

            QSqlQuery query(db);
            query.prepare("SELECT id, password_hash, salt FROM users WHERE username = ?");
            query.addBindValue(username);

            if (!query.exec() || !query.next()) {
                // 用户不存在时也做一次等价的慢哈希，避免通过响应时间枚举用户名
                Utils::verifyPassword(password,
                                      QStringLiteral("00000000000000000000000000000000"),
                                      QStringLiteral("pbkdf2$120000$00"));
                return QVariant::fromValue(result);
            }

            const QString salt = query.value("salt").toString();
            const QString storedHash = query.value("password_hash").toString();

            result.userId = query.value("id").toLongLong();
            result.salt = salt;
            result.needRehash = Utils::passwordHashNeedsUpgrade(storedHash);
            result.passwordOk = Utils::verifyPassword(password, salt, storedHash);
            return QVariant::fromValue(result);
        },
        [callback](QVariant result) {
            if (callback) callback(result.value<LoginCheckResult>());
        }
    );
}

void UserRepository::upgradePasswordHashAsync(qint64 userId, const QString &password,
                                              std::function<void(bool)> callback,
                                              QPointer<QObject> receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [userId, password]() -> QVariant {
            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            if (!db.isValid() || !db.isOpen()) {
                return QVariant(false);
            }

            const QString salt = Utils::generateSalt();
            const QString hash = Utils::hashPassword(password, salt);

            QSqlQuery query(db);
            query.prepare("UPDATE users SET password_hash = ?, salt = ?, updated_at = ? WHERE id = ?");
            query.addBindValue(hash);
            query.addBindValue(salt);
            query.addBindValue(QDateTime::currentSecsSinceEpoch());
            query.addBindValue(userId);

            const bool success = query.exec();
            if (success) {
                qInfo() << "[UserRepository] 口令哈希已升级为 PBKDF2, userId:" << userId;
            } else {
                qWarning() << "[UserRepository] 升级口令哈希失败:" << query.lastError().text();
            }
            return QVariant(success);
        },
        [callback](QVariant result) {
            if (callback) callback(result.toBool());
        }
    );
}

void UserRepository::getUserIdAsync(const QString &username,
                                    std::function<void(qint64)> callback,
                                    QPointer<QObject> receiver)
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

void UserRepository::updateUserStatusAsync(qint64 userId, int status,
                                           std::function<void(bool)> callback,
                                           QPointer<QObject> receiver)
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

            const bool success = query.exec();
            return QVariant(success);
        },
        [callback](QVariant result) {
            if (callback) callback(result.toBool());
        }
    );
}
