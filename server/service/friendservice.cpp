#include "friendservice.h"
#include "clienthandler.h"
#include "message.h"
#include "dbmanager.h"
#include <QPointer>

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
    
    uint32_t sequence = msg.sequence();
    QPointer<ClientHandler> safeClient(client);
    
    // 第一步：查找好友ID
    DbManager::instance().getUserIdAsync(friendUsername,
        [this, safeClient, userId, friendUsername, sequence](qint64 friendId) {
            if (!safeClient) return;
            
            if (friendId == -1) {
                sendErrorResponse(safeClient.data(), MessageType::RSP_ADD_FRIEND, 
                                 sequence, "用户不存在");
                return;
            }
            
            if (userId == friendId) {
                sendErrorResponse(safeClient.data(), MessageType::RSP_ADD_FRIEND, 
                                 sequence, "不能添加自己为好友");
                return;
            }
            
            // 第二步：检查好友关系
            checkFriendshipAndAdd(userId, friendId, friendUsername, sequence, safeClient);
        }, safeClient.data());
}

void FriendService::checkFriendshipAndAdd(qint64 userId, qint64 friendId,
                                           const QString &friendUsername, uint32_t sequence,
                                           QPointer<ClientHandler> safeClient)
{
    DbManager::instance().isFriendAsync(userId, friendId,
        [this, safeClient, userId, friendId, friendUsername, sequence](bool isFriend) {
            if (!safeClient) return;
            
            if (isFriend) {
                sendErrorResponse(safeClient.data(), MessageType::RSP_ADD_FRIEND, 
                                 sequence, "已经是好友关系");
                return;
            }
            
            // 第三步：检查待处理请求
            checkPendingAndAdd(userId, friendId, friendUsername, sequence, safeClient);
        }, safeClient.data());
}

