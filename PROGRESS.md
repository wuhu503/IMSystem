# IMSystem 开发进度报告

> 项目：Qt6 即时通讯系统
> 技术栈：Qt 6.11 + C++17 + SQLite + CMake
> 更新时间：2026-07-30

---

## 项目概述

构建可维护、可扩展的 C/S 架构即时通讯系统，支持用户注册登录、好友系统、一对一聊天、群聊、文件传输等功能。

---

## 已完成任务

### 2026-07-30

#### ✅ 10. 聊天服务 - ChatService
- [x] 创建 server/service/chatservice.h（类声明）
- [x] 创建 server/service/chatservice.cpp（类实现）
- [x] 实现单例模式（instance()）
- [x] 实现文本消息处理（handleTextMessage）
  - 解析请求 JSON（接收者、消息内容）
  - 验证参数
  - 生成消息ID
  - 存储消息到数据库
  - 返回消息确认
  - 转发消息给接收者
- [x] 实现历史消息请求（handleHistoryRequest）
  - 解析请求 JSON（好友用户名、分页参数）
  - 从数据库查询历史消息
  - 返回消息列表
- [x] 实现消息确认处理（handleMessageAck）
- [x] 实现辅助方法（createResponse、sendErrorResponse、sendSuccessResponse）

#### ✅ 数据库层扩展 - DbManager
- [x] 添加消息相关数据库操作方法
  - saveMessage() - 保存消息到数据库
  - getChatHistory() - 获取聊天历史
  - markMessageAsRead() - 标记消息已读
  - getUnreadMessageCount() - 获取未读消息数量

#### ✅ 客户端聊天功能 - MainWindow
- [x] 实现消息发送功能
  - 输入消息内容
  - 发送消息到服务端
  - 本地显示消息
- [x] 实现消息接收功能
  - 接收来自好友的消息
  - 显示在聊天窗口
- [x] 实现历史消息加载
  - 点击好友时请求历史消息
  - 显示历史消息记录
- [x] 实现消息确认处理
  - 处理消息发送失败的情况

### 2026-07-29

#### ✅ 9. 好友服务 - FriendService
- [x] 创建 server/service/friendservice.h（类声明）
- [x] 创建 server/service/friendservice.cpp（类实现）
- [x] 实现单例模式（instance()）
- [x] 实现添加好友申请（handleAddFriend）
- [x] 实现获取好友列表（handleFriendList）
- [x] 实现接受好友请求（handleAcceptFriend）
- [x] 实现拒绝好友请求（handleRejectFriend）
- [x] 实现删除好友（handleDeleteFriend）
- [x] 实现搜索用户（handleSearchUser）

#### ✅ 数据库层扩展 - DbManager
- [x] 添加好友相关数据库操作方法
  - addFriendRequest() - 添加好友申请
  - acceptFriendRequest() - 接受好友请求
  - rejectFriendRequest() - 拒绝好友请求
  - deleteFriend() - 删除好友
  - isFriend() - 检查是否是好友
  - hasPendingFriendRequest() - 检查是否有待处理的好友请求
  - getFriendList() - 获取好友列表
  - getPendingFriendRequests() - 获取待处理的好友请求
  - searchUsers() - 搜索用户

#### ✅ 协议扩展 - protocol.h
- [x] 添加新的消息类型枚举
  - REQ_ACCEPT_FRIEND = 3006  // 接受好友请求
  - RSP_ACCEPT_FRIEND = 3007  // 接受好友响应
  - REQ_REJECT_FRIEND = 3008  // 拒绝好友请求
  - RSP_REJECT_FRIEND = 3009  // 拒绝好友响应
  - REQ_DELETE_FRIEND = 3010  // 删除好友请求
  - RSP_DELETE_FRIEND = 3011  // 删除好友响应
  - REQ_SEARCH_USER = 3012    // 搜索用户请求
  - RSP_SEARCH_USER = 3013    // 搜索用户响应

#### ✅ 客户端好友UI - MainWindow
- [x] 添加好友操作按钮（添加好友、好友请求、刷新、删除好友）
- [x] 实现好友列表显示（头像、昵称、在线状态）
- [x] 实现添加好友对话框
- [x] 实现删除好友功能
- [x] 连接TcpClient信号，处理好友相关响应消息

