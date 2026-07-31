#include "chatservice.h"
#include "clienthandler.h"
#include "message.h"
#include "dbmanager.h"
#include "usermanager.h"
#include "utils.h"
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
        sendErrorResponse(client, MessageType::MSG_ACK, 
                         msg.sequence(), "接收者或消息内容不能为空");
        return;
    }
    
    qint64 senderId = client->userId();
    if (senderId == -1) {
        sendErrorResponse(client, MessageType::MSG_ACK, 
                         msg.sequence(), "请先登录");
        return;
    }
    
    // 在主线程同步获取发送者用户名（仅一次，后续传递给异步链）
    QVariantMap senderInfo = DbManager::instance().getUserInfo(senderId);
    QString senderUsername = senderInfo["username"].toString();
    
    uint32_t sequence = msg.sequence();
    QPointer<ClientHandler> safeClient(client);
    
    DbManager::instance().getUserIdAsync(receiverUsername,
        [this, safeClient, senderId, senderUsername, content, sequence, msg](qint64 receiverId) {
            if (!safeClient) return;
            
            if (receiverId == -1) {
                sendErrorResponse(safeClient.data(), MessageType::MSG_ACK, 
                                 sequence, "接收者不存在");
                return;
            }
            
            DbManager::instance().isFriendAsync(senderId, receiverId,
                [this, safeClient, senderId, senderUsername, receiverId, content, sequence, msg](bool isFriend) {
                    if (!safeClient) return;
                    
                    if (!isFriend) {
                        sendErrorResponse(safeClient.data(), MessageType::MSG_ACK, 
                                         sequence, "对方不是您的好友，无法发送消息");
                        return;
                    }
                    
                    QString msgId = Utils::generateUUID();
                    
                    DbManager::instance().saveMessageAsync(
                        msgId, senderId, receiverId,
                        static_cast<int>(MessageType::MSG_TEXT), content,
                        [this, safeClient, senderId, senderUsername, receiverId, sequence, msgId, msg](bool success) {
                            if (!safeClient) return;
                            
                            if (success) {
                                QJsonObject ackData;
                                ackData["msg_id"] = msgId;
                                ackData["success"] = true;
                                sendSuccessResponse(safeClient.data(), MessageType::MSG_ACK, sequence, ackData);
                                
                                // 使用已获取的 senderUsername，不再查询数据库
                                forwardMessage(senderId, senderUsername, receiverId, msg);
                            } else {
                                sendErrorResponse(safeClient.data(), MessageType::MSG_ACK, 
                                                 sequence, "消息保存失败");
                            }
                        }, safeClient.data());
                }, safeClient.data());
        }, safeClient.data());
}

void ChatService::handleHistoryRequest(ClientHandler *client, const Message &msg)
{
    qInfo() << "处理历史消息请求";
    
    QJsonObject body = msg.jsonBody();
    QString friendUsername = body["username"].toString();
    int limit = body["limit"].toInt(50);
    int offset = body["offset"].toInt(0);
    
    // 校验 limit 和 offset
    if (limit <= 0) limit = 1;
    if (limit > 100) limit = 100;
    if (offset < 0) offset = 0;
    
    qint64 userId = client->userId();
    if (userId == -1) {
        sendErrorResponse(client, MessageType::MSG_HISTORY, 
                         msg.sequence(), "请先登录");
        return;
    }
    
    uint32_t sequence = msg.sequence();
    QPointer<ClientHandler> safeClient(client);
    
    DbManager::instance().getUserIdAsync(friendUsername,
        [this, safeClient, userId, friendUsername, limit, offset, sequence](qint64 friendId) {
            if (!safeClient) return;
            
            if (friendId == -1) {
                sendErrorResponse(safeClient.data(), MessageType::MSG_HISTORY, 
                                 sequence, "用户不存在");
                return;
            }
            
            DbManager::instance().getChatHistoryAsync(userId, friendId, limit, offset,
                [this, safeClient, friendUsername, sequence](QJsonArray messages) {
                    if (!safeClient) return;
                    
                    QJsonObject data;
                    data["messages"] = messages;
                    data["count"] = messages.size();
                    data["friend_username"] = friendUsername;
                    
                    sendSuccessResponse(safeClient.data(), MessageType::MSG_HISTORY, sequence, data);
                }, safeClient.data());
        }, safeClient.data());
}

void ChatService::handleMessageAck(ClientHandler *client, const Message &msg)
{
    Q_UNUSED(client);
    Q_UNUSED(msg);
    qInfo() << "收到消息确认";
}

void ChatService::forwardMessage(qint64 senderId, const QString &senderUsername, 
                                  qint64 receiverId, const Message &msg)
{
    ClientHandler *receiverHandler = UserManager::instance().getHandler(receiverId);
    
    if (receiverHandler) {
        QJsonObject body = msg.jsonBody();
        body["sender"] = senderUsername;
        
        Message forwardMsg(MessageType::MSG_TEXT);
        forwardMsg.setJsonBody(body);
        
        receiverHandler->sendMessage(forwardMsg);
        qInfo() << "消息已转发:" << senderUsername << "->" << receiverId;
    } else {
        qInfo() << "接收者离线，消息已存储:" << senderId << "->" << receiverId;
    }
}

Message ChatService::createResponse(MessageType type, uint32_t sequence, 
                                     const QJsonObject &body)
{
    Message msg(type);
    msg.setSequence(sequence);
    msg.setJsonBody(body);
    return msg;
}

void ChatService::sendErrorResponse(ClientHandler *client, MessageType type, 
                                     uint32_t sequence, const QString &errorMessage)
{
    QJsonObject body;
    body["success"] = false;
    body["message"] = errorMessage;
    
    Message response = createResponse(type, sequence, body);
    client->sendMessage(response);
    
    qWarning() << "聊天操作失败:" << errorMessage;
}

void ChatService::sendSuccessResponse(ClientHandler *client, MessageType type, 
                                       uint32_t sequence, const QJsonObject &data)
{
    QJsonObject body;
    body["success"] = true;
    body["message"] = "操作成功";
    
    for (auto it = data.begin(); it != data.end(); ++it) {
        body[it.key()] = it.value();
    }
    
    Message response = createResponse(type, sequence, body);
    client->sendMessage(response);
}
