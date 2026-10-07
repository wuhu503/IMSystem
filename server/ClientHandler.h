#ifndef CLIENTHANDLER_H
#define CLIENTHANDLER_H

#include <QObject>
#include <QTcpSocket>
#include "Message.h"

class ClientHandler : public QObject
{
    Q_OBJECT

public:
    explicit ClientHandler(QTcpSocket *socket, QObject *parent = nullptr);
    ~ClientHandler();

    void sendMessage(const Message &msg);
    
    qint64 userId() const;
    void setUserId(qint64 id);

    // 登录用户名
    QString username() const;
    void setUsername(const QString &username);

    QString token() const;
    void setToken(const QString &token);

    QTcpSocket* socket() const;

    // 连接建立时的socket描述符
    qintptr descriptor() const;

    // 清理登录态
    void resetLoginState();

    // 是否已经超过空闲阈值
    bool isIdle(qint64 nowMs) const;

signals:
    void clientDisconnect(qintptr socketDescriptor);

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    void touch();

    void handleMessage(const Message &msg);
    bool verifyToken(const Message &msg);
    void sendAuthErrorResponse(MessageType type, uint32_t sequence, const QString &reason);

    QTcpSocket *m_socket;
    qint64 m_userId;
    QString m_username;
    QString m_token;
    QByteArray m_buffer;  //缓冲区
    qintptr m_descriptor; //连接建立时的socket描述符
    qint64 m_lastActiveMs = 0;
};

#endif // CLIENTHANDLER_H
