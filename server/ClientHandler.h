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

    // 登录用户名：登录时缓存下来，避免每条消息都去数据库查一次
    QString username() const;
    void setUsername(const QString &username);

    QString token() const;
    void setToken(const QString &token);

    QTcpSocket* socket() const;

    // 连接建立时的 socket 描述符（断开后 socketDescriptor() 会变成 -1，详见 .cpp）
    qintptr descriptor() const;

    // 清理登录态（退出登录/被踢时调用）
    void resetLoginState();

    // 是否已经超过空闲阈值（nowMs 由调用方传入，便于批量扫描时共用同一时刻）
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
    qintptr m_descriptor; //连接建立时的 socket 描述符（断开后 socketDescriptor() 会变成 -1）
    qint64 m_lastActiveMs = 0;
};

#endif // CLIENTHANDLER_H
