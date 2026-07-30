#include "friendservice.h"
#include "clienthandler.h"
#include "message.h"
#include "dbmanager.h"

FriendService& FriendService::instance()
{
    static FriendService instance;
    return instance;
}

FriendService::FriendService()
{
    qInfo() << "FriendService 创建";
}

FriendService::~FriendService()
{
    qInfo() << "FriendService 销毁";
}

void FriendService::handleAddFriend(ClientHandler *client, const Message &msg)
{
    qInfo() << "处理添加好友请求";
    
    QJsonObject body = msg.jsonBody();
    QString friendUsername = body["username"].toString();
    
    if (friendUsername.isEmpty()) {
        sendErrorResponse(client, MessageType::RSP_ADD_FRIEND, 
                         msg.sequence(), "好友用户名不能为空");
        return;
    }
    
    qint64 userId = client->userId();
    if (userId == -1) {
        sendErrorResponse(client, MessageType::RSP_ADD_FRIEND, 
                         msg.sequence(), "请先登录");
        return;
    }
    
    qint64 friendId = DbManager::instance().getUserId(friendUsername);
    if (friendId == -1) {
        sendErrorResponse(client, MessageType::RSP_ADD_FRIEND, 
                         msg.sequence(), "用户不存在");
        return;
    }
    
    if (userId == friendId) {
        sendErrorResponse(client, MessageType::RSP_ADD_FRIEND, 
                         msg.sequence(), "不能添加自己为好友");
        return;
    }
    
    if (DbManager::instance().isFriend(userId, friendId)) {
        sendErrorResponse(client, MessageType::RSP_ADD_FRIEND, 
                         msg.sequence(), "已经是好友关系");
        return;
    }
    
    if (DbManager::instance().hasPendingFriendRequest(userId, friendId)) {
        sendErrorResponse(client, MessageType::RSP_ADD_FRIEND, 
                         msg.sequence(), "已发送过好友请求，请等待对方处理");
        return;
    }
    
    if (!DbManager::instance().addFriendRequest(userId, friendId)) {
        sendErrorResponse(client, MessageType::RSP_ADD_FRIEND, 
                         msg.sequence(), "添加好友失败，请稍后重试");
        return;
    }
    
    QJsonObject data;
    data["friend_username"] = friendUsername;
    data["message"] = "好友请求已发送";
    sendSuccessResponse(client, MessageType::RSP_ADD_FRIEND, msg.sequence(), data);
    
    qInfo() << "用户" << userId << "向" << friendId << "发送好友请求";
}

void FriendService::handleFriendList(ClientHandler *client, const Message &msg)
{
    qInfo() << "处理获取好友列表";
    
    qint64 userId = client->userId();
    if (userId == -1) {
        sendErrorResponse(client, MessageType::RSP_FRIEND_LIST, 
                         msg.sequence(), "请先登录");
        return;
    }
    
    QJsonArray friends = DbManager::instance().getFriendList(userId);
    
    QJsonObject data;
    data["friends"] = friends;
    data["count"] = friends.size();
    
    sendSuccessResponse(client, MessageType::RSP_FRIEND_LIST, msg.sequence(), data);
}

void FriendService::handleAcceptFriend(ClientHandler *client, const Message &msg)
{
    qInfo() << "处理接受好友请求";
    
    QJsonObject body = msg.jsonBody();
    QString friendUsername = body["username"].toString();
    
    qint64 userId = client->userId();
    if (userId == -1) {
        sendErrorResponse(client, MessageType::RSP_ACCEPT_FRIEND, 
                         msg.sequence(), "请先登录");
        return;
    }
    
    qint64 friendId = DbManager::instance().getUserId(friendUsername);
    if (friendId == -1) {
        sendErrorResponse(client, MessageType::RSP_ACCEPT_FRIEND, 
                         msg.sequence(), "用户不存在");
        return;
    }
    
    if (!DbManager::instance().acceptFriendRequest(friendId, userId)) {
        sendErrorResponse(client, MessageType::RSP_ACCEPT_FRIEND, 
                         msg.sequence(), "接受好友请求失败");
        return;
    }
    
    QJsonObject data;
    data["friend_username"] = friendUsername;
    data["message"] = "已接受好友请求";
    sendSuccessResponse(client, MessageType::RSP_ACCEPT_FRIEND, msg.sequence(), data);
}

