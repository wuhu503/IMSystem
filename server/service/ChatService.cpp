#include "ChatService.h"
#include "ClientHandler.h"
#include "Message.h"
#include "FriendRepository.h"
#include "MessageRepository.h"
#include "UserRepository.h"
#include "UserManager.h"
#include "Utils.h"
#include "Reply.h"
#include "Constants.h"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>

ChatService& ChatService::instance()
{
    static ChatService instance;
    return instance;
}

ChatService::ChatService()
{
    qInfo() << "ChatService 创建";
}

ChatService::~ChatService()
{
    qInfo() << "ChatService 销毁";
}

void ChatService::handleTextMessage(ClientHandler *client, const Message &msg)
{
    qInfo() << "处理文本消息";
    
    QJsonObject body = msg.jsonBody();
    QString receiverUsername = body["receiver"].toString();
    QString content = body["content"].toString();
    
    if (receiverUsername.isEmpty() || content.isEmpty()) {
        sendErrorResponse(client, MessageType::RSP_TEXT,
                         msg.sequence(), "接收者或消息内容不能为空");
        return;
    }

    if (content.length() > IMConstants::kMaxTextContentLength) {
        sendErrorResponse(client, MessageType::RSP_TEXT,
                         msg.sequence(), "消息内容过长，最多 "
                             + QString::number(IMConstants::kMaxTextContentLength) + " 个字符");
        return;
    }

    qint64 senderId = client->userId();
    if (senderId == -1) {
        sendErrorResponse(client, MessageType::RSP_TEXT,
                         msg.sequence(), "请先登录");
        return;
    }

    // 用户名在登录时已缓存到连接上，这里无需再同步查库（避免阻塞事件循环）
    const QString senderUsername = client->username();
    
    uint32_t sequence = msg.sequence();
    QPointer<ClientHandler> safeClient(client);
    
    UserRepository::instance().getUserIdAsync(receiverUsername,
        [this, safeClient, senderId, senderUsername, content, sequence, msg](qint64 receiverId) {
            if (!safeClient) return;
            
            if (receiverId == -1) {
                sendErrorResponse(safeClient.data(), MessageType::RSP_TEXT,
                                 sequence, "接收者不存在");
                return;
            }
            
            FriendRepository::instance().isFriendAsync(senderId, receiverId,
                [this, safeClient, senderId, senderUsername, receiverId, content, sequence, msg](bool isFriend) {
                    if (!safeClient) return;
                    
                    if (!isFriend) {
                        sendErrorResponse(safeClient.data(), MessageType::RSP_TEXT,
                                         sequence, "对方不是您的好友，无法发送消息");
                        return;
                    }
                    
                    QString msgId = Utils::generateUUID();
                    const qint64 timestamp = QDateTime::currentSecsSinceEpoch();
                    
                    MessageRepository::instance().saveMessageAsync(
                        msgId, senderId, receiverId,
                        static_cast<int>(MessageType::MSG_TEXT), content, timestamp,
                        [this, safeClient, senderUsername, receiverId, sequence,
                         msgId, timestamp, content](bool success) {
                            if (!safeClient) return;
                            
                            if (success) {
                                QJsonObject ackData;
                                ackData["msg_id"] = msgId;
                                ackData["success"] = true;
                                sendSuccessResponse(safeClient.data(), MessageType::RSP_TEXT, sequence, ackData);
                                
                                // 在线推送成功要立刻标记已投递，
                                // 否则对方下次登录会把这条消息当成离线消息重复推一遍
                                if (forwardMessage(senderUsername, receiverId, content, msgId, timestamp)) {
                                    MessageRepository::instance().markMessageDeliveredAsync(msgId, nullptr, safeClient);
                                }
                            } else {
                                sendErrorResponse(safeClient.data(), MessageType::RSP_TEXT,
                                                 sequence, "消息保存失败");
                            }
                        }, safeClient);
                }, safeClient);
        }, safeClient);
}

