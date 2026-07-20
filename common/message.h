#ifndef MESSAGE_H
#define MESSAGE_H

#include <QByteArray>
#include <QJsonObject>
#include <QJsonDocument>
#include "protocol.h"

class Message
{
public:
    Message();
    explicit Message(MessageType type);

    //消息头字段操作
    void setType(MessageType type);
    MessageType type() const;
    void setSequence(uint32_t seq);
    uint32_t sequence() const;

    //消息体操作
    void setBody(const QByteArray& body);
    QByteArray body() const;

    //序列化/反序列化
    QByteArray serialize() const;
    static Message deserialize(const QByteArray& data);

    //JSON
    void setJsonBody(const QJsonObject& json);
    QJsonObject jsonBody() const;

private:
    MessageHeader m_header;  // 16字节消息头
    QByteArray m_body;       // 消息体
};

#endif // MESSAGE_H
