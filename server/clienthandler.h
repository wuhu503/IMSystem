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

    void sendMessage(const Message &msg);
    
    qint64 userId() const;
    void setUserId(qint64 id);
    
    QString token() const;
    void setToken(const QString &token);
    
    QTcpSocket* socket() const;

signals:
    void clientDisconnect(qintptr socketDescriptor);
    void messageReceived(qint64 userId, const Message &msg);

private slots:
    void onReadyRead();
    void onDisconnected();

private:
    void handleMessage(const Message &msg);

    QTcpSocket *m_socket;
    qint64 m_userId;
    QString m_token;
    QByteArray m_buffer;  //缓冲区
};

#endif // CLIENTHANDLER_H