### 2026-07-22

#### ✅ 8. 认证服务 - AuthService
- [x] 创建 server/service/authservice.h（类声明）
- [x] 创建 server/service/authservice.cpp（类实现）
- [x] 实现单例模式（instance()）
- [x] 实现注册处理（handleRegister）
- [x] 实现登录处理（handleLogin）
- [x] 实现辅助方法（createResponse、sendErrorResponse）

### 2026-07-21

#### ✅ 7. TCP 服务器 - TcpServer
- [x] 创建 server/tcpserver.h（类声明）
- [x] 创建 server/tcpserver.cpp（类实现）
- [x] 继承 QTcpServer，重写 incomingConnection()
- [x] 实现启动/停止服务器
- [x] 实现接受新连接
- [x] 实现客户端断开处理

#### ✅ 6. 客户端连接处理 - ClientHandler
- [x] 创建 server/clienthandler.h（类声明）
- [x] 创建 server/clienthandler.cpp（类实现）
- [x] 实现粘包处理（onReadyRead）
- [x] 实现消息分发（handleMessage）
- [x] 实现发送消息（sendMessage）

### 2026-07-20

#### ✅ 5. 数据库层 - DbManager（单例模式）
- [x] 创建 server/database/ 目录结构
- [x] 编写 init.sql 建表脚本
- [x] 实现 DbManager 类（单例模式）

#### ✅ 4. 公共模块 - utils.h / utils.cpp（工具函数）
- [x] 实现密码加密（SHA256 + 盐值）
- [x] 实现时间工具
- [x] 实现 UUID 生成

#### ✅ 3. 公共模块 - message.h / message.cpp（消息封装类）
- [x] 实现序列化 serialize()
- [x] 实现反序列化 deserialize()

#### ✅ 2. 公共模块 - protocol.h（协议定义）
- [x] 定义消息类型枚举 MessageType
- [x] 定义消息头结构 MessageHeader

#### ✅ 1. 项目初始化
- [x] 创建 Qt Widgets 项目模板
- [x] 配置 CMakeLists.txt
- [x] 初始化 Git 仓库

---

## 当前文件结构

```
IMSystem/
├── CMakeLists.txt
├── main.cpp                        # 客户端入口
├── mainwindow.h/cpp/ui             # 客户端主窗口
├── PROGRESS.md
├── SUMMARY.md
│
├── common/                         # ✅ 公共模块（已完成）
│   ├── protocol.h                  # 协议定义
│   ├── message.h/cpp               # 消息封装
│   └── utils.h/cpp                 # 工具函数
│
└── server/                         # ✅ 服务端（已完成）
    ├── main.cpp                    # 服务端入口
    ├── tcpserver.h/cpp             # ✅ TCP服务器
    ├── clienthandler.h/cpp         # ✅ 客户端连接处理
    ├── database/
    │   ├── init.sql                # 建表脚本
    │   └── dbmanager.h/cpp         # ✅ 数据库管理
    └── service/
        ├── authservice.h/cpp       # ✅ 认证服务
        ├── friendservice.h/cpp     # ✅ 好友服务
        └── chatservice.h/cpp       # ✅ 聊天服务
```

---

## 下一步任务

### 待实现：群聊系统

**功能清单：**
1. **创建群组**
   - 输入群组名称
   - 选择群成员
   - 创建群组
   
2. **加入群组**
   - 搜索群组
   - 申请加入群组
   
3. **群消息收发**
   - 发送群消息
   - 接收群消息
   - 群消息存储

### 待实现：消息可靠性

**功能清单：**
1. **离线消息推送**
   - 用户上线时推送离线消息
   - 消息状态同步
   
2. **消息去重**
   - 基于消息ID去重
   - 防止重复显示

### 待实现：心跳机制

**功能清单：**
1. **客户端心跳**
   - 定期发送心跳包
   - 检测连接状态
   
2. **服务端超时断开**
   - 检测客户端心跳超时
   - 自动断开连接

---

