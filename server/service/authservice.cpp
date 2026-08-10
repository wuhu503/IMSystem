#include "authservice.h"
#include "clienthandler.h"
#include "message.h"
#include "dbmanager.h"
#include "usermanager.h"
#include "utils.h"
#include <QPointer>

AuthService& AuthService::instance()
{
    static AuthService instance;
    return instance;
}

AuthService::AuthService()
{
    qInfo() << "AuthService 创建";
}

AuthService::~AuthService()
{
    qInfo() << "AuthService 销毁";
}

void AuthService::handleRegister(ClientHandler *client, const Message &msg)
{
    qInfo() << "处理注册请求";
    
    QJsonObject body = msg.jsonBody();
    QString username = body["username"].toString();
    QString password = body["password"].toString();
    
    if (username.isEmpty() || password.isEmpty()) {
        sendErrorResponse(client, MessageType::RSP_REGISTER, 
                         msg.sequence(), "用户名和密码不能为空");
        return;
    }
    
    if (username.length() < 3 || username.length() > 20) {
        sendErrorResponse(client, MessageType::RSP_REGISTER, 
                         msg.sequence(), "用户名长度必须在3-20之间");
        return;
    }
    
    if (password.length() < 6) {
        sendErrorResponse(client, MessageType::RSP_REGISTER, 
                         msg.sequence(), "密码长度不能少于6位");
        return;
    }
    
    uint32_t sequence = msg.sequence();
    QPointer<ClientHandler> safeClient(client);
    QString salt = Utils::generateSalt();
    QString passwordHash = Utils::hashPassword(password, salt);

    // 异步检查用户名占用
    DbManager::instance().isUsernameExistsAsync(username,
        [this, safeClient, sequence, username, passwordHash, salt](bool exists) {
            if (!safeClient) return;

            if (exists) {
                sendErrorResponse(safeClient.data(), MessageType::RSP_REGISTER,
                                 sequence, "用户名已存在");
                return;
            }

            // 异步插入用户
            DbManager::instance().insertUserAsync(username, passwordHash, salt,
                [this, safeClient, sequence, username](bool inserted) {
                    if (!safeClient) return;

                    if (!inserted) {
                        sendErrorResponse(safeClient.data(), MessageType::RSP_REGISTER,
                                         sequence, "注册失败，请稍后重试");
                        return;
                    }

                    // 异步获取新用户ID
                    DbManager::instance().getUserIdAsync(username,
                        [this, safeClient, sequence, username](qint64 userId) {
                            if (!safeClient) return;

                            QJsonObject responseBody;
                            responseBody["success"] = true;
                            responseBody["user_id"] = userId;
                            responseBody["message"] = "注册成功";

                            Message response = createResponse(MessageType::RSP_REGISTER,
                                                              sequence, responseBody);
                            safeClient->sendMessage(response);
                            qInfo() << "用户注册成功:" << username << ", userId:" << userId;
                        }, safeClient.data());
                }, safeClient.data());
        }, safeClient.data());
}

