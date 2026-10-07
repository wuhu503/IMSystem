#ifndef FRIENDSTORE_H
#define FRIENDSTORE_H

#include <QHash>
#include <QJsonArray>
#include <QList>
#include <QObject>
#include <QString>

// 好友条目：界面只读这几个字段，显示文案由 displayName()/statusText() 统一推导。
// 之前这些信息被拆散在 QListWidgetItem 的 UserRole 里，在线状态还被提前拼成
// "在线/离线" 字符串，于是会话标题和好友列表各读各的，刷新后必然有一个停在旧值。
struct FriendInfo {
    qint64  userId = -1;
    QString username;
    QString nickname;
    int     online = 0;

    bool isValid() const { return !username.isEmpty(); }
    QString displayName() const { return nickname.isEmpty() ? username : nickname; }
    QString statusText() const
    {
        return online == 1 ? QString::fromUtf8("在线") : QString::fromUtf8("离线");
    }
};

// 客户端好友状态模型：好友列表 + 每个好友的未读数。
// 只依赖 QJsonArray（服务端响应体），不依赖任何 widget，
// 因此未读数可以早于好友列表存在——登录瞬间补推的离线消息不会再丢红点。
class FriendStore : public QObject
{
    Q_OBJECT

public:
    explicit FriendStore(QObject *parent = nullptr);

    // 用服务端返回的好友列表整体替换本地状态（未读计数保留）
    void setFriends(const QJsonArray &friends);

    const QList<FriendInfo>& friends() const { return m_friends; }
    bool contains(const QString &username) const;
    FriendInfo find(const QString &username) const;

    int  unreadCount(const QString &username) const;
    bool hasUnread(const QString &username) const;
    void addUnread(const QString &username);
    void clearUnread(const QString &username);

signals:
    void listChanged();                            // 好友列表整体变化
    void unreadChanged(const QString &username);   // 单个好友的未读变化

private:
    QList<FriendInfo>   m_friends;
    QHash<QString, int> m_unread;
};

#endif // FRIENDSTORE_H
