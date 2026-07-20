#include "utils.h"
#include <QCryptographicHash>
#include <QUuid>
#include <QRandomGenerator>
#include <QTimeZone>

namespace Utils {

//密码加密
QString generateSalt(int length)
{
    //创建一个指定长度的字节数组
    QByteArray salt(length, '\0');
    //用随机数填充
    for (int i = 0; i < length; ++i) {
        salt[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
    }
    //把字节数组转换为十六进制字符串
    return QString::fromLatin1(salt.toHex());
}

QString hashPassword(const QString& password, const QString& salt)
{
    //拼接密码和盐值
    QByteArray data = (password + salt).toUtf8();

    //计算 SHA256 哈希
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    
    //转换为十六进制字符
    return QString::fromLatin1(hash.toHex());
}

//验证密码
bool verifyPassword(const QString& password, const QString& salt, const QString& storedHash)
{

    QString computedHash = hashPassword(password, salt);
    return computedHash == storedHash;
}

//时间工具
qint64 currentTimestamp()
{
    return QDateTime::currentSecsSinceEpoch();
}

qint64 currentTimestampMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}


QString formatTimestamp(qint64 timestamp, const QString& format)
{

    QDateTime dateTime = QDateTime::fromSecsSinceEpoch(timestamp, QTimeZone::utc());
    return dateTime.toString(format);
}

QString currentDateTime(const QString& format)
{
    return QDateTime::currentDateTime().toString(format);
}

//UUID 生成
QString generateUUID()
{
    return QUuid::createUuid().toString().mid(1, 36);
}

QString generateUUIDWithoutHyphen()
{

    return QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-');
}

} // namespace Utils
