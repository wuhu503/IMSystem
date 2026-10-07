#include "ChatSession.h"

#include "Constants.h"
#include "TcpClient.h"

#include <QDebug>
#include <QMetaMethod>
#include <QTimer>

ChatSession& ChatSession::instance()
{
    static ChatSession instance;
    return instance;
}

ChatSession::ChatSession(QObject *parent)
    : QObject(parent)
{
    TcpClient &tcp = TcpClient::instance();

    connect(&tcp, &TcpClient::messageReceived,
            this, &ChatSession::onMessageReceived);
    connect(&tcp, &TcpClient::connectionEstablished,
            this, &ChatSession::onConnected);
    connect(&tcp, &TcpClient::connectionClosed,
            this, &ChatSession::onDisconnected);
    connect(&tcp, &TcpClient::errorOccurred,
            this, &ChatSession::onTransportError);

    // 心跳：让服务端知道对端还活着，否则空闲连接会被回收
    m_heartbeatTimer = new QTimer(this);
    m_heartbeatTimer->setInterval(IMConstants::kHeartbeatIntervalMs);
    connect(m_heartbeatTimer, &QTimer::timeout, this, &ChatSession::sendHeartbeat);
    m_heartbeatTimer->start();

    // 已读回执去抖：同一会话连续来消息时只上报一次，避免每条消息都写一次数据库
    m_readReceiptTimer = new QTimer(this);
    m_readReceiptTimer->setSingleShot(true);
    m_readReceiptTimer->setInterval(1500);
    connect(m_readReceiptTimer, &QTimer::timeout, this, &ChatSession::flushReadReceipt);
}

ChatSession::~ChatSession() = default;

uint32_t ChatSession::nextSequence()
{
    return m_sequence.fetch_add(1);
}

void ChatSession::sendJson(MessageType type, const QJsonObject &body, uint32_t sequence)
{
    TcpClient::instance().sendJsonMessage(type, body, sequence);
}

// ========== 连接 ==========

void ChatSession::connectToServer(const QString &host, quint16 port)
{
    TcpClient::instance().connectToServer(host, port);
}

void ChatSession::setServerAddress(const QString &host, quint16 port)
{
    TcpClient::instance().setServerAddress(host, port);
}

void ChatSession::disconnectFromServer()
{
    TcpClient::instance().disconnectFromServer();
}

bool ChatSession::isConnected() const
{
    return TcpClient::instance().isConnected();
}

bool ChatSession::isConnecting() const
{
    return TcpClient::instance().isConnecting();
}

QString ChatSession::host() const
{
    return TcpClient::instance().host();
}

quint16 ChatSession::port() const
{
    return TcpClient::instance().port();
}

// ========== 账号 ==========

void ChatSession::login(const QString &username, const QString &password)
{
    QJsonObject body;
    body["username"] = username;
    body["password"] = password;
    sendJson(MessageType::REQ_LOGIN, body, nextSequence());
}

void ChatSession::registerAccount(const QString &username, const QString &password)
{
    QJsonObject body;
    body["username"] = username;
    body["password"] = password;
    sendJson(MessageType::REQ_REGISTER, body, nextSequence());
}

void ChatSession::logout()
{
    flushReadReceipt();  // 退出前把待上报的已读回执发出去
    sendJson(MessageType::REQ_LOGOUT, QJsonObject(), nextSequence());
    disconnectFromServer();
    TcpClient::instance().clearToken();
    m_loggedIn = false;
}

// ========== 好友 ==========

void ChatSession::requestFriendList()
{
    sendJson(MessageType::REQ_FRIEND_LIST, QJsonObject(), nextSequence());
}

void ChatSession::requestPendingRequests()
{
    sendJson(MessageType::REQ_PENDING_REQUESTS, QJsonObject(), nextSequence());
}

void ChatSession::addFriend(const QString &username)
{
    QJsonObject body;
    body["username"] = username;
    sendJson(MessageType::REQ_ADD_FRIEND, body, nextSequence());
}

void ChatSession::acceptFriend(const QString &username)
{
    QJsonObject body;
    body["username"] = username;
    sendJson(MessageType::REQ_ACCEPT_FRIEND, body, nextSequence());
}

void ChatSession::rejectFriend(const QString &username)
{
    QJsonObject body;
    body["username"] = username;
    sendJson(MessageType::REQ_REJECT_FRIEND, body, nextSequence());
}

void ChatSession::deleteFriend(const QString &username)
{
    QJsonObject body;
    body["username"] = username;
    sendJson(MessageType::REQ_DELETE_FRIEND, body, nextSequence());
}

void ChatSession::searchUser(const QString &keyword)
{
    QJsonObject body;
    body["keyword"] = keyword;
    sendJson(MessageType::REQ_SEARCH_USER, body, nextSequence());
}

// ========== 聊天 ==========

bool ChatSession::sendText(const QString &receiver, const QString &content)
{
    if (!isConnected()) {
        return false;
    }

    QJsonObject body;
    body["receiver"] = receiver;
    body["content"] = content;

    // 先登记再发送：服务端确认（或失败）回来时按序列号找回原文
    const uint32_t sequence = nextSequence();
    m_pendingTexts.insert(sequence, PendingText{receiver, content});
    sendJson(MessageType::MSG_TEXT, body, sequence);
    return true;
}

void ChatSession::requestHistory(const QString &friendUsername, int limit, int offset)
{
    // 记下本次请求的序列号：响应回来时比对，丢弃切换会话后才到达的旧响应
    const uint32_t sequence = nextSequence();
    m_historyRequestSeq = sequence;

    QJsonObject body;
    body["username"] = friendUsername;
    body["limit"] = limit;
    body["offset"] = offset;
    sendJson(MessageType::REQ_HISTORY, body, sequence);
}

