#include "tcpclient.h"

TcpClient::TcpClient(QObject *parent)
    :QObject(parent)
    ,m_socket(new QTcpSocket(this))
    ,m_host("127.0.0.1")
    ,m_port(8080)
{
    connect(m_socket,&QTcpSocket::readyRead,this,&TcpClient::onReadyRead);
    connect(m_socket,&QTcpSocket::connected,this,&TcpClient::onConnected);
    connect(m_socket,&QTcpSocket::disconnected,this,&TcpClient::onDisconnected);
    connect(m_socket,&QTcpSocket::errorOccurred,this,&TcpClient::onErrorOccurred);
}

TcpClient& TcpClient::instance()
{
    static TcpClient instance;
    return instance;
}

TcpClient::~TcpClient()
{

}

void TcpClient::connectToServer(const QString& host,quint16 port)
{
    m_host = host;
    m_port = port;

    if(m_socket->state()!= QAbstractSocket::UnconnectedState)
    {
        m_socket->disconnect();
    }
    m_socket->connectToHost(host,port);
}

void TcpClient::disconnectToServer()
{
    if(m_socket->state()!=QAbstractSocket::ConnectedState)
    {
        qInfo()<<"未连接服务器";
        return;
    }
    m_socket->disconnectFromHost();
}

void TcpClient::sendMessage(const Message &msg)
{
    if(!isConnect())
    {
        qInfo()<<"未连接上服务器";
        return;
    }
    QByteArray data=msg.serialize();
    m_socket->write(data);
    m_socket->flush();
}

bool TcpClient::isConnect() const
{
    return m_socket->state()==QAbstractSocket::ConnectedState;
}

QString TcpClient::host() const
{
    return m_host;
}

quint16 TcpClient::port() const
{
    return m_port;
}

void TcpClient::onReadyRead()
{
    m_buffer.append(m_socket->readAll());

    while (m_buffer.size() >= HEADER_SIZE) {
        MessageHeader header;
        memcpy(&header, m_buffer.constData(), HEADER_SIZE);

        int totalSize = HEADER_SIZE + header.bodyLength;
        if (m_buffer.size() < totalSize) {
            break;
        }

        QByteArray data = m_buffer.left(totalSize);
        m_buffer.remove(0, totalSize);

        Message msg = Message::deserialize(data);
        emit messageReceived(msg);
    }
}

void TcpClient::onConnected()
{
    emit connectionEstablished();
}

void TcpClient::onDisconnected()
{
    emit connectionClosed();
}

void TcpClient::onErrorOccurred(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    qDebug() << "连接出错：" << m_socket->errorString();
    emit errorOccurred(m_socket->errorString());
}