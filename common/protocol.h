#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>

//协议常量

constexpr uint32_t PROTOCOL_MAGIC   = 0x494D5359;  // "IMSY" 的十六进制
constexpr uint8_t  PROTOCOL_VERSION = 1;
constexpr int      HEADER_SIZE      = 16;           // 固定消息头大小
constexpr int      MAX_BODY_SIZE    = 10 * 1024 * 1024;  // 消息体最大 10MB

//消息类型枚举

enum class MessageType : uint16_t {
    // 用户系统 (1xxx)
    REQ_REGISTER        = 1000,  // 注册请求
    RSP_REGISTER        = 1001,  // 注册响应
    REQ_LOGIN           = 1002,  // 登录请求
    RSP_LOGIN           = 1003,  // 登录响应
    REQ_LOGOUT          = 1004,  // 登出请求
    RSP_LOGOUT          = 1005,  // 登出响应

    // 好友系统 (3xxx)
    REQ_ADD_FRIEND      = 3001,  // 添加好友
    RSP_ADD_FRIEND      = 3002,  // 好友响应
    REQ_FRIEND_LIST     = 3003,  // 好友列表
    RSP_FRIEND_LIST     = 3004,  // 好友列表响应
    NTF_FRIEND_STATUS   = 3005,  // 好友上线/下线通知

    // 聊天消息 (4xxx)
    MSG_TEXT             = 4001,  // 文本消息
    MSG_IMAGE            = 4002,  // 图片消息
    MSG_FILE             = 4003,  // 文件消息
    MSG_ACK              = 4004,  // 消息确认
    MSG_HISTORY          = 4005,  // 历史消息

    // 群聊系统 (5xxx)
    REQ_CREATE_GROUP    = 5001,  // 创建群组
    RSP_CREATE_GROUP    = 5002,  // 创建群组响应
    REQ_JOIN_GROUP      = 5003,  // 加入群组
    RSP_JOIN_GROUP      = 5004,  // 加入群组响应
    REQ_GROUP_LIST      = 5005,  // 群组列表
    RSP_GROUP_LIST      = 5006,  // 群组列表响应
    REQ_GROUP_MEMBERS   = 5007,  // 群成员列表
    RSP_GROUP_MEMBERS   = 5008,  // 群成员列表响应
    MSG_GROUP_TEXT      = 5009,  // 群文本消息
    MSG_GROUP_IMAGE     = 5010,  // 群图片消息
    MSG_GROUP_FILE      = 5011,  // 群文件消息

    // 系统 (9xxx)
    HEARTBEAT           = 9001,  // 心跳包
};

//消息头结构(16字节)

#pragma pack(push, 1)  // 按 1 字节对齐，避免编译器填充

struct MessageHeader {
    uint32_t magic;      // 4 bytes - 魔数校验
    uint8_t  version;    // 1 byte  - 协议版本
    uint16_t type;       // 2 bytes - 消息类型
    uint8_t  reserved;   // 1 byte  - 保留字段
    uint32_t bodyLength; // 4 bytes - 消息体长度
    uint32_t sequence;   // 4 bytes - 序列号
};

#pragma pack(pop)  // 恢复默认对齐

static_assert(sizeof(MessageHeader) == 16, "MessageHeader must be 16 bytes");

#endif // PROTOCOL_H
