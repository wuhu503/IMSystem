#ifndef CHATSESSION_H
#define CHATSESSION_H

#include "Message.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>
#include <atomic>

// 客户端会话层：位于传输层（TcpClient）与界面之间。
// 负责连接、登录注册、序列号分配、待确认消息、心跳、已读回执，
// 并把服务端报文翻译成语义化信号；界面只关心“发生了什么”，不关心协议细节。
class ChatSession : public QObject
{
    Q_OBJECT

public:
    static ChatSession& instance();

    ChatSession(const ChatSession&) = delete;
    ChatSession& operator=(const ChatSession&) = delete;

    // ========== 连接 ==========
    void connectToServer(const QString &host, quint16 port);
    void setServerAddress(const QString &host, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;
    bool isConnecting() const;
    QString host() const;
    quint16 port() const;

    // ========== 账号 ==========
    void login(const QString &username, const QString &password);
    void registerAccount(const QString &username, const QString &password);
    void logout();

    // ========== 好友 ==========
    void requestFriendList();
    void requestPendingRequests();
    void addFriend(const QString &username);
    void acceptFriend(const QString &username);
    void rejectFriend(const QString &username);
    void deleteFriend(const QString &username);
    void searchUser(const QString &keyword);

    // ========== 聊天 ==========
    // 返回是否已发出；服务端确认后通过 textSendSucceeded/textSendFailed 通知
    bool sendText(const QString &receiver, const QString &content);
    void requestHistory(const QString &friendUsername, int limit, int offset);
    void markConversationRead(const QString &friendUsername);

    // 界面未就绪期间收到的文本消息（见 takePendingTexts 注释）
    struct IncomingText {
        QString sender;
        QString content;
        qint64  timestamp = 0;
    };

    // 登录成功那一刻服务端会立刻补推离线消息，而这时主窗口还没构造
    // （登录对话框还停在“登录成功”提示上）。这期间到达的消息先缓存在会话层，
    // 由界面就绪后取走并补渲染，否则消息既不上屏也不计入未读。
    QList<IncomingText> takePendingTexts();

signals:
    void connected();
    void disconnected();
    void transportError(const QString &error);

    void loginSucceeded(const QString &token);
    void loginFailed(const QString &message);
    void registerSucceeded();
    void registerFailed(const QString &message);
    void kickedOffline(const QString &message);

    // 好友相关响应原样带上响应体，界面只做展示
    void friendListReceived(const QJsonObject &body);
    void pendingRequestsReceived(const QJsonObject &body);
    void searchResultReceived(const QJsonObject &body);
    void addFriendResultReceived(const QJsonObject &body);
    void acceptFriendResultReceived(const QJsonObject &body);
    void rejectFriendResultReceived(const QJsonObject &body);
    void deleteFriendResultReceived(const QJsonObject &body);

    void newFriendRequestReceived();
    void friendAcceptedNotification();
    void friendRejectedNotification(const QString &username);
    void friendRemovedNotification(const QString &username);

    void textReceived(const QString &sender, const QString &content, qint64 timestamp);
    void textSendSucceeded(const QString &receiver, const QString &content);
    void textSendFailed(const QString &receiver, const QString &content, const QString &message);
    void historyReceived(const QString &friendUsername, const QJsonArray &messages);
    void historyFailed(const QString &friendUsername, const QString &message);

private slots:
    void onMessageReceived(const Message &msg);
    void onConnected();
    void onDisconnected();
    void onTransportError(const QString &error);
    void sendHeartbeat();
    void flushReadReceipt();

private:
    explicit ChatSession(QObject *parent = nullptr);
    ~ChatSession() override;

    uint32_t nextSequence();
    void sendJson(MessageType type, const QJsonObject &body, uint32_t sequence);
    void scheduleReadReceipt(const QString &friendUsername);
    void handleTextSendResult(uint32_t sequence, const QJsonObject &body);

    struct PendingText {
        QString receiver;
        QString content;
    };

    std::atomic<uint32_t> m_sequence{1};
    bool m_loggedIn = false;
    QHash<uint32_t, PendingText> m_pendingTexts;  // 已发出、等待服务端确认的文本
    QList<IncomingText> m_pendingIncoming;        // 界面未就绪时暂存的入站文本
    uint32_t m_historyRequestSeq = 0;             // 最近一次历史请求，用于丢弃过期响应
    QTimer *m_heartbeatTimer = nullptr;
    QTimer *m_readReceiptTimer = nullptr;
    QString m_pendingReadFriend;
};

#endif // CHATSESSION_H
