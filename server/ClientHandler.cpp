#include "ClientHandler.h"
#include "AuthService.h"
#include "FriendService.h"
#include "ChatService.h"
#include "UserManager.h"
#include "UserRepository.h"
#include "Constants.h"
#include <QDateTime>
#include <cstring>

namespace {

// 认证失败响应必须使用对应的 RSP_* 类型，否则客户端按类型分发时无法识别
MessageType authErrorResponseType(MessageType reqType)
{
    switch (reqType) {
    case MessageType::REQ_REGISTER:          return MessageType::RSP_REGISTER;
    case MessageType::REQ_LOGIN:             return MessageType::RSP_LOGIN;
    case MessageType::REQ_LOGOUT:            return MessageType::RSP_LOGOUT;
    case MessageType::REQ_ADD_FRIEND:        return MessageType::RSP_ADD_FRIEND;
    case MessageType::REQ_FRIEND_LIST:       return MessageType::RSP_FRIEND_LIST;
    case MessageType::REQ_ACCEPT_FRIEND:     return MessageType::RSP_ACCEPT_FRIEND;
    case MessageType::REQ_REJECT_FRIEND:     return MessageType::RSP_REJECT_FRIEND;
    case MessageType::REQ_DELETE_FRIEND:     return MessageType::RSP_DELETE_FRIEND;
    case MessageType::REQ_SEARCH_USER:       return MessageType::RSP_SEARCH_USER;
    case MessageType::REQ_PENDING_REQUESTS:  return MessageType::RSP_PENDING_REQUESTS;
    case MessageType::MSG_TEXT:              return MessageType::RSP_TEXT;
    case MessageType::REQ_HISTORY:           return MessageType::RSP_HISTORY;
    case MessageType::MSG_ACK:               return MessageType::MSG_ACK;
    default:                                 return reqType;
    }
}

} // namespace

ClientHandler::ClientHandler(QTcpSocket *socket, QObject *parent)
    : QObject(parent)
    , m_socket(socket)
    , m_userId(-1)
    , m_descriptor(-1)
{
    // 接管socket生命周期
    socket->setParent(this);

    m_descriptor = socket->socketDescriptor();
    m_lastActiveMs = QDateTime::currentMSecsSinceEpoch();

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

QString ClientHandler::username() const
{
    return m_username;
}

void ClientHandler::setUsername(const QString &username)
{
    m_username = username;
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

qintptr ClientHandler::descriptor() const
{
    return m_descriptor;
}

void ClientHandler::touch()
{
    m_lastActiveMs = QDateTime::currentMSecsSinceEpoch();
}

bool ClientHandler::isIdle(qint64 nowMs) const
{
    return (nowMs - m_lastActiveMs) > IMConstants::kConnectionIdleMs;
}

void ClientHandler::resetLoginState()
{
    m_userId = -1;
    m_username.clear();
    m_token.clear();
}

//发送消息给客户端
void ClientHandler::sendMessage(const Message &msg)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "发送失败：socket 未连接";
        return;
    }
    
    // 对端只连不读时写缓冲会持续膨胀，超过高水位主动断开，避免拖垮服务端内存
    if (m_socket->bytesToWrite() > IMConstants::kMaxSocketWriteBuffer) {
        qWarning() << "发送缓冲超过上限，断开连接:" << m_socket->bytesToWrite();
        m_socket->disconnectFromHost();
        return;
    }

    const QByteArray data = msg.serialize();
    if (data.isEmpty()) {
        return;
    }
    m_socket->write(data);
}

//解析来自客户端的数据
void ClientHandler::onReadyRead()
{
    m_buffer.append(m_socket->readAll());
    touch();
    
    int offset = 0;
    while (m_buffer.size() - offset >= HEADER_SIZE) {
        MessageHeader header;
        std::memcpy(&header, m_buffer.constData() + offset, HEADER_SIZE);

        // 协议校验
        if (header.magic != PROTOCOL_MAGIC ||
            header.version != PROTOCOL_VERSION ||
            header.bodyLength > MAX_BODY_SIZE) {
            qWarning() << "收到非法消息头，断开连接: magic="
                       << QString::number(header.magic, 16)
                       << "version=" << static_cast<int>(header.version)
                       << "bodyLength=" << header.bodyLength;
            m_socket->disconnectFromHost();
            return;
        }

        qint64 totalSize = static_cast<qint64>(HEADER_SIZE) + header.bodyLength;

        if (m_buffer.size() - offset < totalSize) {
            break;
        }

        QByteArray data = m_buffer.mid(offset, static_cast<int>(totalSize));
        offset += static_cast<int>(totalSize);

        //反序列化
        Message msg = Message::deserialize(data);
        if (!msg.isValid()) {
            qWarning() << "消息解析失败，断开连接";
            m_socket->disconnectFromHost();
            return;
        }
        //处理消息
        handleMessage(msg);

        // 退出登录会在 handleMessage 里主动断开，后续粘包数据不再处理
        if (m_socket->state() != QAbstractSocket::ConnectedState) {
            m_buffer.clear();
            return;
        }
    }

    if (offset > 0) {
        m_buffer.remove(0, offset);
    }
}

void ClientHandler::onDisconnected()
{
    // 用户下线（检查是否仍在在线列表，避免踢人时重复移除）
    if (m_userId != -1) {
        if (UserManager::instance().isOnline(m_userId)) {
            UserManager::instance().userOffline(m_userId);
            // 异步更新数据库状态，避免阻塞主线程
            UserRepository::instance().updateUserStatusAsync(m_userId, 0,
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
    
    emit clientDisconnect(m_descriptor);
}

//处理消息，根据消息的类型来分发业务
void ClientHandler::handleMessage(const Message &msg)
{
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

    case MessageType::REQ_LOGOUT:
        qInfo() << "收到退出登录请求";
        if (m_userId != -1) {
            if (UserManager::instance().isOnline(m_userId)) {
                UserManager::instance().userOffline(m_userId);
            }
            UserRepository::instance().updateUserStatusAsync(m_userId, 0, nullptr, this);
            qInfo() << "用户退出登录, userId:" << m_userId;
        }
        {
            QJsonObject body;
            body["success"] = true;
            body["message"] = "退出成功";
            Message response(MessageType::RSP_LOGOUT);
            response.setSequence(msg.sequence());
            response.setJsonBody(body);
            sendMessage(response);
        }
        // 先清登录态再断开：断开回调里不会再把它当成已登录用户处理
        resetLoginState();
        m_socket->disconnectFromHost();
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
        ChatService::instance().handleTextMessage(this, msg);
        break;
        
    case MessageType::REQ_HISTORY:
        ChatService::instance().handleHistoryRequest(this, msg);
        break;
        
    case MessageType::MSG_ACK:
        ChatService::instance().handleMessageAck(this, msg);
        break;
        
    case MessageType::HEARTBEAT:
        // 心跳只用来刷新活跃时间（onReadyRead 已 touch），无需回包
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
    
    Message response(authErrorResponseType(type));
    response.setSequence(sequence);
    response.setJsonBody(body);
    sendMessage(response);
}


