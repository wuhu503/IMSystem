#include "authservice.h"
#include "clienthandler.h"
#include "message.h"
#include "dbmanager.h"
#include "usermanager.h"
#include "utils.h"

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
    
    if (DbManager::instance().isUsernameExists(username)) {
        sendErrorResponse(client, MessageType::RSP_REGISTER, 
                         msg.sequence(), "用户名已存在");
        return;
    }
    
    QString salt = Utils::generateSalt();
    QString passwordHash = Utils::hashPassword(password, salt);
    
    if (!DbManager::instance().insertUser(username, passwordHash, salt)) {
        sendErrorResponse(client, MessageType::RSP_REGISTER, 
                         msg.sequence(), "注册失败，请稍后重试");
        return;
    }
    
    qint64 userId = DbManager::instance().getUserId(username);
    
    QJsonObject responseBody;
    responseBody["success"] = true;
    responseBody["user_id"] = userId;
    responseBody["message"] = "注册成功";
    
    Message response = createResponse(MessageType::RSP_REGISTER, 
                                      msg.sequence(), responseBody);
    client->sendMessage(response);
    
    qInfo() << "用户注册成功:" << username << ", userId:" << userId;
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
    
    // 如果当前连接已经登录了，先下线旧用户
    if (client->userId() != -1) {
        qInfo() << "当前连接已登录用户" << client->userId() << "，先下线";
        UserManager::instance().userOffline(client->userId());
        DbManager::instance().updateUserStatusAsync(client->userId(), 0, nullptr, client);
    }
    
    qint64 userId = DbManager::instance().getUserId(username);
    if (userId == -1) {
        sendErrorResponse(client, MessageType::RSP_LOGIN, 
                         msg.sequence(), "用户名或密码错误");
        return;
    }
    
    QVariantMap userInfo = DbManager::instance().getUserInfo(userId);
    QString salt = userInfo["salt"].toString();
    QString storedHash = userInfo["password_hash"].toString();
    
    QString inputHash = Utils::hashPassword(password, salt);
    if (inputHash != storedHash) {
        sendErrorResponse(client, MessageType::RSP_LOGIN, 
                         msg.sequence(), "用户名或密码错误");
        return;
    }
    
    // 检查该用户是否已在其他连接登录
    if (UserManager::instance().isOnline(userId)) {
        ClientHandler *oldHandler = UserManager::instance().getHandler(userId);
        if (oldHandler && oldHandler != client) {
            qInfo() << "用户" << userId << "在其他地方登录，踢掉旧连接";
            
            QJsonObject kickBody;
            kickBody["success"] = false;
            kickBody["message"] = "您的账号在其他地方登录";
            Message kickMsg(MessageType::RSP_LOGIN);
            kickMsg.setJsonBody(kickBody);
            oldHandler->sendMessage(kickMsg);
            
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
    
    // 更新在线状态
    DbManager::instance().updateUserStatusAsync(userId, 1, nullptr, client);
    
    QJsonObject responseBody;
    responseBody["success"] = true;
    responseBody["token"] = token;
    responseBody["user_id"] = userId;
    responseBody["message"] = "登录成功";
    
    Message response = createResponse(MessageType::RSP_LOGIN, 
                                      msg.sequence(), responseBody);
    client->sendMessage(response);
    
    qInfo() << "用户登录成功:" << username << ", userId:" << userId;
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

