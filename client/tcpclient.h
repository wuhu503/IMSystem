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

    QString host() const;
    quint16 port() const;

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
};

#endif // TCPCLIENT_H