void ChatService::handleHistoryRequest(ClientHandler *client, const Message &msg)
{
    qInfo() << "处理历史消息请求";
    
    QJsonObject body = msg.jsonBody();
    QString friendUsername = body["username"].toString();
    int limit = body["limit"].toInt(IMConstants::kDefaultHistoryLimit);
    int offset = body["offset"].toInt(0);
    
    // 校验 limit 和 offset
    if (limit <= 0) limit = 1;
    if (limit > IMConstants::kMaxHistoryLimit) limit = IMConstants::kMaxHistoryLimit;
    if (offset < 0) offset = 0;
    
    qint64 userId = client->userId();
    if (userId == -1) {
        sendErrorResponse(client, MessageType::RSP_HISTORY,
                         msg.sequence(), "请先登录");
        return;
    }
    
    uint32_t sequence = msg.sequence();
    QPointer<ClientHandler> safeClient(client);
    
    UserRepository::instance().getUserIdAsync(friendUsername,
        [this, safeClient, userId, friendUsername, limit, offset, sequence](qint64 friendId) {
            if (!safeClient) return;
            
            if (friendId == -1) {
                sendErrorResponse(safeClient.data(), MessageType::RSP_HISTORY,
                                 sequence, "用户不存在");
                return;
            }
            
            MessageRepository::instance().getChatHistoryAsync(userId, friendId, limit, offset,
                [this, safeClient, friendUsername, sequence](QJsonArray messages) {
                    if (!safeClient) return;
                    
                    QJsonObject data;
                    data["messages"] = messages;
                    data["count"] = messages.size();
                    data["friend_username"] = friendUsername;
                    
                    sendSuccessResponse(safeClient.data(), MessageType::RSP_HISTORY, sequence, data);
                }, safeClient);
        }, safeClient);
}

void ChatService::handleMessageAck(ClientHandler *client, const Message &msg)
{
    // 已读回执：把某个好友发给自己的消息标记为已读
    const QString friendUsername = msg.jsonBody()["username"].toString();
    const qint64 userId = client->userId();

    if (userId == -1 || friendUsername.isEmpty()) {
        return;
    }

    QPointer<ClientHandler> safeClient(client);
    UserRepository::instance().getUserIdAsync(friendUsername,
        [safeClient, userId](qint64 friendId) {
            if (!safeClient || friendId == -1) return;
            MessageRepository::instance().markConversationReadAsync(userId, friendId, nullptr, safeClient);
        }, safeClient);
}

bool ChatService::forwardMessage(const QString &senderUsername, qint64 receiverId,
                                  const QString &content, const QString &msgId, qint64 timestamp)
{
    QPointer<ClientHandler> receiverHandler = UserManager::instance().getHandler(receiverId);

    if (!receiverHandler.isNull()) {
        // 重建转发 body：只保留必要字段，
        // 避免把发送者的 token、receiver 等内部字段泄露给接收方
        QJsonObject body;
        body["sender"] = senderUsername;
        body["content"] = content;
        body["msg_id"] = msgId;
        body["timestamp"] = timestamp;
        
        Message forwardMsg(MessageType::MSG_TEXT);
        forwardMsg.setJsonBody(body);
        
        receiverHandler->sendMessage(forwardMsg);
        qInfo() << "消息已转发:" << senderUsername << "->" << receiverId;
        return true;
    }

    qInfo() << "接收者离线，消息已入库，待其上线后推送:" << senderUsername << "->" << receiverId;
    return false;
}

void ChatService::deliverOfflineMessages(ClientHandler *client)
{
    if (!client || client->userId() == -1) {
        return;
    }

    const qint64 userId = client->userId();
    QPointer<ClientHandler> safeClient(client);

    MessageRepository::instance().getUndeliveredMessagesAsync(userId,
        [safeClient, userId](QJsonArray messages) {
            if (!safeClient || messages.isEmpty()) {
                return;
            }

            qint64 maxMessageId = 0;
            for (const QJsonValue &value : messages) {
                const QJsonObject obj = value.toObject();
                maxMessageId = qMax(maxMessageId, obj["id"].toVariant().toLongLong());

                QJsonObject body;
                body["msg_id"] = obj["msg_id"];
                body["sender"] = obj["sender"];
                body["content"] = obj["content"];
                body["timestamp"] = obj["timestamp"];

                Message offlineMsg(MessageType::MSG_TEXT);
                offlineMsg.setJsonBody(body);
                safeClient->sendMessage(offlineMsg);
            }

            qInfo() << "已推送离线消息" << messages.size() << "条, userId:" << userId;

            // 推送完成后标记已投递，避免下次登录重复推送
            if (maxMessageId > 0) {
                MessageRepository::instance().markMessagesDeliveredAsync(userId, maxMessageId,
                                                                 nullptr, safeClient);
            }
        }, safeClient);
}

void ChatService::sendErrorResponse(ClientHandler *client, MessageType type, 
                                     uint32_t sequence, const QString &errorMessage)
{
    Reply::sendFailure(client, type, sequence, errorMessage, "聊天操作失败:");
}

void ChatService::sendSuccessResponse(ClientHandler *client, MessageType type, 
                                       uint32_t sequence, const QJsonObject &data)
{
    Reply::sendSuccess(client, type, sequence, data);
}
