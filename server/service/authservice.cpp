#include "authservice.h"
#include "clienthandler.h"
#include "message.h"
#include "dbmanager.h"
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
    
    // 1. 解析请求 JSON
    QJsonObject body = msg.jsonBody();
    QString username = body["username"].toString();
    QString password = body["password"].toString();
    
    // 2. 验证参数
    if (username.isEmpty() || password.isEmpty()) {
        sendErrorResponse(client, MessageType::RSP_REGISTER, 
                         msg.sequence(), "用户名和密码不能为空");
        return;
    }
    
    // 3. 检查用户名长度
    if (username.length() < 3 || username.length() > 20) {
        sendErrorResponse(client, MessageType::RSP_REGISTER, 
                         msg.sequence(), "用户名长度必须在3-20之间");
        return;
    }
    
    // 4. 检查密码长度
    if (password.length() < 6) {
        sendErrorResponse(client, MessageType::RSP_REGISTER, 
                         msg.sequence(), "密码长度不能少于6位");
        return;
    }
    
    // 5. 检查用户名是否已存在
    if (DbManager::instance().isUsernameExists(username)) {
        sendErrorResponse(client, MessageType::RSP_REGISTER, 
                         msg.sequence(), "用户名已存在");
        return;
    }
    
    // 6. 生成盐值和密码哈希
    QString salt = Utils::generateSalt();
    QString passwordHash = Utils::hashPassword(password, salt);
    
    // 7. 存入数据库
    if (!DbManager::instance().insertUser(username, passwordHash, salt)) {
        sendErrorResponse(client, MessageType::RSP_REGISTER, 
                         msg.sequence(), "注册失败，请稍后重试");
        return;
    }
    
    // 8. 获取用户ID
    qint64 userId = DbManager::instance().getUserId(username);
    
    // 9. 返回成功响应
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
    
    // 1. 解析请求 JSON
    QJsonObject body = msg.jsonBody();
    QString username = body["username"].toString();
    QString password = body["password"].toString();
    
    // 2. 验证参数
    if (username.isEmpty() || password.isEmpty()) {
        sendErrorResponse(client, MessageType::RSP_LOGIN, 
                         msg.sequence(), "用户名和密码不能为空");
        return;
    }
    
    // 3. 检查用户是否存在
    qint64 userId = DbManager::instance().getUserId(username);
    if (userId == -1) {
        sendErrorResponse(client, MessageType::RSP_LOGIN, 
                         msg.sequence(), "用户名或密码错误");
        return;
    }
    
    // 4. 获取用户信息（盐值、密码哈希）
    QVariantMap userInfo = DbManager::instance().getUserInfo(userId);
    QString salt = userInfo["salt"].toString();
    QString storedHash = userInfo["password_hash"].toString();
    
    // 5. 验证密码
    QString inputHash = Utils::hashPassword(password, salt);
    if (inputHash != storedHash) {
        sendErrorResponse(client, MessageType::RSP_LOGIN, 
                         msg.sequence(), "用户名或密码错误");
        return;
    }
    
    // 6. 生成 Token（使用 UUID）
    QString token = Utils::generateUUID();
    
    // 7. 设置用户ID（标记为已登录）
    client->setUserId(userId);
    
    // 8. 更新在线状态
    DbManager::instance().updateUserStatus(userId, 1);
    
    // 9. 返回成功响应
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
