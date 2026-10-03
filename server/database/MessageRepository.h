#ifndef MESSAGEREPOSITORY_H
#define MESSAGEREPOSITORY_H

#include <QJsonArray>
#include <QPointer>
#include <QString>
#include <functional>

// messages 表的读写：消息入库、聊天历史、离线投递、已读回执
class MessageRepository
{
public:
    static MessageRepository& instance();

    MessageRepository(const MessageRepository&) = delete;
    MessageRepository& operator=(const MessageRepository&) = delete;

    void saveMessageAsync(const QString &msgId, qint64 senderId, qint64 receiverId,
                          int type, const QString &content, qint64 timestamp,
                          std::function<void(bool)> callback, QPointer<QObject> receiver);

    void getChatHistoryAsync(qint64 userId, qint64 friendId, int limit, int offset,
                             std::function<void(QJsonArray)> callback,
                             QPointer<QObject> receiver);

    // 未投递的离线消息（按时间正序）
    void getUndeliveredMessagesAsync(qint64 userId,
                                     std::function<void(QJsonArray)> callback,
                                     QPointer<QObject> receiver);

    // 离线补推完成后按 (接收者, 最大消息 id) 批量标记已投递
    void markMessagesDeliveredAsync(qint64 receiverId, qint64 maxMessageId,
                                    std::function<void(bool)> callback,
                                    QPointer<QObject> receiver);

    // 在线转发成功后标记单条消息已投递（避免下次登录重复补推）
    void markMessageDeliveredAsync(const QString &msgId,
                                   std::function<void(bool)> callback,
                                   QPointer<QObject> receiver);

    // 已读回执：把某个好友发给自己的消息标记为已读
    void markConversationReadAsync(qint64 userId, qint64 friendId,
                                   std::function<void(bool)> callback,
                                   QPointer<QObject> receiver);

private:
    MessageRepository() = default;
    ~MessageRepository() = default;
};

#endif // MESSAGEREPOSITORY_H