void FriendService::checkPendingAndAdd(qint64 userId, qint64 friendId,
                                        const QString &friendUsername, uint32_t sequence,
                                        QPointer<ClientHandler> safeClient)
{
    DbManager::instance().hasPendingFriendRequestAsync(userId, friendId,
        [this, safeClient, userId, friendId, friendUsername, sequence](bool hasPending) {
            if (!safeClient) return;
            
            if (hasPending) {
                sendErrorResponse(safeClient.data(), MessageType::RSP_ADD_FRIEND, 
                                 sequence, "已发送过好友请求，请等待对方处理");
                return;
            }
            
            // 第四步：添加好友请求
            DbManager::instance().addFriendRequestAsync(userId, friendId,
                [this, safeClient, friendUsername, sequence](bool success) {
                    if (!safeClient) return;
                    
                    if (success) {
                        QJsonObject data;
                        data["friend_username"] = friendUsername;
                        data["message"] = "好友请求已发送";
                        sendSuccessResponse(safeClient.data(), MessageType::RSP_ADD_FRIEND, sequence, data);
                        qInfo() << "好友请求已发送";
                    } else {
                        sendErrorResponse(safeClient.data(), MessageType::RSP_ADD_FRIEND, 
                                         sequence, "添加好友失败，请稍后重试");
                    }
                }, safeClient.data());
        }, safeClient.data());
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
    
    uint32_t sequence = msg.sequence();
    QPointer<ClientHandler> safeClient(client);
    
    // 异步获取好友列表
    DbManager::instance().getFriendListAsync(userId,
        [this, safeClient, sequence](QJsonArray friends) {
            if (!safeClient) return;
            
            QJsonObject data;
            data["friends"] = friends;
            data["count"] = friends.size();
            
            sendSuccessResponse(safeClient.data(), MessageType::RSP_FRIEND_LIST, sequence, data);
        }, safeClient.data());
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
    
    uint32_t sequence = msg.sequence();
    QPointer<ClientHandler> safeClient(client);
    
    // 异步查找好友ID
    DbManager::instance().getUserIdAsync(friendUsername,
        [this, safeClient, userId, friendUsername, sequence](qint64 friendId) {
            if (!safeClient) return;
            
            if (friendId == -1) {
                sendErrorResponse(safeClient.data(), MessageType::RSP_ACCEPT_FRIEND, 
                                 sequence, "用户不存在");
                return;
            }
            
            // 异步接受好友请求
            DbManager::instance().acceptFriendRequestAsync(friendId, userId,
                [this, safeClient, friendUsername, sequence](bool success) {
                    if (!safeClient) return;
                    
                    if (success) {
                        QJsonObject data;
                        data["friend_username"] = friendUsername;
                        data["message"] = "已接受好友请求";
                        sendSuccessResponse(safeClient.data(), MessageType::RSP_ACCEPT_FRIEND, sequence, data);
                    } else {
                        sendErrorResponse(safeClient.data(), MessageType::RSP_ACCEPT_FRIEND, 
                                         sequence, "接受好友请求失败");
                    }
                }, safeClient.data());
        }, safeClient.data());
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
    
    uint32_t sequence = msg.sequence();
    QPointer<ClientHandler> safeClient(client);
    
    // 异步查找好友ID
    DbManager::instance().getUserIdAsync(friendUsername,
        [this, safeClient, userId, friendUsername, sequence](qint64 friendId) {
            if (!safeClient) return;
            
            if (friendId == -1) {
                sendErrorResponse(safeClient.data(), MessageType::RSP_REJECT_FRIEND, 
                                 sequence, "用户不存在");
                return;
            }
            
            // 异步拒绝好友请求
            DbManager::instance().rejectFriendRequestAsync(friendId, userId,
                [this, safeClient, friendUsername, sequence](bool success) {
                    if (!safeClient) return;
                    
                    if (success) {
                        QJsonObject data;
                        data["friend_username"] = friendUsername;
                        data["message"] = "已拒绝好友请求";
                        sendSuccessResponse(safeClient.data(), MessageType::RSP_REJECT_FRIEND, sequence, data);
                    } else {
                        sendErrorResponse(safeClient.data(), MessageType::RSP_REJECT_FRIEND, 
                                         sequence, "拒绝好友请求失败");
                    }
                }, safeClient.data());
        }, safeClient.data());
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
    
    uint32_t sequence = msg.sequence();
    QPointer<ClientHandler> safeClient(client);
    
    // 异步查找好友ID
    DbManager::instance().getUserIdAsync(friendUsername,
        [this, safeClient, userId, friendUsername, sequence](qint64 friendId) {
            if (!safeClient) return;
            
            if (friendId == -1) {
                sendErrorResponse(safeClient.data(), MessageType::RSP_DELETE_FRIEND, 
                                 sequence, "用户不存在");
                return;
            }
            
            // 异步删除好友
            DbManager::instance().deleteFriendAsync(userId, friendId,
                [this, safeClient, friendUsername, sequence](bool success) {
                    if (!safeClient) return;
                    
                    if (success) {
                        QJsonObject data;
                        data["friend_username"] = friendUsername;
                        data["message"] = "已删除好友";
                        sendSuccessResponse(safeClient.data(), MessageType::RSP_DELETE_FRIEND, sequence, data);
                    } else {
                        sendErrorResponse(safeClient.data(), MessageType::RSP_DELETE_FRIEND, 
                                         sequence, "删除好友失败");
                    }
                }, safeClient.data());
        }, safeClient.data());
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
    
    uint32_t sequence = msg.sequence();
    QPointer<ClientHandler> safeClient(client);
    
    // 异步搜索用户
    DbManager::instance().searchUsersAsync(keyword, userId,
        [this, safeClient, sequence](QJsonArray users) {
            if (!safeClient) return;
            
            QJsonObject data;
            data["users"] = users;
            data["count"] = users.size();
            
            sendSuccessResponse(safeClient.data(), MessageType::RSP_SEARCH_USER, sequence, data);
        }, safeClient.data());
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
    
    uint32_t sequence = msg.sequence();
    QPointer<ClientHandler> safeClient(client);
    
    // 异步获取待处理好友请求
    DbManager::instance().getPendingFriendRequestsAsync(userId,
        [this, safeClient, sequence](QJsonArray requests) {
            if (!safeClient) return;
            
            QJsonObject data;
            data["requests"] = requests;
            data["count"] = requests.size();
            
            sendSuccessResponse(safeClient.data(), MessageType::RSP_PENDING_REQUESTS, sequence, data);
        }, safeClient.data());
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

