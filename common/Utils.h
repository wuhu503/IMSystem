#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QByteArray>

#include "Constants.h"

namespace Utils {

// ========== 口令 ==========
// 生成随机盐（使用系统密码学安全随机源），返回十六进制字符串
QString generateSalt(int byteLength = 16);

// 使用 PBKDF2-HMAC-SHA256 派生口令哈希，返回 "pbkdf2$<迭代次数>$<十六进制>" 格式
QString hashPassword(const QString &password, const QString &salt,
                     int iterations = IMConstants::kPasswordHashIterations);

// 校验口令；同时兼容历史版本的单轮 SHA256(password + salt) 十六进制哈希
bool verifyPassword(const QString &password, const QString &salt,
                    const QString &storedHash);

// 存储的哈希是否为旧算法（旧哈希应在登录成功后自动升级）
bool passwordHashNeedsUpgrade(const QString &storedHash);

// ========== UUID ==========
QString generateUUID();

} // namespace Utils

#endif // UTILS_H
