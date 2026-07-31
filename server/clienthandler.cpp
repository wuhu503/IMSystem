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

//发送消息给客户端
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

//解析来自客户端的数据
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

        //反序列化
        Message msg = Message::deserialize(data);
        //处理消息
        handleMessage(msg);
    }
}

void ClientHandler::onDisconnected()
{
    // 用户下线（检查是否仍在在线列表，避免踢人时重复移除）
    if (m_userId != -1) {
        if (UserManager::instance().isOnline(m_userId)) {
            UserManager::instance().userOffline(m_userId);
            // 异步更新数据库状态，避免阻塞主线程
            DbManager::instance().updateUserStatusAsync(m_userId, 0,
                [userId = m_userId](bool success) {
                    if (success) {
                        qInfo() << "用户离线状态已更新, userId:" << userId;
                    }
                }, this);
        }
        qInfo() << "用户离线, userId:" << m_userId;
    }
    
    qInfo() << "客户端断开连接, userId:" << m_userId 
            << ", address:" << m_socket->peerAddress().toString();
    
    emit clientDisconnect(m_socket->socketDescriptor());
}

//处理消息，根据消息的类型来分发业务
void ClientHandler::handleMessage(const Message &msg)
{
    emit messageReceived(m_userId, msg);
    
    // ========== 登录/注册请求不需要验证 token ==========
    if (msg.type() != MessageType::REQ_REGISTER && 
        msg.type() != MessageType::REQ_LOGIN) {
        // 其他请求需要验证 token
        if (!verifyToken(msg)) {
            sendAuthErrorResponse(msg.type(), msg.sequence(), "认证失败，请重新登录");
            return;
        }
    }
    
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

bool ClientHandler::verifyToken(const Message &msg)
{
    // 未登录用户
    if (m_userId == -1) {
        qWarning() << "未登录用户发送请求";
        return false;
    }
    
    QJsonObject body = msg.jsonBody();
    QString token = body["token"].toString();
    
    if (token.isEmpty() || token != m_token) {
        qWarning() << "token 验证失败, userId:" << m_userId 
                    << " 收到token:" << token << " 期望token:" << m_token;
        return false;
    }
    
    return true;
}

void ClientHandler::sendAuthErrorResponse(MessageType type, uint32_t sequence, const QString &reason)
{
    QJsonObject body;
    body["success"] = false;
    body["message"] = reason;
    
    Message response(type);
    response.setSequence(sequence);
    response.setJsonBody(body);
    sendMessage(response);
}


