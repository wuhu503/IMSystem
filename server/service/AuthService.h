#ifndef AUTHSERVICE_H
#define AUTHSERVICE_H

#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>
#include <QHash>
#include <QString>

class ClientHandler;
class Message;
enum class MessageType : uint16_t;

class AuthService : public QObject
{
    Q_OBJECT

public:
    static AuthService& instance();
    // 禁止拷贝和赋值
    AuthService(const AuthService&) = delete;
    AuthService& operator=(const AuthService&) = delete;
    void handleRegister(ClientHandler *client, const Message &msg);
    void handleLogin(ClientHandler *client, const Message &msg);

private:
    AuthService();
    ~AuthService();

    // 登录失败限速（仅在主线程访问，无需加锁）
    struct LoginAttempt {
        int    failures = 0;
        qint64 lockedUntilSecs = 0;
    };

    bool validateCredentials(const QString &username, const QString &password,
                             QString *errorMessage) const;
    bool isLoginLocked(const QString &username, QString *errorMessage) const;
    void recordLoginFailure(const QString &username);
    void clearLoginFailures(const QString &username);

    void sendErrorResponse(ClientHandler *client, MessageType type, 
                          uint32_t sequence, const QString &errorMessage);

    QHash<QString, LoginAttempt> m_loginAttempts;
};

#endif // AUTHSERVICE_H
