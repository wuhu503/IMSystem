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

// 处理添加好友请求
void FriendService::handleAddFriend(ClientHandler *client, const Message &msg)
{
    qInfo() << "处理添加好友请求";
    
    QJsonObject body = msg.jsonBody();
    QString friendUsername = body["username"].toString();
    
    // 验证参数
    if (friendUsername.isEmpty()) {
        sendErrorResponse(client, MessageType::RSP_ADD_FRIEND, 
                         msg.sequence(), "好友用户名不能为空");
        return;
    }
    
    // 获取当前用户ID
    qint64 userId = client->userId();
    if (userId == -1) {
        sendErrorResponse(client, MessageType::RSP_ADD_FRIEND, 
                         msg.sequence(), "请先登录");
        return;
    }
    
    // 获取好友用户ID
    qint64 friendId = DbManager::instance().getUserId(friendUsername);
    if (friendId == -1) {
        sendErrorResponse(client, MessageType::RSP_ADD_FRIEND, 
                         msg.sequence(), "用户不存在");
        return;
    }
    
    // 不能添加自己为好友
    if (userId == friendId) {
        sendErrorResponse(client, MessageType::RSP_ADD_FRIEND, 
                         msg.sequence(), "不能添加自己为好友");
        return;
    }
    
    // 检查是否已经是好友
    if (DbManager::instance().isFriend(userId, friendId)) {
        sendErrorResponse(client, MessageType::RSP_ADD_FRIEND, 
                         msg.sequence(), "已经是好友关系");
        return;
    }
    
    // 检查是否已发送过请求
    if (DbManager::instance().hasPendingFriendRequest(userId, friendId)) {
        sendErrorResponse(client, MessageType::RSP_ADD_FRIEND, 
                         msg.sequence(), "已发送过好友请求，请等待对方处理");
        return;
    }
    
    // 添加好友申请
    if (!DbManager::instance().addFriendRequest(userId, friendId)) {
        sendErrorResponse(client, MessageType::RSP_ADD_FRIEND, 
                         msg.sequence(), "添加好友失败，请稍后重试");
        return;
    }
    
    // 返回成功
    QJsonObject data;
    data["friend_username"] = friendUsername;
    data["message"] = "好友请求已发送";
    sendSuccessResponse(client, MessageType::RSP_ADD_FRIEND, msg.sequence(), data);
    
    qInfo() << "用户" << userId << "向" << friendId << "发送好友请求";
}

// 处理获取好友列表
void FriendService::handleFriendList(ClientHandler *client, const Message &msg)
{
    qInfo() << "处理获取好友列表";
    
    qint64 userId = client->userId();
    if (userId == -1) {
        sendErrorResponse(client, MessageType::RSP_FRIEND_LIST, 
                         msg.sequence(), "请先登录");
        return;
    }
    
    // 获取好友列表
    QJsonArray friends = DbManager::instance().getFriendList(userId);
    
    QJsonObject data;
    data["friends"] = friends;
    data["count"] = friends.size();
    
    sendSuccessResponse(client, MessageType::RSP_FRIEND_LIST, msg.sequence(), data);
    
    qInfo() << "用户" << userId << "获取好友列表，共" << friends.size() << "个好友";
}