## 技术决策记录

| 决策 | 选择 | 原因 |
|------|------|------|
| 消息格式 | 16字节固定头 + JSON体 | 固定头便于快速解析，JSON灵活可扩展 |
| 数据库 | SQLite | 轻量级，无需额外服务器 |
| 数据库访问 | 单例模式 DbManager | 全局唯一连接 |
| 粘包处理 | 缓冲区 + 长度前缀 | 可靠的消息边界划分 |
| 连接管理 | ClientHandler per connection | 职责清晰，易于扩展 |
| 服务器设计 | 继承 QTcpServer | 利用 Qt 框架，减少代码量 |
| 密码加密 | SHA256 + 随机盐值 | 安全性高，防止彩虹表攻击 |
| Token 生成 | UUID | 全局唯一，足够随机 |
| 好友关系 | 双向记录 | 查询方便，状态一致 |
| 好友状态 | 0待确认/1已接受/2已拒绝 | 清晰的状态机 |
| 消息存储 | 消息表存储所有消息 | 便于历史查询和统计 |

---

## 面试要点备忘

1. **注册流程**：参数验证 → 用户名检查 → 盐值生成 → 密码加密 → 数据库存储
2. **登录流程**：用户查询 → 密码验证 → Token 生成 → 在线状态更新
3. **好友系统**：添加申请 → 接受/拒绝 → 双向关系建立
4. **聊天系统**：消息发送 → 服务端存储 → 消息转发 → 消息确认
5. **密码安全**：SHA256 + 随机盐值，数据库不存明文密码
6. **单例模式**：使用 static 局部变量实现线程安全
7. **错误处理**：统一的错误响应格式

---

## 关键代码片段

### ChatService 使用示例
```cpp
// 在 ClientHandler::handleMessage() 中调用
case MessageType::MSG_TEXT:
    ChatService::instance().handleTextMessage(this, msg);
    break;

case MessageType::MSG_HISTORY:
    ChatService::instance().handleHistoryRequest(this, msg);
    break;
```

### 聊天消息格式
```json
// 发送文本消息
{
    "type": 4001,
    "receiver": "lisi",
    "content": "你好！",
    "sequence": 7
}

// 消息确认
{
    "type": 4004,
    "success": true,
    "msg_id": "550e8400-e29b-41d4-a716-446655440000",
    "sequence": 7
}

// 历史消息请求
{
    "type": 4005,
    "username": "lisi",
    "limit": 50,
    "offset": 0,
    "sequence": 8
}

// 历史消息响应
{
    "type": 4005,
    "success": true,
    "messages": [
        {
            "msg_id": "550e8400-e29b-41d4-a716-446655440000",
            "sender_id": 10001,
            "receiver_id": 10002,
            "sender_name": "zhangsan",
            "receiver_name": "lisi",
            "type": 4001,
            "content": "你好！",
            "timestamp": 1690000000,
            "is_read": 1
        }
    ],
    "count": 1,
    "friend_username": "lisi",
    "sequence": 8
}
```

---

## 常见问题

### Q: 为什么登录失败不明确提示"用户名不存在"或"密码错误"？
A: 安全考虑。如果明确提示，攻击者可以枚举有效的用户名。统一提示"用户名或密码错误"更安全。

### Q: Token 应该存储在哪里？
A: 当前实现使用内存存储（ClientHandler 的 userId）。生产环境应该使用 Redis 或数据库存储 Token，并设置过期时间。

### Q: 如何防止暴力破解？
A: 可以添加登录失败次数限制，例如连续失败5次后锁定账号15分钟。

### Q: 好友关系为什么是双向记录？
A: 查询方便。当用户A添加用户B为好友后，需要在friendships表中插入两条记录：
- A->B（状态：已接受）
- B->A（状态：已接受）
这样查询"谁是我的好友"时只需要单表查询。

### Q: 消息如何保证送达？
A: 当前实现使用消息确认机制（ACK）。发送消息后，服务端返回确认。如果确认失败，客户端可以重试。未来可以添加离线消息推送功能。

---

> 📝 每次完成功能后更新此报告，记录新完成的任务和相关技术决策
