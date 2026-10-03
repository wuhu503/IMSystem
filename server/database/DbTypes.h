#ifndef DBTYPES_H
#define DBTYPES_H

#include <QMetaType>
#include <QString>

// 仓储层对外返回的结果类型，放在单独头文件里供各 Repository 共用

// 登录校验结果（口令校验在数据库线程内完成，避免慢哈希阻塞主线程）
struct LoginCheckResult {
    qint64  userId = -1;         // -1 表示用户不存在
    bool    passwordOk = false;
    bool    needRehash = false;  // 命中旧哈希算法，登录成功后应自动升级
    QString salt;
};

// 注册结果
struct RegisterResult {
    bool    success = false;
    qint64  userId = -1;
    QString errorMessage;
};

// 添加好友请求的结果
enum class AddFriendResult {
    Success,            // 请求已写入
    AlreadyFriend,      // 已经是好友
    AlreadyRequested,   // 自己已经发过请求
    ReverseRequested,   // 对方已经向自己发过请求
    Failed
};

Q_DECLARE_METATYPE(LoginCheckResult)
Q_DECLARE_METATYPE(RegisterResult)

#endif // DBTYPES_H
