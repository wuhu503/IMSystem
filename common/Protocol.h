#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>

// 协议常量
constexpr uint32_t PROTOCOL_MAGIC   = 0x494D5359;  // "IMSY"
constexpr uint8_t  PROTOCOL_VERSION = 1;
constexpr int      HEADER_SIZE      = 16;
constexpr int      MAX_BODY_SIZE    = 10 * 1024 * 1024;

// 消息类型枚举
enum class MessageType : uint16_t {
    // 用户系统 (1xxx)
    REQ_REGISTER        = 1000,
    RSP_REGISTER        = 1001,
    REQ_LOGIN           = 1002,
    RSP_LOGIN           = 1003,
    REQ_LOGOUT          = 1004,
    RSP_LOGOUT          = 1005,

    // 好友系统 (3xxx)
    REQ_ADD_FRIEND      = 3001,
    RSP_ADD_FRIEND      = 3002,
    REQ_FRIEND_LIST     = 3003,
    RSP_FRIEND_LIST     = 3004,
    REQ_ACCEPT_FRIEND   = 3006,
    RSP_ACCEPT_FRIEND   = 3007,
    REQ_REJECT_FRIEND   = 3008,
    RSP_REJECT_FRIEND   = 3009,
    REQ_DELETE_FRIEND   = 3010,
    RSP_DELETE_FRIEND   = 3011,
    REQ_SEARCH_USER     = 3012,
    RSP_SEARCH_USER     = 3013,
    REQ_PENDING_REQUESTS = 3014,
    RSP_PENDING_REQUESTS = 3015,

    // 服务端主动推送（单向通知，无对应 RSP）
    NTF_FRIEND_REQUEST  = 3016,  // 收到新的好友请求
    NTF_FRIEND_ACCEPTED = 3017,  // 自己发出的好友请求被接受
    NTF_FRIEND_REJECTED = 3018,  // 自己发出的好友请求被拒绝
    NTF_FRIEND_REMOVED  = 3019,  // 对方解除了好友关系

    // 聊天消息 (4xxx)：请求与响应使用不同类型，避免“同码既是请求又是响应”
    MSG_TEXT            = 4001,  // C->S 发送文本消息
    MSG_IMAGE           = 4002,  // 预留：图片消息
    MSG_FILE            = 4003,  // 预留：文件消息
    RSP_TEXT            = 4004,  // S->C 文本发送结果（成功时带 msg_id）
    MSG_ACK             = 4005,  // C->S 已读回执（携带 username）
    REQ_HISTORY         = 4006,  // C->S 请求聊天记录
    RSP_HISTORY         = 4007,  // S->C 聊天记录响应

    // 群聊系统 (5xxx)
    REQ_CREATE_GROUP    = 5001,
    RSP_CREATE_GROUP    = 5002,
    REQ_JOIN_GROUP      = 5003,
    RSP_JOIN_GROUP      = 5004,
    REQ_GROUP_LIST      = 5005,
    RSP_GROUP_LIST      = 5006,
    REQ_GROUP_MEMBERS   = 5007,
    RSP_GROUP_MEMBERS   = 5008,
    MSG_GROUP_TEXT      = 5009,
    MSG_GROUP_IMAGE     = 5010,
    MSG_GROUP_FILE      = 5011,

    // 系统 (9xxx)
    HEARTBEAT           = 9001,
};

// 消息头结构(16字节)
#pragma pack(push, 1)

struct MessageHeader {
    uint32_t magic;      // 4 bytes
    uint8_t  version;    // 1 byte
    uint16_t type;       // 2 bytes
    uint8_t  reserved;   // 1 byte
    uint32_t bodyLength; // 4 bytes
    uint32_t sequence;   // 4 bytes
};

#pragma pack(pop)

static_assert(sizeof(MessageHeader) == 16, "MessageHeader must be 16 bytes");

#endif // PROTOCOL_H