// 处理接受好友请求
void FriendService::handleAcceptFriend(ClientHandler *client, const Message &msg)
{
    qInfo() << "处理接受好友请求";
    
    QJsonObject body = msg.jsonBody();
    QString friendUsername = body["username"].toString();
    
    qint64 userId = client->userId();  // 当前用户（接受请求的人）
    if (userId == -1) {
        sendErrorResponse(client, MessageType::RSP_ACCEPT_FRIEND, 
                         msg.sequence(), "请先登录");
        return;
    }
    
    qint64 friendId = DbManager::instance().getUserId(friendUsername);  // 发送请求的人
    if (friendId == -1) {
        sendErrorResponse(client, MessageType::RSP_ACCEPT_FRIEND, 
                         msg.sequence(), "用户不存在");
        return;
    }
    
    // 接受好友请求：friendId是发送请求的人，userId是接受请求的人
    if (!DbManager::instance().acceptFriendRequest(friendId, userId)) {
        sendErrorResponse(client, MessageType::RSP_ACCEPT_FRIEND, 
                         msg.sequence(), "接受好友请求失败");
        return;
    }
    
    // 返回成功
    QJsonObject data;
    data["friend_username"] = friendUsername;
    data["message"] = "已接受好友请求";
    sendSuccessResponse(client, MessageType::RSP_ACCEPT_FRIEND, msg.sequence(), data);
    
    qInfo() << "用户" << userId << "接受了" << friendId << "的好友请求";
}

// 处理拒绝好友请求
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
    
    // 拒绝好友请求
    if (!DbManager::instance().rejectFriendRequest(friendId, userId)) {
        sendErrorResponse(client, MessageType::RSP_REJECT_FRIEND, 
                         msg.sequence(), "拒绝好友请求失败");
        return;
    }
    
    // 返回成功
    QJsonObject data;
    data["friend_username"] = friendUsername;
    data["message"] = "已拒绝好友请求";
    sendSuccessResponse(client, MessageType::RSP_REJECT_FRIEND, msg.sequence(), data);
    
    qInfo() << "用户" << userId << "拒绝了" << friendId << "的好友请求";
}

// 处理删除好友
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
    
    // 删除好友
    if (!DbManager::instance().deleteFriend(userId, friendId)) {
        sendErrorResponse(client, MessageType::RSP_DELETE_FRIEND, 
                         msg.sequence(), "删除好友失败");
        return;
    }
    
    // 返回成功
    QJsonObject data;
    data["friend_username"] = friendUsername;
    data["message"] = "已删除好友";
    sendSuccessResponse(client, MessageType::RSP_DELETE_FRIEND, msg.sequence(), data);
    
    qInfo() << "用户" << userId << "删除了好友" << friendId;
}

// 处理搜索用户
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
    
    // 搜索用户
    QJsonArray users = DbManager::instance().searchUsers(keyword, userId);
    
    QJsonObject data;
    data["users"] = users;
    data["count"] = users.size();
    
    sendSuccessResponse(client, MessageType::RSP_SEARCH_USER, msg.sequence(), data);
    
    qInfo() << "用户" << userId << "搜索用户:" << keyword << "，找到" << users.size() << "个结果";
}

// 处理获取待处理的好友请求
void FriendService::handlePendingRequests(ClientHandler *client, const Message &msg)
{
    qInfo() << "处理获取待处理的好友请求";
    
    qint64 userId = client->userId();
    if (userId == -1) {
        sendErrorResponse(client, MessageType::RSP_FRIEND_LIST, 
                         msg.sequence(), "请先登录");
        return;
    }
    
    // 获取待处理的好友请求
    QJsonArray requests = DbManager::instance().getPendingFriendRequests(userId);
    
    QJsonObject data;
    data["requests"] = requests;
    data["count"] = requests.size();
    
    sendSuccessResponse(client, MessageType::RSP_FRIEND_LIST, msg.sequence(), data);
    
    qInfo() << "用户" << userId << "获取待处理好友请求，共" << requests.size() << "个";
}

// 广播好友状态变化
void FriendService::broadcastFriendStatus(qint64 userId, int status)
{
    // TODO: 实现广播功能，需要维护在线用户列表
    qInfo() << "广播用户" << userId << "状态变化:" << status;
}

// 创建响应消息
Message FriendService::createResponse(MessageType type, uint32_t sequence, 
                                       const QJsonObject &body)
{
    Message msg(type);
    msg.setSequence(sequence);
    msg.setJsonBody(body);
    return msg;
}

// 发送错误响应
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

// 发送成功响应
void FriendService::sendSuccessResponse(ClientHandler *client, MessageType type, 
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
