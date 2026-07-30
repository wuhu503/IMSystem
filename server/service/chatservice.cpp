#include "chatservice.h"
#include "clienthandler.h"
#include "message.h"
#include "dbmanager.h"
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

// 处理文本消息
void ChatService::handleTextMessage(ClientHandler *client, const Message &msg)
{
    qInfo() << "处理文本消息";
    
    QJsonObject body = msg.jsonBody();
    QString receiverUsername = body["receiver"].toString();
    QString content = body["content"].toString();
    
    // 验证参数
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
    
    // 生成消息ID
    QString msgId = Utils::generateUUID();
    
    // 存储消息到数据库
    if (!DbManager::instance().saveMessage(msgId, senderId, receiverId, 
                                            static_cast<int>(MessageType::MSG_TEXT), 
                                            content)) {
        sendErrorResponse(client, MessageType::MSG_ACK, 
                         msg.sequence(), "消息保存失败");
        return;
    }
    
    // 返回消息确认
    QJsonObject ackData;
    ackData["msg_id"] = msgId;
    ackData["success"] = true;
    sendSuccessResponse(client, MessageType::MSG_ACK, msg.sequence(), ackData);
    
    // 转发消息给接收者
    forwardMessage(senderId, receiverId, msg);
    
    qInfo() << "消息已发送:" << senderId << "->" << receiverId;
}

// 处理历史消息请求
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
    
    // 获取历史消息
    QJsonArray messages = DbManager::instance().getChatHistory(userId, friendId, limit, offset);
    
    QJsonObject data;
    data["messages"] = messages;
    data["count"] = messages.size();
    data["friend_username"] = friendUsername;
    
    sendSuccessResponse(client, MessageType::MSG_HISTORY, msg.sequence(), data);
    
    qInfo() << "获取历史消息:" << userId << "和" << friendId << "，共" << messages.size() << "条";
}

// 处理消息确认
void ChatService::handleMessageAck(ClientHandler *client, const Message &msg)
{
    qInfo() << "收到消息确认";
    // TODO: 更新消息状态为已读
}

// 转发消息给目标用户
void ChatService::forwardMessage(qint64 senderId, qint64 receiverId, const Message &msg)
{
    // TODO: 实现消息转发，需要维护在线用户列表
    // 当前实现：如果接收者在线，直接转发
    qInfo() << "转发消息:" << senderId << "->" << receiverId;
}

// 创建响应消息
Message ChatService::createResponse(MessageType type, uint32_t sequence, 
                                     const QJsonObject &body)
{
    Message msg(type);
    msg.setSequence(sequence);
    msg.setJsonBody(body);
    return msg;
}

// 发送错误响应
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

// 发送成功响应
void ChatService::sendSuccessResponse(ClientHandler *client, MessageType type, 
                                       uint32_t sequence, const QJsonObject &data)
{
    QJsonObject body;
    body["success"] = true;
    body["message"] = "操作成功";
    
    // 合并数据
    for (auto it = data.begin(); it != data.end(); ++it) {
        body[it.key()] = it.value();
    }
    
    Message response = createResponse(type, sequence, body);
    client->sendMessage(response);
}
