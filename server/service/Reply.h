#ifndef REPLY_H
#define REPLY_H

#include "ClientHandler.h"
#include "Message.h"

#include <QDebug>
#include <QJsonObject>

// 服务端响应构造与发送的唯一入口。
// 之前 authservice / friendservice / chatservice 各写了一份
// “拼 success/message + 合并 data + 判空发送” 的相同代码，这里统一收口。
namespace Reply {

inline Message make(MessageType type, uint32_t sequence, const QJsonObject &body)
{
    Message msg(type);
    msg.setSequence(sequence);
    msg.setJsonBody(body);
    return msg;
}

// success = true；data 里的字段会合并进响应体
inline Message success(MessageType type, uint32_t sequence,
                       const QJsonObject &data = QJsonObject())
{
    QJsonObject body;
    body["success"] = true;
    body["message"] = QStringLiteral("操作成功");

    for (auto it = data.begin(); it != data.end(); ++it) {
        body[it.key()] = it.value();
    }
    return make(type, sequence, body);
}

// success = false，附带错误提示
inline Message failure(MessageType type, uint32_t sequence, const QString &message)
{
    QJsonObject body;
    body["success"] = false;
    body["message"] = message;
    return make(type, sequence, body);
}

// 发送（连接可能已经断开，内部判空）
inline void send(ClientHandler *client, const Message &msg)
{
    if (client) {
        client->sendMessage(msg);
    }
}

inline void sendSuccess(ClientHandler *client, MessageType type, uint32_t sequence,
                        const QJsonObject &data = QJsonObject())
{
    send(client, success(type, sequence, data));
}

// tag 用于区分是哪个业务的失败日志，例如 "认证失败:"
inline void sendFailure(ClientHandler *client, MessageType type, uint32_t sequence,
                        const QString &message, const char *tag)
{
    send(client, failure(type, sequence, message));
    qWarning() << (tag ? tag : "") << message;
}

} // namespace Reply

#endif // REPLY_H
