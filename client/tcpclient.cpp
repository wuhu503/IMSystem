#include "tcpclient.h"

TcpClient::TcpClient(QObject *parent)
    :QObject(parent)
    ,m_socket(new QTcpSocket(this))
{
    connect(m_socket,&QTcpSocket::readyRead,this,&TcpClient::onReadyRead);
    connect(m_socket,&QTcpSocket::connected,this,&TcpClient::onConnected);
    connect(m_socket,&QTcpSocket::disconnected,this,&TcpClient::onDisconnected);
    connect(m_socket,&QTcpSocket::errorOccurred,this,&TcpClient::onErrorOccurred);
}

//单例实现
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

void TcpClient::onReadyRead()
{
    // 追加数据到缓冲区
    m_buffer.append(m_socket->readAll());

    // 循环处理完整消息
    while (m_buffer.size() >= HEADER_SIZE) {
        MessageHeader header;
        memcpy(&header, m_buffer.constData(), HEADER_SIZE);

        int totalSize = HEADER_SIZE + header.bodyLength;
        if (m_buffer.size() < totalSize) {
            break;  // 数据不完整
        }

        QByteArray data = m_buffer.left(totalSize);
        m_buffer.remove(0, totalSize);

        Message msg = Message::deserialize(data);
        emit messageReceived(msg);  // 发送信号
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


























