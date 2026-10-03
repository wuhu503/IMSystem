#include "Message.h"
#include <cstring>
#include <QDebug>
Message::Message()
    : m_valid(true)
{
    std::memset(&m_header, 0, sizeof(MessageHeader));
    m_header.magic = PROTOCOL_MAGIC;
    m_header.version = PROTOCOL_VERSION;
}


Message::Message(MessageType type) : Message()
{
    m_header.type = static_cast<uint16_t>(type);
}

//消息头字段操作：
void Message::setType(MessageType type)
{
    m_header.type = static_cast<uint16_t>(type);
}

MessageType Message::type() const
{
    return static_cast<MessageType>(m_header.type);
}

void Message::setSequence(uint32_t seq)
{
    m_header.sequence = seq;
}

uint32_t Message::sequence() const
{
    return m_header.sequence;
}

//消息体操作
void Message::setBody(const QByteArray& body)
{
    if (body.size() > MAX_BODY_SIZE) {
        qWarning() << "Message::setBody: body 超过上限，拒绝写入:"
                   << body.size() << ">=" << MAX_BODY_SIZE;
        return;
    }
    m_body = body;
    m_header.bodyLength = static_cast<uint32_t>(m_body.size());
}

QByteArray Message::body() const
{
    return m_body;
}

//序列化
QByteArray Message::serialize() const
{
    if (m_body.size() > MAX_BODY_SIZE) {
        qWarning() << "Message::serialize: body 超过上限，拒绝序列化:"
                   << m_body.size() << ">=" << MAX_BODY_SIZE;
        return QByteArray();
    }

    int totalSize = HEADER_SIZE + m_body.size();
    QByteArray data(totalSize, '\0');

    std::memcpy(data.data(), &m_header, HEADER_SIZE);   //先把消息头传入

    if (!m_body.isEmpty()) {
        std::memcpy(data.data() + HEADER_SIZE,     //跳过已经存储的消息头
                    m_body.constData(), 
                    m_body.size());
    }
    return data;
}

//反序列化
Message Message::deserialize(const QByteArray& data)
{
    if (data.size() < HEADER_SIZE) {
        qWarning() << "Message::deserialize: 数据太短，无法解析 header";
        return invalidMessage();
    }
    MessageHeader header;
    std::memcpy(&header, data.constData(), HEADER_SIZE);

    if (header.magic != PROTOCOL_MAGIC) {
        qWarning() << "Message::deserialize: 魔数校验失败，期望"
                   << QString::number(PROTOCOL_MAGIC, 16)
                   << "实际" << QString::number(header.magic, 16);
        return invalidMessage();
    }

    if (header.version != PROTOCOL_VERSION) {
        qWarning() << "Message::deserialize: 协议版本不匹配，期望"
                   << static_cast<int>(PROTOCOL_VERSION)
                   << "实际" << static_cast<int>(header.version);
        return invalidMessage();
    }

    if (header.bodyLength > MAX_BODY_SIZE) {
        qWarning() << "Message::deserialize: body 长度超限:"
                   << header.bodyLength;
        return invalidMessage();
    }

    if (data.size() < HEADER_SIZE + static_cast<int>(header.bodyLength)) {
        qWarning() << "Message::deserialize: 数据太短，无法解析 body"
                   << "需要" << HEADER_SIZE + header.bodyLength
                   << "实际" << data.size();
        return invalidMessage();
    }
    QByteArray body;
    if (header.bodyLength > 0) {
        body = data.mid(HEADER_SIZE, header.bodyLength);
    }
    
    Message msg;
    msg.m_header = header;      // 复制解析出的 header
    msg.m_body = body;          // 设置 body
    
    return msg;
}

bool Message::isValid() const
{
    return m_valid
        && m_header.magic == PROTOCOL_MAGIC
        && m_header.version == PROTOCOL_VERSION;
}

Message Message::invalidMessage()
{
    Message msg;
    msg.m_valid = false;
    return msg;
}

//JSON
void Message::setJsonBody(const QJsonObject& json)
{
    QJsonDocument doc(json);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    if (data.size() > MAX_BODY_SIZE) {
        qWarning() << "Message::setJsonBody: body 超过上限，拒绝写入:"
                   << data.size() << ">=" << MAX_BODY_SIZE;
        return;
    }
    m_body = data;
    m_header.bodyLength = static_cast<uint32_t>(m_body.size());
}

QJsonObject Message::jsonBody() const
{
    if (m_body.isEmpty()) {
        return QJsonObject();
    }
    QJsonDocument doc = QJsonDocument::fromJson(m_body);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "Message::jsonBody: JSON 解析失败或不是对象类型";
        return QJsonObject();
    }
    
    return doc.object();
}


