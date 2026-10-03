#include "FriendRepository.h"

#include "Constants.h"
#include "DbConnectionHelper.h"
#include "TaskRunner.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

FriendRepository& FriendRepository::instance()
{
    static FriendRepository instance;
    return instance;
}

void FriendRepository::getFriendListAsync(qint64 userId,
                                          std::function<void(QJsonArray)> callback,
                                          QPointer<QObject> receiver)
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
                "WHERE f.friend_id = ? AND f.status = 1 "
                "ORDER BY username COLLATE NOCASE"
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

            return QVariant::fromValue(friends);
        },
        [callback](QVariant result) {
            if (callback) callback(result.value<QJsonArray>());
        }
    );
}

void FriendRepository::isFriendAsync(qint64 userId, qint64 friendId,
                                     std::function<void(bool)> callback,
                                     QPointer<QObject> receiver)
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

void FriendRepository::searchUsersAsync(const QString &keyword, qint64 excludeUserId,
                                        std::function<void(QJsonArray)> callback,
                                        QPointer<QObject> receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [keyword, excludeUserId]() -> QVariant {
            QJsonArray users;
            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            QSqlQuery query(db);

            // 转义 LIKE 通配符，否则用户输入 % 或 _ 会变成“匹配任意内容”
            QString escaped = keyword;
            escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
            escaped.replace(QLatin1Char('%'), QStringLiteral("\\%"));
            escaped.replace(QLatin1Char('_'), QStringLiteral("\\_"));
            const QString pattern = QStringLiteral("%1%").arg(escaped);

            query.prepare(
                "SELECT id, username, nickname, avatar, status "
                "FROM users "
                "WHERE (username LIKE ? ESCAPE '\\' OR nickname LIKE ? ESCAPE '\\') AND id != ? "
                "ORDER BY username COLLATE NOCASE "
                "LIMIT ?"
            );
            query.addBindValue(pattern);
            query.addBindValue(pattern);
            query.addBindValue(excludeUserId);
            query.addBindValue(IMConstants::kMaxSearchResults);

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

            return QVariant::fromValue(users);
        },
        [callback](QVariant result) {
            if (callback) callback(result.value<QJsonArray>());
        }
    );
}

void FriendRepository::getPendingFriendRequestsAsync(qint64 userId,
                                                     std::function<void(QJsonArray)> callback,
                                                     QPointer<QObject> receiver)
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

            return QVariant::fromValue(requests);
        },
        [callback](QVariant result) {
            if (callback) callback(result.value<QJsonArray>());
        }
    );
}

