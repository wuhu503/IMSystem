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

#### ✅ 5. 数据库层 - DbManager（单例模式）
- [x] 创建 server/database/ 目录结构
- [x] 编写 init.sql 建表脚本
  - users 表：存储用户信息（id, username, password_hash, salt, nickname, avatar, status）
  - friendships 表：存储好友关系（user_id, friend_id, status）
  - messages 表：存储聊天消息（msg_id, sender_id, receiver_id, type, content）
  - 索引优化：username, user_id, timestamp 等字段
- [x] 实现 DbManager 类（单例模式）
  - `instance()` — 获取单例实例（线程安全）
  - `init(dbPath)` — 初始化数据库连接
  - `createTables()` — 执行建表脚本
  - `insertUser()` — 插入新用户（注册）
  - `verifyUser()` — 验证用户密码（登录）
  - `getUserId()` — 获取用户ID
  - `getUserInfo()` — 获取用户信息
  - `isUsernameExists()` — 检查用户名是否存在
  - `updateUserStatus()` — 更新用户在线状态
- [x] 更新 CMakeLists.txt
  - 添加 Qt::Sql 模块
  - 分离服务端和客户端构建目标
  - 添加 init.sql 复制命令

#### ✅ 4. 公共模块 - utils.h / utils.cpp（工具函数）
- [x] 实现密码加密（SHA256 + 盐值）
- [x] 实现时间工具
- [x] 实现 UUID 生成

#### ✅ 3. 公共模块 - message.h / message.cpp（消息封装类）
- [x] 实现序列化 serialize()
- [x] 实现反序列化 deserialize()
- [x] 实现 JSON 便捷方法

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
│
├── common/                         # ✅ 公共模块（已完成）
│   ├── protocol.h                  # 协议定义
│   ├── message.h/cpp               # 消息封装
│   └── utils.h/cpp                 # 工具函数
│
└── server/                         # 服务端（开发中）
    └── database/
        ├── init.sql                # ✅ 建表脚本（已完成）
        ├── dbmanager.h             # ✅ 数据库管理类声明（已完成）
        └── dbmanager.cpp           # ✅ 数据库管理类实现（已完成）
```

---

## 下一步任务

### 待实现：server/clienthandler.h + clienthandler.cpp（客户端连接处理）

**功能清单：**
1. **ClientHandler 类**
   - 管理单个客户端的 socket 连接
   - 读取数据并解析消息（处理粘包/拆包）
   - 发送消息给客户端
   - 处理客户端断开连接

2. **消息处理流程**
   ```
   TCP接收 → 粘包处理 → Message::deserialize() → handleMessage() → 业务处理
   ```

3. **关键实现点**
   - 使用 QByteArray 缓冲区处理粘包
   - 先读取 16 字节 header，再读取 body
   - 使用信号槽通知上层模块

---

## 技术决策记录

| 决策 | 选择 | 原因 |
|------|------|------|
| 消息格式 | 16字节固定头 + JSON体 | 固定头便于快速解析，JSON灵活可扩展 |
| 数据库 | SQLite | 轻量级，无需额外服务器，适合单机应用 |
| 数据库访问 | 单例模式 DbManager | 全局唯一连接，避免连接泄漏 |
| SQL防护 | 参数化查询 bindValue | 防止 SQL 注入攻击 |
| 项目结构 | 服务端/客户端分离 | 独立部署，职责清晰 |

---

## 面试要点备忘

1. **单例模式**：使用 static 局部变量实现线程安全的单例
2. **SQL 注入防护**：使用 QSqlQuery::bindValue() 参数化查询
3. **数据库设计**：用户表、好友表、消息表，使用外键约束
4. **索引优化**：在常用查询字段上建立索引
5. **SQLite 特点**：嵌入式数据库，无需服务器进程，数据存储在单个文件

---

## 关键代码片段

### DbManager 使用示例
```cpp
// 初始化数据库
DbManager::instance().init("imsystem.db");

// 注册用户
QString salt = Utils::generateSalt();
QString hash = Utils::hashPassword(password, salt);
bool success = DbManager::instance().insertUser(username, hash, salt);

// 登录验证
QString hash = Utils::hashPassword(inputPassword, storedSalt);
if (DbManager::instance().verifyUser(username, hash)) {
    // 登录成功
    qint64 userId = DbManager::instance().getUserId(username);
}
```

---

## 常见问题

### Q: 为什么使用单例模式？
A: 数据库连接是昂贵的资源，单例保证全局只有一个连接，避免资源浪费和连接泄漏。

### Q: SQLite 支持并发吗？
A: SQLite 支持多读者单写者模型。对于小型 IM 系统足够使用，如果需要高并发，可以考虑 MySQL。

### Q: 如何防止 SQL 注入？
A: 使用参数化查询（bindValue），不要使用字符串拼接 SQL。

---

> 📝 每次完成功能后更新此报告，记录新完成的任务和相关技术决策
