#include "AuthService.h"
#include "ClientHandler.h"
#include "Message.h"
#include "UserRepository.h"
#include "UserManager.h"
#include "Utils.h"
#include "ChatService.h"
#include "Reply.h"
#include "Constants.h"
#include <QDateTime>
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

    const QJsonObject body = msg.jsonBody();
    const QString username = body["username"].toString().trimmed();
    const QString password = body["password"].toString();

    QString error;
    if (!validateCredentials(username, password, &error)) {
        sendErrorResponse(client, MessageType::RSP_REGISTER, msg.sequence(), error);
        return;
    }

    const uint32_t sequence = msg.sequence();
    QPointer<ClientHandler> safeClient(client);

    // 查重、加盐、哈希、插入都在数据库线程内一次完成，避免“先查后插”的竞态
    UserRepository::instance().registerUserAsync(username, password,
        [this, safeClient, sequence, username](RegisterResult result) {
            if (!safeClient) return;

            if (!result.success) {
                sendErrorResponse(safeClient.data(), MessageType::RSP_REGISTER,
                                  sequence,
                                  result.errorMessage.isEmpty()
                                      ? QStringLiteral("注册失败，请稍后重试")
                                      : result.errorMessage);
                return;
            }

            QJsonObject responseBody;
            responseBody["success"] = true;
            responseBody["user_id"] = result.userId;
            responseBody["message"] = "注册成功";

            safeClient->sendMessage(Reply::make(MessageType::RSP_REGISTER,
                                                sequence, responseBody));
            qInfo() << "用户注册成功:" << username << ", userId:" << result.userId;
        }, safeClient);
}

void AuthService::handleLogin(ClientHandler *client, const Message &msg)
{
    qInfo() << "处理登录请求";

    const QJsonObject body = msg.jsonBody();
    const QString username = body["username"].toString().trimmed();
    const QString password = body["password"].toString();

    if (username.isEmpty() || password.isEmpty()) {
        sendErrorResponse(client, MessageType::RSP_LOGIN,
                         msg.sequence(), "用户名和密码不能为空");
        return;
    }

    QString lockMessage;
    if (isLoginLocked(username, &lockMessage)) {
        sendErrorResponse(client, MessageType::RSP_LOGIN, msg.sequence(), lockMessage);
        return;
    }

    // 同一连接重复登录：先把旧用户从在线表移除（DB 状态在登录成功后链式更新）
    const qint64 oldUserId = client->userId();
    if (oldUserId != -1) {
        qInfo() << "当前连接已登录用户" << oldUserId << "，先下线";
        UserManager::instance().userOffline(oldUserId);
    }

    const uint32_t sequence = msg.sequence();
    QPointer<ClientHandler> safeClient(client);

    // 查询与口令校验都在数据库线程内完成（PBKDF2 是慢哈希，不能放在主线程）
    UserRepository::instance().verifyUserAsync(username, password,
        [this, safeClient, sequence, username, password, oldUserId](LoginCheckResult check) {
            if (!safeClient) return;

            ClientHandler *client = safeClient.data();

            if (check.userId == -1 || !check.passwordOk) {
                recordLoginFailure(username);
                sendErrorResponse(client, MessageType::RSP_LOGIN,
                                  sequence, "用户名或密码错误");
                return;
            }

            clearLoginFailures(username);

            const qint64 userId = check.userId;

            // 旧哈希（单轮 SHA256）在登录成功后自动升级为 PBKDF2，对用户无感
            if (check.needRehash) {
                UserRepository::instance().upgradePasswordHashAsync(userId, password, nullptr, safeClient);
            }

            // 已在其它连接登录 → 踢掉旧连接
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

                    // 清登录态：断开前的窗口期请求无法再通过 token 校验
                    UserManager::instance().userOffline(userId);
                    oldHandler->resetLoginState();
                    oldHandler->socket()->disconnectFromHost();
                }
            }

            const QString token = Utils::generateUUID();

            client->setUserId(userId);
            client->setUsername(username);
            client->setToken(token);
            UserManager::instance().userOnline(userId, client);

            // 更新在线状态：先置旧账号离线，再置新账号在线（链式执行，避免并发写同一行的竞态）
            auto markNewUserOnline = [safeClient, userId](bool) {
                if (!safeClient) return;
                UserRepository::instance().updateUserStatusAsync(userId, 1, nullptr, safeClient);
            };
            if (oldUserId != -1 && oldUserId != userId) {
                UserRepository::instance().updateUserStatusAsync(oldUserId, 0, markNewUserOnline, safeClient);
            } else {
                UserRepository::instance().updateUserStatusAsync(userId, 1, nullptr, safeClient);
            }

            QJsonObject responseBody;
            responseBody["success"] = true;
            responseBody["token"] = token;
            responseBody["user_id"] = userId;
            responseBody["message"] = "登录成功";

            client->sendMessage(Reply::make(MessageType::RSP_LOGIN, sequence, responseBody));

            qInfo() << "用户登录成功:" << username << ", userId:" << userId;

            // 登录成功后再补推离线消息
            ChatService::instance().deliverOfflineMessages(client);
        }, safeClient);
}

bool AuthService::validateCredentials(const QString &username, const QString &password,
                                      QString *errorMessage) const
{
    if (username.isEmpty() || password.isEmpty()) {
        *errorMessage = QStringLiteral("用户名和密码不能为空");
        return false;
    }

    if (username.length() < IMConstants::kUsernameMinLength
        || username.length() > IMConstants::kUsernameMaxLength) {
        *errorMessage = QStringLiteral("用户名长度必须在 %1-%2 之间")
                            .arg(IMConstants::kUsernameMinLength)
                            .arg(IMConstants::kUsernameMaxLength);
        return false;
    }

    if (password.length() < IMConstants::kPasswordMinLength) {
        *errorMessage = QStringLiteral("密码长度不能少于 %1 位")
                            .arg(IMConstants::kPasswordMinLength);
        return false;
    }

    if (password.length() > IMConstants::kPasswordMaxLength) {
        *errorMessage = QStringLiteral("密码长度不能超过 %1 位")
                            .arg(IMConstants::kPasswordMaxLength);
        return false;
    }

    return true;
}

bool AuthService::isLoginLocked(const QString &username, QString *errorMessage) const
{
    const auto it = m_loginAttempts.constFind(username);
    if (it == m_loginAttempts.constEnd()) {
        return false;
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (it->lockedUntilSecs <= now) {
        return false;
    }

    *errorMessage = QStringLiteral("登录失败次数过多，请 %1 秒后再试")
                        .arg(it->lockedUntilSecs - now);
    return true;
}

void AuthService::recordLoginFailure(const QString &username)
{
    LoginAttempt &attempt = m_loginAttempts[username];
    ++attempt.failures;

    if (attempt.failures >= IMConstants::kMaxLoginFailures) {
        attempt.failures = 0;
        attempt.lockedUntilSecs = QDateTime::currentSecsSinceEpoch() + IMConstants::kLoginLockoutSecs;
        qWarning() << "账号因连续登录失败被临时锁定:" << username;
    }
}

void AuthService::clearLoginFailures(const QString &username)
{
    m_loginAttempts.remove(username);
}

void AuthService::sendErrorResponse(ClientHandler *client, MessageType type, 
                                    uint32_t sequence, const QString &errorMessage)
{
    Reply::sendFailure(client, type, sequence, errorMessage, "认证失败:");
}

