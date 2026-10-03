#ifndef USERREPOSITORY_H
#define USERREPOSITORY_H

#include "DbTypes.h"

#include <QPointer>
#include <QString>
#include <functional>

// users 表的读写：注册、登录校验、口令升级、在线状态更新
class UserRepository
{
public:
    static UserRepository& instance();

    UserRepository(const UserRepository&) = delete;
    UserRepository& operator=(const UserRepository&) = delete;

    // 注册：查重 + 加盐 + 哈希 + 插入都在数据库线程内一次完成，避免“先查后插”的竞态
    void registerUserAsync(const QString &username, const QString &password,
                           std::function<void(RegisterResult)> callback,
                           QPointer<QObject> receiver);

    // 登录校验：查询与口令校验都在数据库线程内完成
    void verifyUserAsync(const QString &username, const QString &password,
                         std::function<void(LoginCheckResult)> callback,
                         QPointer<QObject> receiver);

    // 旧哈希升级（登录成功后自动调用，重算在数据库线程内完成）
    void upgradePasswordHashAsync(qint64 userId, const QString &password,
                                  std::function<void(bool)> callback,
                                  QPointer<QObject> receiver);

    void getUserIdAsync(const QString &username,
                        std::function<void(qint64)> callback,
                        QPointer<QObject> receiver);

    void updateUserStatusAsync(qint64 userId, int status,
                               std::function<void(bool)> callback,
                               QPointer<QObject> receiver);

private:
    UserRepository() = default;
    ~UserRepository() = default;
};

#endif // USERREPOSITORY_H
