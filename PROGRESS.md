# IMSystem 开发进度报告

> 项目：Qt6 即时通讯系统
> 技术栈：Qt 6.11 + C++17 + SQLite + CMake
> 更新时间：2026-07-29

---

## 项目概述

构建可维护、可扩展的 C/S 架构即时通讯系统，支持用户注册登录、好友系统、一对一聊天、群聊、文件传输等功能。

---

## 已完成任务

### 2026-07-29

#### ✅ 9. 好友服务 - FriendService
- [x] 创建 server/service/friendservice.h（类声明）
- [x] 创建 server/service/friendservice.cpp（类实现）
- [x] 实现单例模式（instance()）
- [x] 实现添加好友申请（handleAddFriend）
  - 解析请求 JSON
  - 验证参数（好友用户名）
  - 检查用户是否存在
  - 检查是否已经是好友
  - 检查是否已发送过请求
  - 添加好友申请到数据库
  - 返回成功响应
- [x] 实现获取好友列表（handleFriendList）
  - 从数据库获取好友列表
  - 返回好友信息（ID、用户名、昵称、头像、在线状态）
- [x] 实现接受好友请求（handleAcceptFriend）
  - 更新好友状态为已接受
  - 添加反向好友关系
  - 返回成功响应
- [x] 实现拒绝好友请求（handleRejectFriend）
  - 更新好友状态为已拒绝
  - 返回成功响应
- [x] 实现删除好友（handleDeleteFriend）
  - 删除双向好友关系
  - 返回成功响应
- [x] 实现搜索用户（handleSearchUser）
  - 按用户名或昵称搜索
  - 排除当前用户
  - 返回搜索结果
- [x] 实现辅助方法（createResponse、sendErrorResponse、sendSuccessResponse）

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

#### ✅ 客户端连接处理扩展 - ClientHandler
- [x] 添加好友消息处理分支
  - REQ_ADD_FRIEND → FriendService::handleAddFriend
  - REQ_FRIEND_LIST → FriendService::handleFriendList
  - REQ_ACCEPT_FRIEND → FriendService::handleAcceptFriend
  - REQ_REJECT_FRIEND → FriendService::handleRejectFriend
  - REQ_DELETE_FRIEND → FriendService::handleDeleteFriend
  - REQ_SEARCH_USER → FriendService::handleSearchUser

#### ✅ 构建配置更新 - CMakeLists.txt
- [x] 添加 friendservice.h 和 friendservice.cpp 到服务端构建

### 2026-07-22

#### ✅ 8. 认证服务 - AuthService
- [x] 创建 server/service/authservice.h（类声明）
- [x] 创建 server/service/authservice.cpp（类实现）
- [x] 实现单例模式（instance()）
- [x] 实现注册处理（handleRegister）
- [x] 实现登录处理（handleLogin）
- [x] 实现辅助方法（createResponse、sendErrorResponse）
- [x] 更新 CMakeLists.txt（添加 authservice 和 tcpserver 等文件）

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
        └── friendservice.h/cpp     # ✅ 好友服务
```

---

## 下一步任务

### 待实现：客户端好友UI

**功能清单：**
1. **好友列表界面**
   - 显示好友列表（头像、昵称、在线状态）
   - 点击好友进入聊天界面
   
2. **好友搜索界面**
   - 搜索输入框
   - 搜索结果列表
   - 添加好友按钮
   
3. **好友请求界面**
   - 待处理的好友请求列表
   - 接受/拒绝按钮
   
4. **好友管理**
   - 删除好友功能
   - 好友信息查看

### 待实现：一对一聊天

**功能清单：**
1. **消息发送**
   - 文本消息输入
   - 消息发送到服务端
   
2. **消息接收**
   - 接收来自好友的消息
   - 显示在聊天窗口
   
3. **消息存储**
   - 服务端存储消息到数据库
   - 支持历史消息查询
   
4. **消息显示**
   - 气泡样式显示消息
   - 区分自己和对方的消息
   - 显示消息时间

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

---

## 面试要点备忘

1. **注册流程**：参数验证 → 用户名检查 → 盐值生成 → 密码加密 → 数据库存储
2. **登录流程**：用户查询 → 密码验证 → Token 生成 → 在线状态更新
3. **好友系统**：添加申请 → 接受/拒绝 → 双向关系建立
4. **密码安全**：SHA256 + 随机盐值，数据库不存明文密码
5. **单例模式**：使用 static 局部变量实现线程安全
6. **错误处理**：统一的错误响应格式

---

## 关键代码片段

### FriendService 使用示例
```cpp
// 在 ClientHandler::handleMessage() 中调用
case MessageType::REQ_ADD_FRIEND:
    FriendService::instance().handleAddFriend(this, msg);
    break;

case MessageType::REQ_FRIEND_LIST:
    FriendService::instance().handleFriendList(this, msg);
    break;

case MessageType::REQ_ACCEPT_FRIEND:
    FriendService::instance().handleAcceptFriend(this, msg);
    break;

case MessageType::REQ_REJECT_FRIEND:
    FriendService::instance().handleRejectFriend(this, msg);
    break;

case MessageType::REQ_DELETE_FRIEND:
    FriendService::instance().handleDeleteFriend(this, msg);
    break;

case MessageType::REQ_SEARCH_USER:
    FriendService::instance().handleSearchUser(this, msg);
    break;
```

### 好友请求/响应格式
```json
// 添加好友请求
{
    "type": 3001,
    "username": "lisi",
    "sequence": 3
}

// 添加好友响应
{
    "type": 3002,
    "success": true,
    "friend_username": "lisi",
    "message": "好友请求已发送",
    "sequence": 3
}

// 好友列表请求
{
    "type": 3003,
    "sequence": 4
}

// 好友列表响应
{
    "type": 3004,
    "success": true,
    "friends": [
        {
            "user_id": 10002,
            "username": "lisi",
            "nickname": "李四",
            "avatar": "",
            "status": 1
        }
    ],
    "count": 1,
    "sequence": 4
}

// 接受好友请求
{
    "type": 3006,
    "username": "zhangsan",
    "sequence": 5
}

// 接受好友响应
{
    "type": 3007,
    "success": true,
    "friend_username": "zhangsan",
    "message": "已接受好友请求",
    "sequence": 5
}

// 搜索用户请求
{
    "type": 3012,
    "keyword": "zhang",
    "sequence": 6
}

// 搜索用户响应
{
    "type": 3013,
    "success": true,
    "users": [
        {
            "user_id": 10001,
            "username": "zhangsan",
            "nickname": "张三",
            "avatar": "",
            "status": 1
        }
    ],
    "count": 1,
    "sequence": 6
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

### Q: 如何处理好友申请被拒绝？
A: 将friendships表中对应记录的status更新为2（已拒绝）。如果之后想重新添加，需要先删除旧记录再重新申请。

---

> 📝 每次完成功能后更新此报告，记录新完成的任务和相关技术决策
