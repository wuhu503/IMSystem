#ifndef FRIENDSERVICE_H
#define FRIENDSERVICE_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointer>

class ClientHandler;
class Message;
enum class MessageType : uint16_t;

class FriendService : public QObject
{
    Q_OBJECT

public:
    static FriendService& instance();
    
    FriendService(const FriendService&) = delete;
    FriendService& operator=(const FriendService&) = delete;

    void handleAddFriend(ClientHandler *client, const Message &msg);
    void handleFriendList(ClientHandler *client, const Message &msg);
    void handleAcceptFriend(ClientHandler *client, const Message &msg);
    void handleRejectFriend(ClientHandler *client, const Message &msg);
    void handleDeleteFriend(ClientHandler *client, const Message &msg);
    void handleSearchUser(ClientHandler *client, const Message &msg);
    void handlePendingRequests(ClientHandler *client, const Message &msg);

    void broadcastFriendStatus(qint64 userId, int status);

private:
    FriendService();
    ~FriendService();
    
    // 添加好友的异步检查链
    void checkFriendshipAndAdd(qint64 userId, qint64 friendId,
                               const QString &friendUsername, uint32_t sequence,
                               QPointer<ClientHandler> safeClient);
    void checkPendingAndAdd(qint64 userId, qint64 friendId,
                            const QString &friendUsername, uint32_t sequence,
                            QPointer<ClientHandler> safeClient);
    
    Message createResponse(MessageType type, uint32_t sequence, 
                           const QJsonObject &body);
    void sendErrorResponse(ClientHandler *client, MessageType type, 
                          uint32_t sequence, const QString &errorMessage);
    void sendSuccessResponse(ClientHandler *client, MessageType type, 
                            uint32_t sequence, const QJsonObject &data = QJsonObject());
};

#endif // FRIENDSERVICE_H
