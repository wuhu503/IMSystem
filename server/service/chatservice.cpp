#include "chatservice.h"
#include "clienthandler.h"
#include "message.h"
#include "dbmanager.h"
#include "usermanager.h"
#include "utils.h"
#include <QJsonArray>
#include <QJsonObject>

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
    
    qint64 receiverId = DbManager::instance().getUserId(receiverUsername);
    if (receiverId == -1) {
        sendErrorResponse(client, MessageType::MSG_ACK, 
                         msg.sequence(), "接收者不存在");
        return;
    }
    
    if (!DbManager::instance().isFriend(senderId, receiverId)) {
        sendErrorResponse(client, MessageType::MSG_ACK, 
                         msg.sequence(), "对方不是您的好友，无法发送消息");
        return;
    }
    
    QString msgId = Utils::generateUUID();
    
    if (!DbManager::instance().saveMessage(msgId, senderId, receiverId, 
                                            static_cast<int>(MessageType::MSG_TEXT), 
                                            content)) {
        sendErrorResponse(client, MessageType::MSG_ACK, 
                         msg.sequence(), "消息保存失败");
        return;
    }
    
    QJsonObject ackData;
    ackData["msg_id"] = msgId;
    ackData["success"] = true;
    sendSuccessResponse(client, MessageType::MSG_ACK, msg.sequence(), ackData);
    
    forwardMessage(senderId, receiverId, msg);
}

void ChatService::handleHistoryRequest(ClientHandler *client, const Message &msg)
{
    qInfo() << "处理历史消息请求";
    
    QJsonObject body = msg.jsonBody();
    QString friendUsername = body["username"].toString();
    int limit = body["limit"].toInt(50);
    int offset = body["offset"].toInt(0);
    
    qint64 userId = client->userId();
    if (userId == -1) {
        sendErrorResponse(client, MessageType::MSG_HISTORY, 
                         msg.sequence(), "请先登录");
        return;
    }
    
    qint64 friendId = DbManager::instance().getUserId(friendUsername);
    if (friendId == -1) {
        sendErrorResponse(client, MessageType::MSG_HISTORY, 
                         msg.sequence(), "用户不存在");
        return;
    }
    
    QJsonArray messages = DbManager::instance().getChatHistory(userId, friendId, limit, offset);
    
    QJsonObject data;
    data["messages"] = messages;
    data["count"] = messages.size();
    data["friend_username"] = friendUsername;
    
    sendSuccessResponse(client, MessageType::MSG_HISTORY, msg.sequence(), data);
}

void ChatService::handleMessageAck(ClientHandler *client, const Message &msg)
{
    Q_UNUSED(client);
    Q_UNUSED(msg);
    qInfo() << "收到消息确认";
}

// 核心：消息转发
void ChatService::forwardMessage(qint64 senderId, qint64 receiverId, const Message &msg)
{
    ClientHandler *receiverHandler = UserManager::instance().getHandler(receiverId);
    
    if (receiverHandler) {
        // 获取发送者用户名
        QVariantMap senderInfo = DbManager::instance().getUserInfo(senderId);
        QString senderUsername = senderInfo["username"].toString();
        
        // 构造转发消息
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
