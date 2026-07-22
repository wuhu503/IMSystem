#ifndef CLIENTHANDLER_H
#define CLIENTHANDLER_H

#include <QObject>
#include <QTcpSocket>
#include "dbmanager.h"
#include "message.h"

class ClientHandler : public QObject
{
    Q_OBJECT

public:
    explicit ClientHandler(QTcpSocket *socket, QObject *parent = nullptr);
    ~ClientHandler();

    // 发送消息给客户端
    void sendMessage(const Message &msg);
    
    // 获取用户信息（登录后才有）
    qint64 userId() const;
    void setUserId(qint64 id);
    
    // 获取 socket
    QTcpSocket* socket() const;

signals:
    // 客户端断开信号
    void clientDisconnect(qintptr socketDescriptor);
    
    // 收到完整消息信号
    void messageReceived(qint64 userId, const Message &msg);

private slots:
    // 读取数据
    void onReadyRead();
    
    // 客户端断开
    void onDisconnected();

private:
    // 处理接收到的完整消息
    void handleMessage(const Message &msg);

    QTcpSocket *m_socket;      // 客户端 socket
    qint64 m_userId;           // 用户ID（登录后设置）
    QByteArray m_buffer;       // 数据缓冲区（处理粘包）
};

#endif // CLIENTHANDLER_H
