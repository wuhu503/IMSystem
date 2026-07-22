#ifndef AUTHSERVICE_H
#define AUTHSERVICE_H

#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>

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

signals:
    void sendResponse(ClientHandler *client, const Message &msg);

private:
    AuthService();
    ~AuthService();
    Message createResponse(MessageType type, uint32_t sequence, 
                           const QJsonObject &body);
    
    void sendErrorResponse(ClientHandler *client, MessageType type, 
                          uint32_t sequence, const QString &errorMessage);
};

#endif // AUTHSERVICE_H
