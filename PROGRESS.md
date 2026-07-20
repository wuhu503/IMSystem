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

#### ✅ 4. 公共模块 - utils.h / utils.cpp（工具函数）
- [x] 创建 Utils 命名空间
- [x] 实现密码加密功能
  - `generateSalt(length)` — 生成随机盐值（十六进制格式）
  - `hashPassword(password, salt)` — SHA256 哈希
  - `verifyPassword(password, salt, storedHash)` — 验证密码
- [x] 实现时间工具
  - `currentTimestamp()` — 秒级时间戳
  - `currentTimestampMs()` — 毫秒级时间戳
  - `formatTimestamp(timestamp, format)` — 格式化时间戳
  - `currentDateTime(format)` — 当前时间字符串
- [x] 实现 UUID 生成
  - `generateUUID()` — 带连字符的 UUID
  - `generateUUIDWithoutHyphen()` — 不带连字符的 UUID
- [x] 更新 CMakeLists.txt（添加 utils.h/cpp，修复语法错误）

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

### 2026-07-19

#### ✅ 2. 公共模块 - protocol.h（协议定义）
- [x] 创建 common/ 目录结构
- [x] 定义协议常量
  - `PROTOCOL_MAGIC = 0x494D5359`（"IMSY"）
  - `PROTOCOL_VERSION = 1`
  - `HEADER_SIZE = 16` 字节
  - `MAX_BODY_SIZE = 10MB`
- [x] 定义消息类型枚举 `MessageType`
- [x] 定义消息头结构 `MessageHeader`（16字节，#pragma pack）
- [x] 添加 static_assert 编译时校验

#### ✅ 1. 项目初始化
- [x] 创建 Qt Widgets 项目模板
- [x] 配置 CMakeLists.txt（Qt 6.11 + C++17）
- [x] 初始化 Git 仓库
- [x] 配置 .gitignore

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
    ├── message.cpp              # ✅ 消息类实现（已完成）
    ├── utils.h                 # ✅ 工具函数声明（已完成）
    └── utils.cpp               # ✅ 工具函数实现（已完成）
```

---

## 下一步任务

### 待实现：server/ 目录结构 + server.h / server.cpp（TCP服务器）

**功能清单：**
1. **创建 server/ 目录**
2. **实现 TcpServer 类**
   - `startServer(port)` — 启动服务器监听
   - `stopServer()` — 停止服务器
   - 新客户端连接信号 `newConnection()`
   - 客户端断开信号 `clientDisconnected()`
3. **实现 ClientHandler 类**
   - 处理单个客户端连接
   - 读取数据并解析消息
   - 发送消息给客户端

---

## 技术决策记录

| 决策 | 选择 | 原因 |
|------|------|------|
| 消息格式 | 16字节固定头 + JSON体 | 固定头便于快速解析，JSON灵活可扩展 |
| 枚举类型 | enum class (uint16_t) | 类型安全，底层类型固定2字节 |
| 消息头对齐 | #pragma pack(push,1) | 避免编译器填充，保证16字节 |
| JSON格式 | Compact格式 | 网络传输节省带宽 |
| 反序列化校验 | 魔数 + 长度双重校验 | 保证数据完整性和协议匹配 |
| 密码加密 | SHA256 + 随机盐值 | 安全性高，防止彩虹表攻击 |
| 工具函数 | 命名空间 Utils | 避免全局命名污染，便于使用 |

---

## 面试要点备忘

1. **自定义协议设计**：16字节固定头 + JSON 消息体，含版本号支持升级
2. **消息头结构**：magic校验 + version + type + bodyLength + sequence
3. **static_assert**：编译时保证结构体大小正确
4. **序列化/反序列化**：使用 memcpy 进行二进制数据复制
5. **JSON处理**：QJsonDocument 进行 JSON 与 QByteArray 互转
6. **数据校验**：魔数校验 + 长度校验，防止非法数据
7. **密码安全**：SHA256 + 盐值，防止彩虹表攻击
8. **UUID生成**：使用 QUuid 生成唯一标识符

---

## 关键代码片段

### 密码加密示例
```cpp
// 注册时：生成盐值并加密密码
QString salt = Utils::generateSalt(16);
QString hashedPassword = Utils::hashPassword(password, salt);

// 存储到数据库
db->insertUser(username, hashedPassword, salt);

// 登录时：验证密码
if (Utils::verifyPassword(inputPassword, storedSalt, storedHash)) {
    // 密码正确
}
```

### 时间戳使用示例
```cpp
// 消息发送时
QJsonObject body;
body["timestamp"] = Utils::currentTimestamp();
body["content"] = "你好！";

// 显示时间
QString timeStr = Utils::formatTimestamp(timestamp);
```

### UUID 使用示例
```cpp
// 生成消息ID
QString msgId = Utils::generateUUID();

// 生成会话ID
QString sessionId = Utils::generateUUIDWithoutHyphen();
```

---

## 常见问题

### Q: 为什么使用 SHA256 而不是 MD5？
A: SHA256 比 MD5 更安全，MD5 已经被证明存在碰撞漏洞。在密码存储场景中，安全性是首要考虑。

### Q: 盐值应该存储在哪里？
A: 盐值应该和哈希后的密码一起存储在数据库中。每个用户应该有独立的盐值。

### Q: UUID 会重复吗？
A: UUID 设计上是全球唯一的，重复概率极低（约 2^128 分之一）。在实际应用中可以认为不会重复。

---

> 📝 每次完成功能后更新此报告，记录新完成的任务和相关技术决策