void FriendRepository::addFriendRequestAsync(qint64 userId, qint64 friendId,
                                             std::function<void(AddFriendResult)> callback,
                                             QPointer<QObject> receiver)
{
    // 查重（两个方向）与写入放进同一个事务，避免双方同时互加时写出两条待确认记录
    TaskRunner::instance().runDbTask(
        receiver,
        [userId, friendId]() -> QVariant {
            auto fail = [](AddFriendResult r) { return QVariant(static_cast<int>(r)); };

            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            if (!db.isValid() || !db.isOpen()) {
                return fail(AddFriendResult::Failed);
            }

            if (!DbConnectionHelper::beginImmediate(db)) {
                return fail(AddFriendResult::Failed);
            }

            QSqlQuery query(db);
            query.prepare(
                "SELECT user_id, status FROM friendships "
                "WHERE (user_id = ? AND friend_id = ?) OR (user_id = ? AND friend_id = ?)"
            );
            query.addBindValue(userId);
            query.addBindValue(friendId);
            query.addBindValue(friendId);
            query.addBindValue(userId);

            if (!query.exec()) {
                DbConnectionHelper::rollback(db);
                qWarning() << "[FriendRepository] 查询好友关系失败:" << query.lastError().text();
                return fail(AddFriendResult::Failed);
            }

            AddFriendResult pendingResult = AddFriendResult::Success;
            bool hasExisting = false;
            while (query.next()) {
                hasExisting = true;
                if (query.value("status").toInt() == 1) {
                    DbConnectionHelper::rollback(db);
                    return fail(AddFriendResult::AlreadyFriend);
                }
                // status == 0：谁发起的决定提示语
                if (query.value("user_id").toLongLong() == userId) {
                    pendingResult = AddFriendResult::AlreadyRequested;
                } else {
                    pendingResult = AddFriendResult::ReverseRequested;
                }
            }

            if (hasExisting) {
                DbConnectionHelper::rollback(db);
                return fail(pendingResult);
            }

            QSqlQuery insertQuery(db);
            insertQuery.prepare(
                "INSERT INTO friendships (user_id, friend_id, status, created_at) "
                "VALUES (?, ?, 0, ?)"
            );
            insertQuery.addBindValue(userId);
            insertQuery.addBindValue(friendId);
            insertQuery.addBindValue(QDateTime::currentSecsSinceEpoch());

            if (!insertQuery.exec()) {
                DbConnectionHelper::rollback(db);
                qWarning() << "[FriendRepository] 添加好友请求失败:" << insertQuery.lastError().text();
                return fail(AddFriendResult::Failed);
            }

            if (!DbConnectionHelper::commit(db)) {
                DbConnectionHelper::rollback(db);
                return fail(AddFriendResult::Failed);
            }

            return fail(AddFriendResult::Success);
        },
        [callback](QVariant result) {
            if (callback) {
                callback(static_cast<AddFriendResult>(result.toInt()));
            }
        }
    );
}

void FriendRepository::acceptFriendRequestAsync(qint64 userId, qint64 friendId,
                                                std::function<void(bool)> callback,
                                                QPointer<QObject> receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [userId, friendId]() -> QVariant {
            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            bool success = false;

            if (DbConnectionHelper::beginImmediate(db)) {
                QSqlQuery query(db);
                query.prepare(
                    "UPDATE friendships SET status = 1 "
                    "WHERE user_id = ? AND friend_id = ? AND status = 0"
                );
                query.addBindValue(userId);
                query.addBindValue(friendId);
                success = query.exec() && query.numRowsAffected() > 0;

                // 补写反向好友关系，保证双方列表都能查到
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
                    DbConnectionHelper::commit(db);
                } else {
                    DbConnectionHelper::rollback(db);
                }
            }

            if (!success) {
                qCritical() << "[FriendRepository] 接受好友请求失败";
            }
            return QVariant(success);
        },
        [callback](QVariant result) {
            if (callback) callback(result.toBool());
        }
    );
}

void FriendRepository::rejectFriendRequestAsync(qint64 userId, qint64 friendId,
                                                std::function<void(bool)> callback,
                                                QPointer<QObject> receiver)
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

            const bool success = query.exec() && query.numRowsAffected() > 0;
            if (!success) {
                qCritical() << "[FriendRepository] 拒绝好友请求失败:" << query.lastError().text();
            }
            return QVariant(success);
        },
        [callback](QVariant result) {
            if (callback) callback(result.toBool());
        }
    );
}

void FriendRepository::deleteFriendAsync(qint64 userId, qint64 friendId,
                                         std::function<void(bool)> callback,
                                         QPointer<QObject> receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [userId, friendId]() -> QVariant {
            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            bool success = false;

            if (DbConnectionHelper::beginImmediate(db)) {
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

                // 同时删除双方聊天记录（幂等删除，0 行也视为成功）
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
                    DbConnectionHelper::commit(db);
                } else {
                    DbConnectionHelper::rollback(db);
                }
            }

            if (!success) {
                qCritical() << "[FriendRepository] 删除好友失败";
            }
            return QVariant(success);
        },
        [callback](QVariant result) {
            if (callback) callback(result.toBool());
        }
    );
}
