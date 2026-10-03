#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <QtGlobal>

// 全局常量集中定义，避免魔法数字散落在各处的业务代码里
namespace IMConstants {

// ========== 网络 ==========
constexpr quint16 kDefaultServerPort     = 8080;
constexpr int     kHeartbeatIntervalMs   = 20 * 1000;   // 客户端心跳发送间隔
constexpr int     kIdleSweepIntervalMs   = 10 * 1000;   // 服务端空闲连接扫描周期
constexpr int     kConnectionIdleMs      = 60 * 1000;   // 超过该时长没有任何报文则断开
constexpr qint64  kMaxSocketWriteBuffer  = 4 * 1024 * 1024;  // 发送缓冲高水位

// ========== 账号 ==========
constexpr int kUsernameMinLength = 3;
constexpr int kUsernameMaxLength = 20;
constexpr int kPasswordMinLength = 6;
constexpr int kPasswordMaxLength = 64;

// 登录失败限速
constexpr int kMaxLoginFailures   = 5;
constexpr int kLoginLockoutSecs   = 60;

// ========== 消息 ==========
constexpr int kMaxTextContentLength   = 2000;
constexpr int kDefaultHistoryLimit    = 50;
constexpr int kMaxHistoryLimit        = 100;
constexpr int kOfflineMessageBatch    = 200;

// ========== 好友 ==========
constexpr int kMaxSearchResults       = 20;

// ========== 口令哈希 ==========
constexpr int kPasswordHashIterations = 120000;

} // namespace IMConstants

#endif // CONSTANTS_H
