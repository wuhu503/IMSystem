#include "TcpClient.h"

TcpClient::TcpClient(QObject *parent)
    :QObject(parent)
    ,m_socket(new QTcpSocket(this))
    ,m_host("127.0.0.1")
    ,m_port(8080)
    ,m_connecting(false)
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
    // 如果正在连接中，忽略此次请求
    if (m_connecting) {
        qInfo() << "正在连接中，请稍候...";
        return;
    }
    
    m_host = host;
    m_port = port;
    m_connecting = true;

    if(m_socket->state()!= QAbstractSocket::UnconnectedState)
    {
        // abort() 关闭连接但保留信号连接，disconnect() 会断开所有信号
        m_socket->abort();
    }
    m_socket->connectToHost(host,port);
}

void TcpClient::setServerAddress(const QString& host, quint16 port)
{
    // 仅更新目标地址缓存，不触发连接（供注册等流程沿用登录框配置）
    m_host = host;
    m_port = port;
}

void TcpClient::disconnectFromServer()
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
    if(!isConnected())
    {
        qInfo()<<"未连接上服务器";
        return;
    }
    
    QByteArray data=msg.serialize();
    m_socket->write(data);
}

void TcpClient::sendJsonMessage(MessageType type, const QJsonObject &body, uint32_t sequence)
{
    if (!isConnected()) {
        qInfo() << "未连接上服务器";
        return;
    }

    QJsonObject json = body;
    // 非登录/注册请求，自动注入 token（构造时完成，只序列化一次）
    if (type != MessageType::REQ_LOGIN &&
        type != MessageType::REQ_REGISTER &&
        !m_token.isEmpty()) {
        json["token"] = m_token;
    }

    Message msg(type);
    msg.setSequence(sequence);
    msg.setJsonBody(json);
    m_socket->write(msg.serialize());
}

bool TcpClient::isConnected() const
{
    return m_socket->state()==QAbstractSocket::ConnectedState;
}

bool TcpClient::isConnecting() const
{
    return m_connecting;
}

QString TcpClient::host() const
{
    return m_host;
}

quint16 TcpClient::port() const
{
    return m_port;
}

void TcpClient::setToken(const QString &token)
{
    m_token = token;
}

QString TcpClient::token() const
{
    return m_token;
}

void TcpClient::clearToken()
{
    m_token.clear();
}

void TcpClient::onReadyRead()
{
    m_buffer.append(m_socket->readAll());

    // 用读取偏移代替反复 remove(0, n)：后者在大量粘包时是 O(n²)
    int offset = 0;
    while (m_buffer.size() - offset >= HEADER_SIZE) {
        MessageHeader header;
        memcpy(&header, m_buffer.constData() + offset, HEADER_SIZE);

        // 协议校验：magic / version / body 长度上限，违规断开并清空缓冲
        if (header.magic != PROTOCOL_MAGIC ||
            header.version != PROTOCOL_VERSION ||
            header.bodyLength > MAX_BODY_SIZE) {
            qWarning() << "收到非法消息头，断开连接: magic="
                       << QString::number(header.magic, 16)
                       << "version=" << static_cast<int>(header.version)
                       << "bodyLength=" << header.bodyLength;
            m_buffer.clear();
            m_socket->disconnectFromHost();
            return;
        }

        qint64 totalSize = static_cast<qint64>(HEADER_SIZE) + header.bodyLength;
        if (m_buffer.size() - offset < totalSize) {
            break;
        }

        QByteArray data = m_buffer.mid(offset, static_cast<int>(totalSize));
        offset += static_cast<int>(totalSize);

        Message msg = Message::deserialize(data);
        if (!msg.isValid()) {
            qWarning() << "消息解析失败，断开连接";
            m_buffer.clear();
            m_socket->disconnectFromHost();
            return;
        }
        emit messageReceived(msg);
    }

    if (offset > 0) {
        m_buffer.remove(0, offset);
    }
}

void TcpClient::onConnected()
{
    m_connecting = false;  // 连接成功，重置连接状态
    emit connectionEstablished();
}

void TcpClient::onDisconnected()
{
    m_connecting = false;  // 断开连接，重置连接状态
    emit connectionClosed();
}

void TcpClient::onErrorOccurred(QAbstractSocket::SocketError error)
{
    m_connecting = false;  // 连接失败，重置连接状态

    // 对端正常关闭（例如服务端回收了空闲连接）不算错误：
    // 交给 connectionClosed 通知界面即可，避免弹出误导性的“连接错误”
    if (error == QAbstractSocket::RemoteHostClosedError) {
        return;
    }

    qDebug() << "连接出错：" << m_socket->errorString();
    emit errorOccurred(m_socket->errorString());
}
