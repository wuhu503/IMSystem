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
    
    ChatService(const ChatService&) = delete;
    ChatService& operator=(const ChatService&) = delete;

    void handleTextMessage(ClientHandler *client, const Message &msg);
    void handleHistoryRequest(ClientHandler *client, const Message &msg);
    void handleMessageAck(ClientHandler *client, const Message &msg);

private:
    ChatService();
    ~ChatService();
    
    Message createResponse(MessageType type, uint32_t sequence, 
                           const QJsonObject &body);
    void sendErrorResponse(ClientHandler *client, MessageType type, 
                          uint32_t sequence, const QString &errorMessage);
    void sendSuccessResponse(ClientHandler *client, MessageType type, 
                            uint32_t sequence, const QJsonObject &data = QJsonObject());
    
    // 转发消息给目标用户（传入发送者用户名，避免同步查询）
    void forwardMessage(qint64 senderId, const QString &senderUsername, 
                        qint64 receiverId, const Message &msg);
};

#endif // CHATSERVICE_H
