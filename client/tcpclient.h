#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QObject>
#include "message.h"
#include <QTcpSocket>
#include <cstring>

class TcpClient:public QObject
{
    Q_OBJECT
public:
    static TcpClient& instance();
    void connectToServer(const QString& host,quint16 port);

    void disconnectToServer();

    void sendMessage(const Message &msg);

    bool isConnect() const;
    bool isConnecting() const;  // 是否正在连接中

    QString host() const;
    quint16 port() const;
    
    // Token 相关
    void setToken(const QString &token);
    QString token() const;
    void clearToken();

signals:
    void messageReceived(const Message &msg);
    void connectionEstablished();
    void connectionClosed();
    void errorOccurred(const QString &error);

private slots:
    void onReadyRead();
    void onConnected();
    void onDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError error);

private:
    TcpClient(QObject *parent=nullptr);
    ~TcpClient();
    QTcpSocket *m_socket;
    QByteArray m_buffer;
    QString m_host;
    quint16 m_port;
    QString m_token;      // 登录成功后的token
    bool m_connecting;    // 是否正在连接中
};

#endif // TCPCLIENT_H
