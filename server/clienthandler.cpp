#include "clienthandler.h"
#include "authservice.h"
#include <cstring>

ClientHandler::ClientHandler(QTcpSocket *socket, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
    , m_userId(-1)
{
    // 连接信号槽
    // readyRead：有数据可读时触发
    // disconnected：客户端断开时触发
    connect(m_socket, &QTcpSocket::readyRead, this, &ClientHandler::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ClientHandler::onDisconnected);
    
    qInfo() << "新客户端连接:" << m_socket->peerAddress().toString();
}


ClientHandler::~ClientHandler()
{
    if (m_socket && m_socket->isOpen()) {
        m_socket->disconnectFromHost();
    }
}

qint64 ClientHandler::userId() const
{
    return m_userId;
}

void ClientHandler::setUserId(qint64 id)
{
    m_userId = id;
    qInfo() << "用户登录, userId:" << id;
}

QTcpSocket* ClientHandler::socket() const
{
    return m_socket;
}

void ClientHandler::sendMessage(const Message &msg)
{
    if (!m_socket || !m_socket->isOpen()) {
        qWarning() << "发送失败：socket 未连接";
        return;
    }
    
    // 序列化消息
    QByteArray data = msg.serialize();
    
    // 发送数据
    m_socket->write(data);
    m_socket->flush();
}


void ClientHandler::onReadyRead()
{
    // 1. 把新数据追加到缓冲区
    m_buffer.append(m_socket->readAll());
    
    // 2. 循环处理完整消息
    while (m_buffer.size() >= HEADER_SIZE) {
        
        // 3. 解析 header（前16字节）
        MessageHeader header;
        std::memcpy(&header, m_buffer.constData(), HEADER_SIZE);
        
        // 4. 检查数据完整性
        //    总长度 = header(16) + body(bodyLength)
        int totalSize = HEADER_SIZE + header.bodyLength;
        
        if (m_buffer.size() < totalSize) {
            // 数据不完整，等待更多数据
            break;
        }
        
        // 5. 提取完整消息
        QByteArray data = m_buffer.left(totalSize);
        
        // 6. 从缓冲区移除已处理的数据
        m_buffer.remove(0, totalSize);
        
        // 7. 反序列化并处理
        Message msg = Message::deserialize(data);
        handleMessage(msg);
    }
}

void ClientHandler::onDisconnected()
{
    // 如果用户已登录，更新离线状态
    if (m_userId != -1) {
        DbManager::instance().updateUserStatus(m_userId, 0);
        qInfo() << "用户离线, userId:" << m_userId;
    }
    
    qInfo() << "客户端断开连接, userId:" << m_userId 
            << ", address:" << m_socket->peerAddress().toString();
    
    // 发送断开信号
    emit clientDisconnect(m_socket->socketDescriptor());
}


void ClientHandler::handleMessage(const Message &msg)
{
    // 发送消息接收信号（供日志或其他模块使用）
    emit messageReceived(m_userId, msg);
    
    // 根据消息类型分发处理
    switch (msg.type()) {
        
    case MessageType::REQ_REGISTER:
        // 注册请求 → 调用 AuthService 处理
        qInfo() << "收到注册请求";
        AuthService::instance().handleRegister(this, msg);
        break;
        
    case MessageType::REQ_LOGIN:
        // 登录请求 → 调用 AuthService 处理
        qInfo() << "收到登录请求";
        AuthService::instance().handleLogin(this, msg);
        break;
        
    case MessageType::MSG_TEXT:
        // 文本消息 → 转发给目标用户
        qInfo() << "收到文本消息";
        // TODO: 转发消息（后续实现）
        break;
        
    case MessageType::HEARTBEAT:
        // 心跳包 → 更新最后活跃时间
        // TODO: 更新心跳时间（后续实现）
        break;
        
    case MessageType::MSG_ACK:
        // 消息确认 → 更新消息状态
        qInfo() << "收到消息确认";
        break;
        
    default:
        qWarning() << "未知消息类型:" << static_cast<int>(msg.type());
        break;
    }
}
