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

    // 登录成功后把库里未投递的离线消息推给该连接
    void deliverOfflineMessages(ClientHandler *client);

private:
    ChatService();
    ~ChatService();
    
    void sendErrorResponse(ClientHandler *client, MessageType type, 
                          uint32_t sequence, const QString &errorMessage);
    void sendSuccessResponse(ClientHandler *client, MessageType type, 
                            uint32_t sequence, const QJsonObject &data = QJsonObject());
    
    // 转发消息给目标用户（传入发送者用户名，避免同步查询）
    // 返回值：接收者在线并已推送为 true；离线（消息入库待补推）为 false
    bool forwardMessage(const QString &senderUsername, qint64 receiverId,
                        const QString &content, const QString &msgId, qint64 timestamp);
};

#endif // CHATSERVICE_H