void AuthService::handleLogin(ClientHandler *client, const Message &msg)
{
    qInfo() << "处理登录请求";
    
    QJsonObject body = msg.jsonBody();
    QString username = body["username"].toString();
    QString password = body["password"].toString();
    
    if (username.isEmpty() || password.isEmpty()) {
        sendErrorResponse(client, MessageType::RSP_LOGIN, 
                         msg.sequence(), "用户名和密码不能为空");
        return;
    }
    
    // 如果当前连接已经登录了，先下线旧用户（内存层面；DB 状态在登录成功后链式更新）
    qint64 oldUserId = client->userId();
    if (oldUserId != -1) {
        qInfo() << "当前连接已登录用户" << oldUserId << "，先下线";
        UserManager::instance().userOffline(oldUserId);
    }

    uint32_t sequence = msg.sequence();
    QPointer<ClientHandler> safeClient(client);

    // 异步查询用户ID
    DbManager::instance().getUserIdAsync(username,
        [this, safeClient, sequence, username, password, oldUserId](qint64 userId) {
            if (!safeClient) return;

            if (userId == -1) {
                sendErrorResponse(safeClient.data(), MessageType::RSP_LOGIN,
                                 sequence, "用户名或密码错误");
                return;
            }

            // 异步获取用户信息（含盐与哈希，用于校验密码）
            DbManager::instance().getUserInfoAsync(userId,
                [this, safeClient, sequence, username, password, userId, oldUserId](QVariantMap userInfo) {
                    if (!safeClient) return;

                    QString salt = userInfo["salt"].toString();
                    QString storedHash = userInfo["password_hash"].toString();

                    QString inputHash = Utils::hashPassword(password, salt);
                    if (inputHash != storedHash) {
                        sendErrorResponse(safeClient.data(), MessageType::RSP_LOGIN,
                                         sequence, "用户名或密码错误");
                        return;
                    }

                    ClientHandler *client = safeClient.data();

                    // 检查该用户是否已在其他连接登录
                    if (UserManager::instance().isOnline(userId)) {
                        ClientHandler *oldHandler = UserManager::instance().getHandler(userId).data();
                        if (oldHandler && oldHandler != client) {
                            qInfo() << "用户" << userId << "在其他地方登录，踢掉旧连接";

                            QJsonObject kickBody;
                            kickBody["success"] = false;
                            kickBody["message"] = "您的账号在其他地方登录";
                            Message kickMsg(MessageType::RSP_LOGIN);
                            kickMsg.setJsonBody(kickBody);
                            oldHandler->sendMessage(kickMsg);

                            // 清除旧连接身份，断开前的窗口期请求将无法通过 token 校验
                            oldHandler->setToken(QString());
                            oldHandler->setUserId(-1);

                            // 先从UserManager移除，再断开连接
                            UserManager::instance().userOffline(userId);
                            oldHandler->socket()->disconnectFromHost();
                        }
                    }

                    // 生成Token
                    QString token = Utils::generateUUID();

                    // 设置用户信息
                    client->setUserId(userId);
                    client->setToken(token);

                    // 注册到在线用户管理
                    UserManager::instance().userOnline(userId, client);

                    // 更新在线状态：先置旧账号离线，再置新账号在线（链式执行，避免并发写同一行的竞态）
                    auto markNewUserOnline = [this, client, userId](bool) {
                        DbManager::instance().updateUserStatusAsync(userId, 1, nullptr, client);
                    };
                    if (oldUserId != -1 && oldUserId != userId) {
                        DbManager::instance().updateUserStatusAsync(oldUserId, 0, markNewUserOnline, client);
                    } else {
                        DbManager::instance().updateUserStatusAsync(userId, 1, nullptr, client);
                    }

                    QJsonObject responseBody;
                    responseBody["success"] = true;
                    responseBody["token"] = token;
                    responseBody["user_id"] = userId;
                    responseBody["message"] = "登录成功";

                    Message response = createResponse(MessageType::RSP_LOGIN,
                                                      sequence, responseBody);
                    client->sendMessage(response);

                    qInfo() << "用户登录成功:" << username << ", userId:" << userId;
                }, safeClient.data());
        }, safeClient.data());
}

Message AuthService::createResponse(MessageType type, uint32_t sequence, 
                                    const QJsonObject &body)
{
    Message msg(type);
    msg.setSequence(sequence);
    msg.setJsonBody(body);
    return msg;
}

void AuthService::sendErrorResponse(ClientHandler *client, MessageType type, 
                                    uint32_t sequence, const QString &errorMessage)
{
    QJsonObject body;
    body["success"] = false;
    body["message"] = errorMessage;
    
    Message response = createResponse(type, sequence, body);
    client->sendMessage(response);
    
    qWarning() << "认证失败:" << errorMessage;
}

