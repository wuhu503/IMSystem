# IMSystem 开发进度报告

> 项目：Qt6 即时通讯系统
> 技术栈：Qt 6.11 + C++17 + SQLite + CMake
> 更新时间：2026-07-20

---

## 项目概述

构建可维护、可扩展的 C/S 架构即时通讯系统，支持用户注册登录、好友系统、一对一聊天、群聊、文件传输等功能。

---

## 已完成任务

### 2026-07-20

#### ✅ 3. 公共模块 - message.h / message.cpp（消息封装类）
- [x] 创建 Message 类声明（message.h）
- [x] 实现构造函数（默认构造 + 带类型构造）
- [x] 实现消息头字段操作（type, sequence）
- [x] 实现消息体操作（setBody, body）
- [x] 实现序列化 serialize()
  - 打包格式：16字节消息头 + 消息体
  - 使用 memcpy 复制二进制数据
- [x] 实现反序列化 deserialize()
  - 校验数据长度 >= 16字节
  - 校验魔数 PROTOCOL_MAGIC
  - 校验数据完整性（header + body）
- [x] 实现 JSON 便捷方法
  - setJsonBody(): QJsonObject → QByteArray（Compact格式）
  - jsonBody(): QByteArray → QJsonObject
- [x] 更新 CMakeLists.txt（添加 protocol.h 和 include 路径）

### 2026-07-19

#### ✅ 1. 项目初始化
- [x] 创建 Qt Widgets 项目模板
- [x] 配置 CMakeLists.txt（Qt 6.11 + C++17）
- [x] 初始化 Git 仓库
- [x] 配置 .gitignore

#### ✅ 2. 公共模块 - protocol.h（协议定义）
- [x] 创建 common/ 目录结构
- [x] 定义协议常量
  - `PROTOCOL_MAGIC = 0x494D5359`（"IMSY"）
  - `PROTOCOL_VERSION = 1`
  - `HEADER_SIZE = 16` 字节
  - `MAX_BODY_SIZE = 10MB`
- [x] 定义消息类型枚举 `MessageType`
  - 用户系统：REQ/RSP_REGISTER, REQ/RSP_LOGIN, REQ/RSP_LOGOUT
  - 好友系统：REQ/RSP_ADD_FRIEND, REQ/RSP_FRIEND_LIST, NTF_FRIEND_STATUS
  - 聊天消息：MSG_TEXT/IMAGE/FILE, MSG_ACK, MSG_HISTORY
  - 群聊系统：REQ/RSP_CREATE_GROUP, REQ/RSP_JOIN_GROUP, MSG_GROUP_TEXT/IMAGE/FILE
  - 系统：HEARTBEAT
- [x] 定义消息头结构 `MessageHeader`（16字节，#pragma pack）
- [x] 添加 static_assert 编译时校验

---

## 当前文件结构

```
IMSystem/
├── CMakeLists.txt              # 顶层构建配置
├── .gitignore                  # Git 忽略规则
├── PROGRESS.md                 # 开发进度报告
├── main.cpp                    # 程序入口
├── mainwindow.h/cpp/ui         # 主窗口（模板）
└── common/
    ├── protocol.h              # ✅ 协议定义（已完成）
    ├── message.h               # ✅ 消息类声明（已完成）
    └── message.cpp             # ✅ 消息类实现（已完成）
```

---

## 下一步任务

### 待实现：common/utils.h + utils.cpp（工具函数）

**功能清单：**
1. **密码加密**
   - `hashPassword(const QString& password, const QString& salt)` — SHA256加密
   - `generateSalt()` — 生成随机盐值（16字节）

2. **时间工具**
   - `currentTimestamp()` — 获取当前时间戳（秒）
   - `currentTimestampMs()` — 获取当前时间戳（毫秒）
   - `formatTimestamp(qint64 timestamp)` — 格式化时间戳为可读字符串

3. **UUID生成**
   - `generateUUID()` — 生成唯一消息ID

---

## 技术决策记录

| 决策 | 选择 | 原因 |
|------|------|------|
| 消息格式 | 16字节固定头 + JSON体 | 固定头便于快速解析，JSON灵活可扩展 |
| 枚举类型 | enum class (uint16_t) | 类型安全，底层类型固定2字节 |
| 消息头对齐 | #pragma pack(push,1) | 避免编译器填充，保证16字节 |
| JSON格式 | Compact格式 | 网络传输节省带宽 |
| 反序列化校验 | 魔数 + 长度双重校验 | 保证数据完整性和协议匹配 |

---

## 面试要点备忘

1. **自定义协议设计**：16字节固定头 + JSON 消息体，含版本号支持升级
2. **消息头结构**：magic校验 + version + type + bodyLength + sequence
3. **static_assert**：编译时保证结构体大小正确
4. **序列化/反序列化**：使用 memcpy 进行二进制数据复制
5. **JSON处理**：QJsonDocument 进行 JSON 与 QByteArray 互转
6. **数据校验**：魔数校验 + 长度校验，防止非法数据

---

## 关键代码片段

### Message 序列化示例
```cpp
// 发送消息
Message msg(MessageType::MSG_TEXT);
msg.setSequence(1);
QJsonObject body;
body["sender_id"] = 10001;
body["receiver_id"] = 10002;
body["content"] = "你好！";
msg.setJsonBody(body);

// 序列化后发送
QByteArray data = msg.serialize();
socket->write(data);
```

### Message 反序列化示例
```cpp
// 接收数据
QByteArray data = socket->readAll();

// 反序列化
Message msg = Message::deserialize(data);

// 使用消息
if (msg.type() == MessageType::MSG_TEXT) {
    QJsonObject body = msg.jsonBody();
    QString content = body["content"].toString();
}
```

---

> 📝 每次完成功能后更新此报告，记录新完成的任务和相关技术决策