void ChatSession::markConversationRead(const QString &friendUsername)
{
    scheduleReadReceipt(friendUsername);
}

void ChatSession::scheduleReadReceipt(const QString &friendUsername)
{
    if (friendUsername.isEmpty()) {
        return;
    }
    m_pendingReadFriend = friendUsername;
    m_readReceiptTimer->start();  // 重启计时：连续消息只会在安静 1.5 秒后上报一次
}

void ChatSession::flushReadReceipt()
{
    if (m_pendingReadFriend.isEmpty()) {
        return;
    }

    QJsonObject body;
    body["username"] = m_pendingReadFriend;
    sendJson(MessageType::MSG_ACK, body, nextSequence());
    m_pendingReadFriend.clear();
}

void ChatSession::sendHeartbeat()
{
    if (isConnected()) {
        sendJson(MessageType::HEARTBEAT, QJsonObject(), nextSequence());
    }
}

// ========== 报文分发 ==========

void ChatSession::onMessageReceived(const Message &msg)
{
    const QJsonObject body = msg.jsonBody();

    switch (msg.type()) {
    case MessageType::RSP_LOGIN:
        if (body["success"].toBool()) {
            m_loggedIn = true;
            TcpClient::instance().setToken(body["token"].toString());
            emit loginSucceeded(body["token"].toString());
        } else if (m_loggedIn) {
            // 主窗口运行期间收到失败的 RSP_LOGIN，只可能是被其他设备顶下线
            m_loggedIn = false;
            TcpClient::instance().clearToken();
            emit kickedOffline(body["message"].toString());
        } else {
            emit loginFailed(body["message"].toString());
        }
        break;

    case MessageType::RSP_REGISTER:
        if (body["success"].toBool()) {
            emit registerSucceeded();
        } else {
            emit registerFailed(body["message"].toString());
        }
        break;

    case MessageType::RSP_FRIEND_LIST:
        emit friendListReceived(body);
        break;

    case MessageType::RSP_PENDING_REQUESTS:
        emit pendingRequestsReceived(body);
        break;

    case MessageType::RSP_SEARCH_USER:
        emit searchResultReceived(body);
        break;

    case MessageType::RSP_ADD_FRIEND:
        emit addFriendResultReceived(body);
        break;

    case MessageType::RSP_ACCEPT_FRIEND:
        emit acceptFriendResultReceived(body);
        break;

    case MessageType::RSP_REJECT_FRIEND:
        emit rejectFriendResultReceived(body);
        break;

    case MessageType::RSP_DELETE_FRIEND:
        emit deleteFriendResultReceived(body);
        break;

    case MessageType::NTF_FRIEND_REQUEST:
        emit newFriendRequestReceived();
        break;

    case MessageType::NTF_FRIEND_ACCEPTED:
        emit friendAcceptedNotification();
        break;

    case MessageType::NTF_FRIEND_REJECTED:
        emit friendRejectedNotification(body["username"].toString());
        break;

    case MessageType::NTF_FRIEND_REMOVED:
        emit friendRemovedNotification(body["username"].toString());
        break;

    case MessageType::MSG_TEXT:
    {
        IncomingText text;
        text.sender    = body["sender"].toString();
        text.content   = body["content"].toString();
        text.timestamp = body["timestamp"].toVariant().toLongLong();

        // 还没有界面在听（登录刚成功、主窗口尚未构造）时先存起来，
        // 等界面就绪后由 takePendingTexts() 补发
        if (isSignalConnected(QMetaMethod::fromSignal(&ChatSession::textReceived))) {
            emit textReceived(text.sender, text.content, text.timestamp);
        } else {
            m_pendingIncoming.append(text);
        }
        break;
    }

    case MessageType::RSP_TEXT:
        handleTextSendResult(msg.sequence(), body);
        break;

    case MessageType::RSP_HISTORY:
        // 只接受最近一次历史请求的响应，避免切换会话后旧响应把记录串到别的窗口
        if (msg.sequence() != m_historyRequestSeq) {
            break;
        }
        if (body["success"].toBool()) {
            emit historyReceived(body["friend_username"].toString(), body["messages"].toArray());
        } else {
            emit historyFailed(body["friend_username"].toString(), body["message"].toString());
        }
        break;

    default:
        break;
    }
}

void ChatSession::handleTextSendResult(uint32_t sequence, const QJsonObject &body)
{
    auto it = m_pendingTexts.find(sequence);
    if (it == m_pendingTexts.end()) {
        return;  // 不是本会话待确认的消息（例如退出登录时已清空）
    }

    const QString receiver = it->receiver;
    const QString content = it->content;
    m_pendingTexts.erase(it);

    if (body["success"].toBool()) {
        emit textSendSucceeded(receiver, content);
    } else {
        emit textSendFailed(receiver, content, body["message"].toString());
    }
}

QList<ChatSession::IncomingText> ChatSession::takePendingTexts()
{
    QList<IncomingText> pending;
    pending.swap(m_pendingIncoming);
    return pending;
}

void ChatSession::onConnected()
{
    emit connected();
}

void ChatSession::onDisconnected()
{
    m_pendingTexts.clear();
    m_pendingIncoming.clear();
    m_pendingReadFriend.clear();
    m_loggedIn = false;
    emit disconnected();
}

void ChatSession::onTransportError(const QString &error)
{
    emit transportError(error);
}