void FriendService::handleRejectFriend(ClientHandler *client, const Message &msg)
{
    qInfo() << "处理拒绝好友请求";
    
    QJsonObject body = msg.jsonBody();
    QString friendUsername = body["username"].toString();
    
    qint64 userId = client->userId();
    if (userId == -1) {
        sendErrorResponse(client, MessageType::RSP_REJECT_FRIEND, 
                         msg.sequence(), "请先登录");
        return;
    }
    
    qint64 friendId = DbManager::instance().getUserId(friendUsername);
    if (friendId == -1) {
        sendErrorResponse(client, MessageType::RSP_REJECT_FRIEND, 
                         msg.sequence(), "用户不存在");
        return;
    }
    
    if (!DbManager::instance().rejectFriendRequest(friendId, userId)) {
        sendErrorResponse(client, MessageType::RSP_REJECT_FRIEND, 
                         msg.sequence(), "拒绝好友请求失败");
        return;
    }
    
    QJsonObject data;
    data["friend_username"] = friendUsername;
    data["message"] = "已拒绝好友请求";
    sendSuccessResponse(client, MessageType::RSP_REJECT_FRIEND, msg.sequence(), data);
}

void FriendService::handleDeleteFriend(ClientHandler *client, const Message &msg)
{
    qInfo() << "处理删除好友";
    
    QJsonObject body = msg.jsonBody();
    QString friendUsername = body["username"].toString();
    
    qint64 userId = client->userId();
    if (userId == -1) {
        sendErrorResponse(client, MessageType::RSP_DELETE_FRIEND, 
                         msg.sequence(), "请先登录");
        return;
    }
    
    qint64 friendId = DbManager::instance().getUserId(friendUsername);
    if (friendId == -1) {
        sendErrorResponse(client, MessageType::RSP_DELETE_FRIEND, 
                         msg.sequence(), "用户不存在");
        return;
    }
    
    if (!DbManager::instance().deleteFriend(userId, friendId)) {
        sendErrorResponse(client, MessageType::RSP_DELETE_FRIEND, 
                         msg.sequence(), "删除好友失败");
        return;
    }
    
    QJsonObject data;
    data["friend_username"] = friendUsername;
    data["message"] = "已删除好友";
    sendSuccessResponse(client, MessageType::RSP_DELETE_FRIEND, msg.sequence(), data);
}

void FriendService::handleSearchUser(ClientHandler *client, const Message &msg)
{
    qInfo() << "处理搜索用户";
    
    QJsonObject body = msg.jsonBody();
    QString keyword = body["keyword"].toString();
    
    if (keyword.isEmpty()) {
        sendErrorResponse(client, MessageType::RSP_SEARCH_USER, 
                         msg.sequence(), "搜索关键词不能为空");
        return;
    }
    
    qint64 userId = client->userId();
    if (userId == -1) {
        sendErrorResponse(client, MessageType::RSP_SEARCH_USER, 
                         msg.sequence(), "请先登录");
        return;
    }
    
    QJsonArray users = DbManager::instance().searchUsers(keyword, userId);
    
    QJsonObject data;
    data["users"] = users;
    data["count"] = users.size();
    
    sendSuccessResponse(client, MessageType::RSP_SEARCH_USER, msg.sequence(), data);
}

void FriendService::handlePendingRequests(ClientHandler *client, const Message &msg)
{
    qInfo() << "处理获取待处理的好友请求";
    
    qint64 userId = client->userId();
    if (userId == -1) {
        sendErrorResponse(client, MessageType::RSP_PENDING_REQUESTS, 
                         msg.sequence(), "请先登录");
        return;
    }
    
    QJsonArray requests = DbManager::instance().getPendingFriendRequests(userId);
    
    QJsonObject data;
    data["requests"] = requests;
    data["count"] = requests.size();
    
    sendSuccessResponse(client, MessageType::RSP_PENDING_REQUESTS, msg.sequence(), data);
}

void FriendService::broadcastFriendStatus(qint64 userId, int status)
{
    Q_UNUSED(userId);
    Q_UNUSED(status);
    qInfo() << "广播好友状态变化(待实现)";
}

Message FriendService::createResponse(MessageType type, uint32_t sequence, 
                                       const QJsonObject &body)
{
    Message msg(type);
    msg.setSequence(sequence);
    msg.setJsonBody(body);
    return msg;
}

void FriendService::sendErrorResponse(ClientHandler *client, MessageType type, 
                                       uint32_t sequence, const QString &errorMessage)
{
    QJsonObject body;
    body["success"] = false;
    body["message"] = errorMessage;
    
    Message response = createResponse(type, sequence, body);
    client->sendMessage(response);
    
    qWarning() << "好友操作失败:" << errorMessage;
}

void FriendService::sendSuccessResponse(ClientHandler *client, MessageType type, 
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
