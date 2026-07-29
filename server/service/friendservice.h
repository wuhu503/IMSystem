#ifndef FRIENDSERVICE_H
#define FRIENDSERVICE_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>

class ClientHandler;
class Message;
enum class MessageType : uint16_t;

class FriendService : public QObject
{
    Q_OBJECT

public:
    static FriendService& instance();
    
    // 禁止拷贝和赋值
    FriendService(const FriendService&) = delete;
    FriendService& operator=(const FriendService&) = delete;

    // 处理好友请求
    void handleAddFriend(ClientHandler *client, const Message &msg);
    void handleFriendList(ClientHandler *client, const Message &msg);
    void handleAcceptFriend(ClientHandler *client, const Message &msg);
    void handleRejectFriend(ClientHandler *client, const Message &msg);
    void handleDeleteFriend(ClientHandler *client, const Message &msg);
    void handleSearchUser(ClientHandler *client, const Message &msg);

    // 广播好友状态变化
    void broadcastFriendStatus(qint64 userId, int status);

private:
    FriendService();
    ~FriendService();
    
    // 辅助方法
    Message createResponse(MessageType type, uint32_t sequence, 
                           const QJsonObject &body);
    void sendErrorResponse(ClientHandler *client, MessageType type, 
                          uint32_t sequence, const QString &errorMessage);
    void sendSuccessResponse(ClientHandler *client, MessageType type, 
                            uint32_t sequence, const QJsonObject &data = QJsonObject());
};

#endif // FRIENDSERVICE_H
