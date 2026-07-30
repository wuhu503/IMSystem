#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <QObject>
#include <QJsonObject>

class ClientHandler;
class Message;
enum class MessageType : uint16_t;

class ChatService : public QObject
{
    Q_OBJECT

public:
    static ChatService& instance();
    
    // 禁止拷贝和赋值
    ChatService(const ChatService&) = delete;
    ChatService& operator=(const ChatService&) = delete;

    // 处理聊天消息
    void handleTextMessage(ClientHandler *client, const Message &msg);
    void handleHistoryRequest(ClientHandler *client, const Message &msg);
    void handleMessageAck(ClientHandler *client, const Message &msg);

private:
    ChatService();
    ~ChatService();
    
    // 辅助方法
    Message createResponse(MessageType type, uint32_t sequence, 
                           const QJsonObject &body);
    void sendErrorResponse(ClientHandler *client, MessageType type, 
                          uint32_t sequence, const QString &errorMessage);
    void sendSuccessResponse(ClientHandler *client, MessageType type, 
                            uint32_t sequence, const QJsonObject &data = QJsonObject());
    
    // 转发消息给目标用户
    void forwardMessage(qint64 senderId, qint64 receiverId, const Message &msg);
};

#endif // CHATSERVICE_H
