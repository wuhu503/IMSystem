#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QTcpServer>
#include <QMap>
#include <QHostAddress>

class ClientHandler;

class TcpServer : public QTcpServer
{
    Q_OBJECT

public:
    explicit TcpServer(QObject *parent = nullptr);
    ~TcpServer();
    bool startServer(quint16 port);
    void stopServer();
    int clientCount() const;

signals:
    void newClientConnected(qintptr socketDescriptor);
    void clientDisconnected(qintptr socketDescriptor);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void onClientDisconnected(qintptr socketDescriptor);

private:
    void clearAllClients();
    // 存储所有活跃的 ClientHandler
    // key: socketDescriptor, value: ClientHandler 指针
    QMap<qintptr, ClientHandler*> m_clients;
};

#endif // TCPSERVER_H
