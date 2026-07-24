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
};

#endif // TCPCLIENT_H
