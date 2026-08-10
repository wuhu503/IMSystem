#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QByteArray>
#include <QDateTime>

namespace Utils {
//密码加密
QString generateSalt(int length = 16);
QString hashPassword(const QString& password, const QString& salt);

//时间工具
qint64 currentTimestamp();
qint64 currentTimestampMs();

//UUID 生成
QString generateUUID();
QString generateUUIDWithoutHyphen();

} // namespace Utils

#endif // UTILS_H
