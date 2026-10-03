#include "Utils.h"

#include <QCryptographicHash>
#include <QPasswordDigestor>
#include <QRandomGenerator>
#include <QStringList>
#include <QUuid>

namespace {

constexpr int  kSaltByteLength  = 16;
constexpr int  kDerivedKeyBytes = 32;
constexpr char kPbkdf2Scheme[]  = "pbkdf2";

// 定长比较：避免比较耗时随匹配前缀变化而泄露信息
bool constantTimeEquals(const QString &lhs, const QString &rhs)
{
    if (lhs.size() != rhs.size()) {
        return false;
    }

    const QByteArray a = lhs.toLatin1();
    const QByteArray b = rhs.toLatin1();
    unsigned char diff = 0;
    for (int i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a.at(i)) ^ static_cast<unsigned char>(b.at(i));
    }
    return diff == 0;
}

} // namespace

namespace Utils {

// ========== 口令 ==========

QString generateSalt(int byteLength)
{
    if (byteLength <= 0) {
        byteLength = kSaltByteLength;
    }

    QByteArray salt(byteLength, '\0');
    for (int i = 0; i < byteLength; ++i) {
        // 必须使用 system()：global() 是梅森旋转，可被预测，不适合安全用途
        salt[i] = static_cast<char>(QRandomGenerator::system()->bounded(256));
    }
    return QString::fromLatin1(salt.toHex());
}

QString hashPassword(const QString &password, const QString &salt, int iterations)
{
    if (iterations <= 0) {
        iterations = IMConstants::kPasswordHashIterations;
    }

    // PBKDF2-HMAC-SHA256：慢哈希 + 随机盐，显著抬高离线爆破成本
    const QByteArray derived = QPasswordDigestor::deriveKeyPbkdf2(
        QCryptographicHash::Sha256,
        password.toUtf8(),
        salt.toUtf8(),
        iterations,
        kDerivedKeyBytes);

    return QStringLiteral("%1$%2$%3")
        .arg(QString::fromLatin1(kPbkdf2Scheme))
        .arg(iterations)
        .arg(QString::fromLatin1(derived.toHex()));
}

bool verifyPassword(const QString &password, const QString &salt, const QString &storedHash)
{
    if (storedHash.isEmpty()) {
        return false;
    }

    const QString prefix = QString::fromLatin1(kPbkdf2Scheme) + QLatin1Char('$');
    if (storedHash.startsWith(prefix)) {
        const QStringList parts = storedHash.split(QLatin1Char('$'));
        if (parts.size() != 3) {
            return false;
        }

        bool ok = false;
        const int iterations = parts.at(1).toInt(&ok);
        if (!ok || iterations <= 0) {
            return false;
        }
        return constantTimeEquals(hashPassword(password, salt, iterations), storedHash);
    }

    // 兼容历史数据：早期版本使用单轮 SHA256(password + salt)
    const QByteArray legacyHash =
        QCryptographicHash::hash((password + salt).toUtf8(), QCryptographicHash::Sha256);
    return constantTimeEquals(QString::fromLatin1(legacyHash.toHex()), storedHash);
}

bool passwordHashNeedsUpgrade(const QString &storedHash)
{
    const QString prefix = QString::fromLatin1(kPbkdf2Scheme) + QLatin1Char('$');
    return !storedHash.isEmpty() && !storedHash.startsWith(prefix);
}

// ========== UUID ==========

QString generateUUID()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

} // namespace Utils
