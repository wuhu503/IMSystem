#include "FriendStore.h"

#include <QJsonObject>
#include <QJsonValue>

FriendStore::FriendStore(QObject *parent)
    : QObject(parent)
{
}

void FriendStore::setFriends(const QJsonArray &friends)
{
    m_friends.clear();
    m_friends.reserve(friends.size());

    for (const QJsonValue &value : friends) {
        const QJsonObject obj = value.toObject();

        FriendInfo info;
        info.userId   = obj["user_id"].toVariant().toLongLong();
        info.username = obj["username"].toString();
        info.nickname = obj["nickname"].toString();
        info.online   = obj["status"].toInt();

        if (info.isValid()) {
            m_friends.append(info);
        }
    }

    emit listChanged();
}

bool FriendStore::contains(const QString &username) const
{
    return find(username).isValid();
}

FriendInfo FriendStore::find(const QString &username) const
{
    if (username.isEmpty()) {
        return FriendInfo();
    }

    for (const FriendInfo &info : m_friends) {
        if (info.username == username) {
            return info;
        }
    }
    return FriendInfo();
}

int FriendStore::unreadCount(const QString &username) const
{
    return m_unread.value(username, 0);
}

bool FriendStore::hasUnread(const QString &username) const
{
    return unreadCount(username) > 0;
}

void FriendStore::addUnread(const QString &username)
{
    if (username.isEmpty()) {
        return;
    }

    m_unread[username] = m_unread.value(username, 0) + 1;
    emit unreadChanged(username);
}

void FriendStore::clearUnread(const QString &username)
{
    if (!m_unread.contains(username)) {
        return;
    }

    m_unread.remove(username);
    emit unreadChanged(username);
}
