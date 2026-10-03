#ifndef MESSAGE_H
#define MESSAGE_H

#include <QByteArray>
#include <QJsonObject>
#include <QJsonDocument>
#include "Protocol.h"

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
    bool isValid() const;

    //JSON
    void setJsonBody(const QJsonObject& json);
    QJsonObject jsonBody() const;

private:
    static Message invalidMessage();

    MessageHeader m_header;  // 16字节消息头
    QByteArray m_body;       // 消息体
    bool m_valid;            // 消息是否有效（反序列化失败时为 false）
};

#endif // MESSAGE_H
