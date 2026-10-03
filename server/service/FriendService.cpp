#include "FriendService.h"
#include "ClientHandler.h"
#include "Message.h"
#include "FriendRepository.h"
#include "UserRepository.h"
#include "UserManager.h"
#include "Reply.h"
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
    QString friendUsername = body["username"].toString().trimmed();
    
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
    UserRepository::instance().getUserIdAsync(friendUsername,
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
            
            // 第二步：查重 + 写入在同一个数据库事务内完成（避免双向重复请求）
            FriendRepository::instance().addFriendRequestAsync(userId, friendId,
                [this, safeClient, userId, friendId, friendUsername, sequence](AddFriendResult result) {
                    if (!safeClient) return;

                    switch (result) {
                    case AddFriendResult::AlreadyFriend:
                        sendErrorResponse(safeClient.data(), MessageType::RSP_ADD_FRIEND,
                                          sequence, "已经是好友关系");
                        return;
                    case AddFriendResult::AlreadyRequested:
                        sendErrorResponse(safeClient.data(), MessageType::RSP_ADD_FRIEND,
                                          sequence, "已发送过好友请求，请等待对方处理");
                        return;
                    case AddFriendResult::ReverseRequested:
                        sendErrorResponse(safeClient.data(), MessageType::RSP_ADD_FRIEND,
                                          sequence, "对方已向您发送好友请求，请在好友请求列表中处理");
                        return;
                    case AddFriendResult::Failed:
                        sendErrorResponse(safeClient.data(), MessageType::RSP_ADD_FRIEND,
                                          sequence, "添加好友失败，请稍后重试");
                        return;
                    case AddFriendResult::Success:
                        break;
                    }

                    QJsonObject data;
                    data["friend_username"] = friendUsername;
                    data["message"] = "好友请求已发送";
                    sendSuccessResponse(safeClient.data(), MessageType::RSP_ADD_FRIEND, sequence, data);
                    qInfo() << "好友请求已发送";

                    // 通知目标用户：收到新的好友请求（离线请求已入库，上线后可在请求列表看到）
                    QPointer<ClientHandler> targetHandler = UserManager::instance().getHandler(friendId);
                    if (!targetHandler.isNull()) {
                        QJsonObject ntfBody;
                        ntfBody["message"] = "收到新的好友请求";
                        ntfBody["user_id"] = userId;
                        ntfBody["username"] = safeClient->username();
                        Message ntf(MessageType::NTF_FRIEND_REQUEST);
                        ntf.setJsonBody(ntfBody);
                        targetHandler->sendMessage(ntf);
                    }
                }, safeClient);
        }, safeClient);
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
    FriendRepository::instance().getFriendListAsync(userId,
        [this, safeClient, sequence](QJsonArray friends) {
            if (!safeClient) return;
            
            // 在线状态以内存中的 UserManager 为准：数据库里的 status 只是快照，
            // 客户端被强杀时会残留，直接返回会给出错误的“在线”
            for (int i = 0; i < friends.size(); ++i) {
                QJsonObject friendObj = friends.at(i).toObject();
                const qint64 friendId = friendObj["user_id"].toVariant().toLongLong();
                friendObj["status"] = UserManager::instance().isOnline(friendId) ? 1 : 0;
                friends.replace(i, friendObj);
            }

            QJsonObject data;
            data["friends"] = friends;
            data["count"] = friends.size();
            
            sendSuccessResponse(safeClient.data(), MessageType::RSP_FRIEND_LIST, sequence, data);
        }, safeClient);
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
    UserRepository::instance().getUserIdAsync(friendUsername,
        [this, safeClient, userId, friendUsername, sequence](qint64 friendId) {
            if (!safeClient) return;
            
            if (friendId == -1) {
                sendErrorResponse(safeClient.data(), MessageType::RSP_ACCEPT_FRIEND, 
                                 sequence, "用户不存在");
                return;
            }
            
            // 异步接受好友请求
            FriendRepository::instance().acceptFriendRequestAsync(friendId, userId,
                [this, safeClient, friendId, friendUsername, sequence](bool success) {
                    if (!safeClient) return;
                    
                    if (success) {
                        QJsonObject data;
                        data["friend_username"] = friendUsername;
                        data["message"] = "已接受好友请求";
                        sendSuccessResponse(safeClient.data(), MessageType::RSP_ACCEPT_FRIEND, sequence, data);

                        // 通知请求方：好友请求已被接受，客户端自动刷新好友列表
                        QPointer<ClientHandler> requesterHandler = UserManager::instance().getHandler(friendId);
                        if (!requesterHandler.isNull()) {
                            QJsonObject ntfBody;
                            ntfBody["message"] = "对方已接受您的好友请求";
                            Message ntf(MessageType::NTF_FRIEND_ACCEPTED);
                            ntf.setJsonBody(ntfBody);
                            requesterHandler->sendMessage(ntf);
                        }
                    } else {
                        sendErrorResponse(safeClient.data(), MessageType::RSP_ACCEPT_FRIEND, 
                                         sequence, "接受好友请求失败");
                    }
                }, safeClient);
        }, safeClient);
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
    UserRepository::instance().getUserIdAsync(friendUsername,
        [this, safeClient, userId, friendUsername, sequence](qint64 friendId) {
            if (!safeClient) return;
            
            if (friendId == -1) {
                sendErrorResponse(safeClient.data(), MessageType::RSP_REJECT_FRIEND, 
                                 sequence, "用户不存在");
                return;
            }
            
            // 异步拒绝好友请求
            FriendRepository::instance().rejectFriendRequestAsync(friendId, userId,
                [this, safeClient, friendId, friendUsername, sequence](bool success) {
                    if (!safeClient) return;
                    
                    if (success) {
                        QJsonObject data;
                        data["friend_username"] = friendUsername;
                        data["message"] = "已拒绝好友请求";
                        sendSuccessResponse(safeClient.data(), MessageType::RSP_REJECT_FRIEND, sequence, data);

                        // 通知请求方：请求被拒绝，避免对方一直等在那里
                        QPointer<ClientHandler> requesterHandler =
                            UserManager::instance().getHandler(friendId);
                        if (!requesterHandler.isNull()) {
                            QJsonObject ntfBody;
                            ntfBody["message"] = "对方拒绝了您的好友请求";
                            ntfBody["username"] = safeClient->username();
                            Message ntf(MessageType::NTF_FRIEND_REJECTED);
                            ntf.setJsonBody(ntfBody);
                            requesterHandler->sendMessage(ntf);
                        }
                    } else {
                        sendErrorResponse(safeClient.data(), MessageType::RSP_REJECT_FRIEND, 
                                         sequence, "拒绝好友请求失败");
                    }
                }, safeClient);
        }, safeClient);
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
    UserRepository::instance().getUserIdAsync(friendUsername,
        [this, safeClient, userId, friendUsername, sequence](qint64 friendId) {
            if (!safeClient) return;
            
            if (friendId == -1) {
                sendErrorResponse(safeClient.data(), MessageType::RSP_DELETE_FRIEND, 
                                 sequence, "用户不存在");
                return;
            }
            
            // 异步删除好友
            FriendRepository::instance().deleteFriendAsync(userId, friendId,
                [this, safeClient, friendId, friendUsername, sequence](bool success) {
                    if (!safeClient) return;
                    
                    if (success) {
                        QJsonObject data;
                        data["friend_username"] = friendUsername;
                        data["message"] = "已删除好友";
                        sendSuccessResponse(safeClient.data(), MessageType::RSP_DELETE_FRIEND, sequence, data);

                        // 通知对方：好友关系已解除，对方列表不必等到下次刷新才发现
                        QPointer<ClientHandler> removedHandler =
                            UserManager::instance().getHandler(friendId);
                        if (!removedHandler.isNull()) {
                            QJsonObject ntfBody;
                            ntfBody["message"] = "对方解除了与您的好友关系";
                            ntfBody["username"] = safeClient->username();
                            Message ntf(MessageType::NTF_FRIEND_REMOVED);
                            ntf.setJsonBody(ntfBody);
                            removedHandler->sendMessage(ntf);
                        }
                    } else {
                        sendErrorResponse(safeClient.data(), MessageType::RSP_DELETE_FRIEND, 
                                         sequence, "删除好友失败");
                    }
                }, safeClient);
        }, safeClient);
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
    FriendRepository::instance().searchUsersAsync(keyword, userId,
        [this, safeClient, sequence](QJsonArray users) {
            if (!safeClient) return;
            
            QJsonObject data;
            data["users"] = users;
            data["count"] = users.size();
            
            sendSuccessResponse(safeClient.data(), MessageType::RSP_SEARCH_USER, sequence, data);
        }, safeClient);
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
    FriendRepository::instance().getPendingFriendRequestsAsync(userId,
        [this, safeClient, sequence](QJsonArray requests) {
            if (!safeClient) return;
            
            QJsonObject data;
            data["requests"] = requests;
            data["count"] = requests.size();
            
            sendSuccessResponse(safeClient.data(), MessageType::RSP_PENDING_REQUESTS, sequence, data);
        }, safeClient);
}

void FriendService::sendErrorResponse(ClientHandler *client, MessageType type, 
                                       uint32_t sequence, const QString &errorMessage)
{
    Reply::sendFailure(client, type, sequence, errorMessage, "好友操作失败:");
}

void FriendService::sendSuccessResponse(ClientHandler *client, MessageType type, 
                                         uint32_t sequence, const QJsonObject &data)
{
    Reply::sendSuccess(client, type, sequence, data);
}

