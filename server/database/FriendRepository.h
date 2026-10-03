#ifndef FRIENDREPOSITORY_H
#define FRIENDREPOSITORY_H

#include "DbTypes.h"

#include <QJsonArray>
#include <QPointer>
#include <QString>
#include <functional>

// friendships 表 + 好友相关的用户检索
class FriendRepository
{
public:
    static FriendRepository& instance();

    FriendRepository(const FriendRepository&) = delete;
    FriendRepository& operator=(const FriendRepository&) = delete;

    void getFriendListAsync(qint64 userId,
                            std::function<void(QJsonArray)> callback,
                            QPointer<QObject> receiver);

    void isFriendAsync(qint64 userId, qint64 friendId,
                       std::function<void(bool)> callback,
                       QPointer<QObject> receiver);

    void searchUsersAsync(const QString &keyword, qint64 excludeUserId,
                          std::function<void(QJsonArray)> callback,
                          QPointer<QObject> receiver);

    void getPendingFriendRequestsAsync(qint64 userId,
                                       std::function<void(QJsonArray)> callback,
                                       QPointer<QObject> receiver);

    // 查重（两个方向）与写入在同一事务内完成，避免双向重复请求
    void addFriendRequestAsync(qint64 userId, qint64 friendId,
                               std::function<void(AddFriendResult)> callback,
                               QPointer<QObject> receiver);

    void acceptFriendRequestAsync(qint64 userId, qint64 friendId,
                                  std::function<void(bool)> callback,
                                  QPointer<QObject> receiver);

    void rejectFriendRequestAsync(qint64 userId, qint64 friendId,
                                  std::function<void(bool)> callback,
                                  QPointer<QObject> receiver);

    void deleteFriendAsync(qint64 userId, qint64 friendId,
                           std::function<void(bool)> callback,
                           QPointer<QObject> receiver);

private:
    FriendRepository() = default;
    ~FriendRepository() = default;
};

#endif // FRIENDREPOSITORY_H
