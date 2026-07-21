# IMSystem 开发进度报告

> 项目：Qt6 即时通讯系统
> 技术栈：Qt 6.11 + C++17 + SQLite + CMake
> 更新时间：2026-07-21

---

## 项目概述

构建可维护、可扩展的 C/S 架构即时通讯系统，支持用户注册登录、好友系统、一对一聊天、群聊、文件传输等功能。

---

## 已完成任务

### 2026-07-21

#### ✅ 7. TCP 服务器 - TcpServer
- [x] 创建 server/tcpserver.h（类声明）
- [x] 创建 server/tcpserver.cpp（类实现）
- [x] 继承 QTcpServer，重写 incomingConnection()
- [x] 实现启动服务器（startServer）
- [x] 实现停止服务器（stopServer）
- [x] 实现接受新连接
  - 创建 QTcpSocket
  - 创建 ClientHandler
  - 连接断开信号
  - 存储到 QMap 映射
- [x] 实现客户端断开处理
  - 从映射中移除
  - 销毁 ClientHandler
- [x] 实现清理所有连接（clearAllClients）

#### ✅ 6. 客户端连接处理 - ClientHandler
- [x] 创建 server/clienthandler.h（类声明）
- [x] 创建 server/clienthandler.cpp（类实现）
- [x] 实现粘包处理（onReadyRead）
- [x] 实现消息分发（handleMessage）
- [x] 实现发送消息（sendMessage）
- [x] 实现用户ID管理

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
│
├── common/                         # ✅ 公共模块（已完成）
│   ├── protocol.h                  # 协议定义
│   ├── message.h/cpp               # 消息封装
│   └── utils.h/cpp                 # 工具函数
│
└── server/                         # 服务端（开发中）
    ├── main.cpp                    # 服务端入口（待实现）
    ├── tcpserver.h/cpp             # ✅ TCP服务器（已完成）
    ├── clienthandler.h/cpp         # ✅ 客户端连接处理（已完成）
    ├── database/
    │   ├── init.sql                # 建表脚本
    │   └── dbmanager.h/cpp         # 数据库管理
    └── service/
        └── authservice.h/cpp       # 认证服务（待实现）
```

**注意**：`server.h` 和 `server.cpp` 是空文件，可以手动删除。

---

## 下一步任务

### 待实现：server/service/authservice.h + authservice.cpp（认证服务）

**功能清单：**
1. **AuthService 类**（单例模式）
   - `handleRegister(client, msg)` — 处理注册请求
   - `handleLogin(client, msg)` — 处理登录请求

2. **注册流程**
   ```
   解析请求 → 检查用户名 → 生成盐值 → 加密密码 → 存入数据库 → 返回响应
   ```

3. **登录流程**
   ```
   解析请求 → 查询用户 → 验证密码 → 生成Token → 返回响应
   ```

4. **消息格式**
   - 注册请求：`{username, password}`
   - 注册响应：`{success, message, user_id}`
   - 登录请求：`{username, password}`
   - 登录响应：`{success, token, user_id}`

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

---

## 面试要点备忘

1. **TcpServer 设计**：继承 QTcpServer，重写 incomingConnection()
2. **连接管理**：使用 QMap 存储所有活跃连接
3. **资源清理**：客户端断开时销毁 ClientHandler，使用 deleteLater()
4. **信号槽**：新连接和断开事件的处理
5. **incomingConnection()**：QTcpServer 的虚函数，有新连接时自动调用

---

## 关键代码片段

### TcpServer 使用示例
```cpp
// 创建并启动服务器
TcpServer *server = new TcpServer(this);

// 连接信号
connect(server, &TcpServer::newClientConnected, 
        [](qintptr desc) {
    qInfo() << "新客户端:" << desc;
});

// 启动服务器
if (server->startServer(8080)) {
    qInfo() << "服务器启动成功";
}
```

### 连接管理流程
```
新连接 → incomingConnection() 
    → 创建 ClientHandler 
    → 存储到 m_clients 
    → 发送 newClientConnected 信号

客户端断开 → ClientHandler::onDisconnected() 
    → 发送 clientDisconnect 信号 
    → TcpServer::onClientDisconnected() 
    → 从 m_clients 移除 
    → 销毁 ClientHandler
```

---

## 常见问题

### Q: 为什么继承 QTcpServer 而不是组合？
A: 继承可以重写 virtual 函数（如 incomingConnection），更符合 Qt 的设计模式。

### Q: 为什么使用 deleteLater() 而不是 delete？
A: deleteLater() 会在事件循环中安全删除，避免在信号槽处理过程中删除对象导致崩溃。

### Q: QMap 和 QHash 的区别？
A: QMap 按 key 排序，查找 O(log n)；QHash 无序，查找 O(1)。这里用 QMap 即可。

---

> 📝 每次完成功能后更新此报告，记录新完成的任务和相关技术决策
