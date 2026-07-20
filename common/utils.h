#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QByteArray>
#include <QDateTime>

namespace Utils {
//密码加密
QString generateSalt(int length = 16);
QString hashPassword(const QString& password, const QString& salt);
bool verifyPassword(const QString& password, const QString& salt, const QString& storedHash);

//时间工具
qint64 currentTimestamp();
qint64 currentTimestampMs();
QString formatTimestamp(qint64 timestamp, const QString& format = "yyyy-MM-dd HH:mm:ss");
QString currentDateTime(const QString& format = "yyyy-MM-dd HH:mm:ss");

//UUID 生成
QString generateUUID();
QString generateUUIDWithoutHyphen();

} // namespace Utils

#endif // UTILS_H
