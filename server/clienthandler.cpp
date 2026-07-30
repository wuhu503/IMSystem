#include "clienthandler.h"
#include "authservice.h"
#include "friendservice.h"
#include "chatservice.h"
#include "usermanager.h"
#include <cstring>

ClientHandler::ClientHandler(QTcpSocket *socket, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
    , m_userId(-1)
{
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

QString ClientHandler::token() const
{
    return m_token;
}

void ClientHandler::setToken(const QString &token)
{
    m_token = token;
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
    
    QByteArray data = msg.serialize();
    m_socket->write(data);
    m_socket->flush();
}

void ClientHandler::onReadyRead()
{
    m_buffer.append(m_socket->readAll());
    
    while (m_buffer.size() >= HEADER_SIZE) {
        MessageHeader header;
        std::memcpy(&header, m_buffer.constData(), HEADER_SIZE);
        
        int totalSize = HEADER_SIZE + header.bodyLength;
        
        if (m_buffer.size() < totalSize) {
            break;
        }
        
        QByteArray data = m_buffer.left(totalSize);
        m_buffer.remove(0, totalSize);
        
        Message msg = Message::deserialize(data);
        handleMessage(msg);
    }
}

void ClientHandler::onDisconnected()
{
    // 用户下线
    if (m_userId != -1) {
        UserManager::instance().userOffline(m_userId);
        DbManager::instance().updateUserStatus(m_userId, 0);
        qInfo() << "用户离线, userId:" << m_userId;
    }
    
    qInfo() << "客户端断开连接, userId:" << m_userId 
            << ", address:" << m_socket->peerAddress().toString();
    
    emit clientDisconnect(m_socket->socketDescriptor());
}

void ClientHandler::handleMessage(const Message &msg)
{
    emit messageReceived(m_userId, msg);
    
    switch (msg.type()) {
        
    // ========== 认证系统 ==========
    case MessageType::REQ_REGISTER:
        qInfo() << "收到注册请求";
        AuthService::instance().handleRegister(this, msg);
        break;
        
    case MessageType::REQ_LOGIN:
        qInfo() << "收到登录请求";
        AuthService::instance().handleLogin(this, msg);
        break;
        
    // ========== 好友系统 ==========
    case MessageType::REQ_ADD_FRIEND:
        qInfo() << "收到添加好友请求";
        FriendService::instance().handleAddFriend(this, msg);
        break;
        
    case MessageType::REQ_FRIEND_LIST:
        qInfo() << "收到好友列表请求";
        FriendService::instance().handleFriendList(this, msg);
        break;
        
    case MessageType::REQ_ACCEPT_FRIEND:
        qInfo() << "收到接受好友请求";
        FriendService::instance().handleAcceptFriend(this, msg);
        break;
        
    case MessageType::REQ_REJECT_FRIEND:
        qInfo() << "收到拒绝好友请求";
        FriendService::instance().handleRejectFriend(this, msg);
        break;
        
    case MessageType::REQ_DELETE_FRIEND:
        qInfo() << "收到删除好友请求";
        FriendService::instance().handleDeleteFriend(this, msg);
        break;
        
    case MessageType::REQ_SEARCH_USER:
        qInfo() << "收到搜索用户请求";
        FriendService::instance().handleSearchUser(this, msg);
        break;
        
    case MessageType::REQ_PENDING_REQUESTS:
        qInfo() << "收到获取待处理好友请求";
        FriendService::instance().handlePendingRequests(this, msg);
        break;
        
    // ========== 聊天系统 ==========
    case MessageType::MSG_TEXT:
        qInfo() << "收到文本消息";
        ChatService::instance().handleTextMessage(this, msg);
        break;
        
    case MessageType::MSG_HISTORY:
        qInfo() << "收到历史消息请求";
        ChatService::instance().handleHistoryRequest(this, msg);
        break;
        
    case MessageType::MSG_ACK:
        qInfo() << "收到消息确认";
        ChatService::instance().handleMessageAck(this, msg);
        break;
        
    case MessageType::HEARTBEAT:
        qInfo() << "收到心跳包";
        break;
        
    default:
        qWarning() << "未知消息类型:" << static_cast<int>(msg.type());
        break;
    }
}
