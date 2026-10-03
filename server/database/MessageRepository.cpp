#include "MessageRepository.h"

#include "Constants.h"
#include "DbConnectionHelper.h"
#include "TaskRunner.h"

#include <QDebug>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

MessageRepository& MessageRepository::instance()
{
    static MessageRepository instance;
    return instance;
}

void MessageRepository::saveMessageAsync(const QString &msgId, qint64 senderId, qint64 receiverId,
                                         int type, const QString &content, qint64 timestamp,
                                         std::function<void(bool)> callback, QPointer<QObject> receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [msgId, senderId, receiverId, type, content, timestamp]() -> QVariant {
            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            QSqlQuery query(db);
            query.prepare(
                "INSERT INTO messages (msg_id, sender_id, receiver_id, type, content, timestamp, is_read) "
                "VALUES (?, ?, ?, ?, ?, ?, 0)"
            );
            query.addBindValue(msgId);
            query.addBindValue(senderId);
            query.addBindValue(receiverId);
            query.addBindValue(type);
            query.addBindValue(content);
            query.addBindValue(timestamp);

            const bool success = query.exec();
            if (!success) {
                qCritical() << "[MessageRepository] 保存消息失败:" << query.lastError().text();
            }
            return QVariant(success);
        },
        [callback](QVariant result) {
            if (callback) callback(result.toBool());
        }
    );
}

void MessageRepository::getChatHistoryAsync(qint64 userId, qint64 friendId, int limit, int offset,
                                            std::function<void(QJsonArray)> callback,
                                            QPointer<QObject> receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [userId, friendId, limit, offset]() -> QVariant {
            QJsonArray messages;
            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            QSqlQuery query(db);
            // UNION ALL 的两个分支各自能走 (sender_id, receiver_id, timestamp) 复合索引，
            // 比 OR 连接快得多；同一秒的消息用 id 兜底排序
            query.prepare(
                "SELECT m.msg_id, m.sender_id, m.receiver_id, m.type, m.content, "
                "m.timestamp, m.is_read, "
                "u1.username as sender_name, u2.username as receiver_name "
                "FROM ("
                "  SELECT * FROM messages WHERE sender_id = ? AND receiver_id = ? "
                "  UNION ALL "
                "  SELECT * FROM messages WHERE sender_id = ? AND receiver_id = ? "
                ") AS m "
                "LEFT JOIN users u1 ON m.sender_id = u1.id "
                "LEFT JOIN users u2 ON m.receiver_id = u2.id "
                "ORDER BY m.timestamp DESC, m.id DESC "
                "LIMIT ? OFFSET ?"
            );
            query.addBindValue(userId);
            query.addBindValue(friendId);
            query.addBindValue(friendId);
            query.addBindValue(userId);
            query.addBindValue(limit);
            query.addBindValue(offset);

            if (query.exec()) {
                while (query.next()) {
                    QJsonObject msgObj;
                    msgObj["msg_id"] = query.value("msg_id").toString();
                    msgObj["sender_id"] = query.value("sender_id").toLongLong();
                    msgObj["receiver_id"] = query.value("receiver_id").toLongLong();
                    msgObj["sender_name"] = query.value("sender_name").toString();
                    msgObj["receiver_name"] = query.value("receiver_name").toString();
                    msgObj["type"] = query.value("type").toInt();
                    msgObj["content"] = query.value("content").toString();
                    msgObj["timestamp"] = query.value("timestamp").toLongLong();
                    msgObj["is_read"] = query.value("is_read").toInt();
                    messages.append(msgObj);
                }
            }

            return QVariant::fromValue(messages);
        },
        [callback](QVariant result) {
            if (callback) callback(result.value<QJsonArray>());
        }
    );
}

void MessageRepository::getUndeliveredMessagesAsync(qint64 userId,
                                                    std::function<void(QJsonArray)> callback,
                                                    QPointer<QObject> receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [userId]() -> QVariant {
            QJsonArray messages;
            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            if (!db.isValid() || !db.isOpen()) {
                return QVariant::fromValue(messages);
            }

            QSqlQuery query(db);
            query.prepare(
                "SELECT m.id, m.msg_id, m.sender_id, m.content, m.timestamp, "
                "u.username AS sender_name "
                "FROM messages m "
                "JOIN users u ON m.sender_id = u.id "
                "WHERE m.receiver_id = ? AND m.delivered = 0 "
                "ORDER BY m.timestamp ASC, m.id ASC "
                "LIMIT ?"
            );
            query.addBindValue(userId);
            query.addBindValue(IMConstants::kOfflineMessageBatch);

            if (query.exec()) {
                while (query.next()) {
                    QJsonObject msgObj;
                    msgObj["id"] = query.value("id").toLongLong();
                    msgObj["msg_id"] = query.value("msg_id").toString();
                    msgObj["sender_id"] = query.value("sender_id").toLongLong();
                    msgObj["sender"] = query.value("sender_name").toString();
                    msgObj["content"] = query.value("content").toString();
                    msgObj["timestamp"] = query.value("timestamp").toLongLong();
                    messages.append(msgObj);
                }
            } else {
                qWarning() << "[MessageRepository] 查询离线消息失败:" << query.lastError().text();
            }

            return QVariant::fromValue(messages);
        },
        [callback](QVariant result) {
            if (callback) callback(result.value<QJsonArray>());
        }
    );
}

void MessageRepository::markMessagesDeliveredAsync(qint64 receiverId, qint64 maxMessageId,
                                                   std::function<void(bool)> callback,
                                                   QPointer<QObject> receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [receiverId, maxMessageId]() -> QVariant {
            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            QSqlQuery query(db);
            query.prepare("UPDATE messages SET delivered = 1 "
                          "WHERE receiver_id = ? AND delivered = 0 AND id <= ?");
            query.addBindValue(receiverId);
            query.addBindValue(maxMessageId);

            const bool success = query.exec();
            if (!success) {
                qWarning() << "[MessageRepository] 标记离线消息已投递失败:" << query.lastError().text();
            }
            return QVariant(success);
        },
        [callback](QVariant result) {
            if (callback) callback(result.toBool());
        }
    );
}

void MessageRepository::markMessageDeliveredAsync(const QString &msgId,
                                                  std::function<void(bool)> callback,
                                                  QPointer<QObject> receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [msgId]() -> QVariant {
            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            QSqlQuery query(db);
            query.prepare("UPDATE messages SET delivered = 1 WHERE msg_id = ?");
            query.addBindValue(msgId);

            const bool success = query.exec();
            if (!success) {
                qWarning() << "[MessageRepository] 标记消息已投递失败:" << query.lastError().text();
            }
            return QVariant(success);
        },
        [callback](QVariant result) {
            if (callback) callback(result.toBool());
        }
    );
}

void MessageRepository::markConversationReadAsync(qint64 userId, qint64 friendId,
                                                  std::function<void(bool)> callback,
                                                  QPointer<QObject> receiver)
{
    TaskRunner::instance().runDbTask(
        receiver,
        [userId, friendId]() -> QVariant {
            QSqlDatabase db = DbConnectionHelper::threadLocalConnection();
            QSqlQuery query(db);
            query.prepare("UPDATE messages SET is_read = 1 "
                          "WHERE receiver_id = ? AND sender_id = ? AND is_read = 0");
            query.addBindValue(userId);
            query.addBindValue(friendId);

            const bool success = query.exec();
            if (!success) {
                qWarning() << "[MessageRepository] 标记会话已读失败:" << query.lastError().text();
            }
            return QVariant(success);
        },
        [callback](QVariant result) {
            if (callback) callback(result.toBool());
        }
    );
}
