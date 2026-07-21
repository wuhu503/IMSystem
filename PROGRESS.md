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

#### ✅ 6. 客户端连接处理 - ClientHandler
- [x] 创建 server/clienthandler.h（类声明）
- [x] 创建 server/clienthandler.cpp（类实现）
- [x] 实现构造函数（接收 socket，连接信号槽）
- [x] 实现析构函数（断开连接）
- [x] 实现粘包处理（onReadyRead）
  - 使用 QByteArray 缓冲区
  - 循环提取完整消息
  - 处理拆包：数据不完整时等待更多数据
- [x] 实现消息分发（handleMessage）
  - REQ_REGISTER → 注册处理
  - REQ_LOGIN → 登录处理
  - MSG_TEXT → 消息转发
  - HEARTBEAT → 心跳处理
- [x] 实现发送消息（sendMessage）
- [x] 实现用户ID管理（userId/setUserId）

### 2026-07-20

#### ✅ 5. 数据库层 - DbManager（单例模式）
- [x] 创建 server/database/ 目录结构
- [x] 编写 init.sql 建表脚本
- [x] 实现 DbManager 类（单例模式）
- [x] 实现用户 CRUD 操作

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
    ├── main.cpp                    # 服务端入口
    ├── server.h/cpp                # TCP服务器
    ├── tcpserver.h/cpp             # TCP服务器（备用）
    ├── clienthandler.h/cpp         # ✅ 客户端连接处理（已完成）
    ├── database/
    │   ├── init.sql                # 建表脚本
    │   └── dbmanager.h/cpp         # 数据库管理
    └── service/
        ├── authservice.h/cpp       # 认证服务
        └── ...
```

---

## 下一步任务

### 待实现：server/server.h + server.cpp（TCP服务器）

**功能清单：**
1. **TcpServer 类**（继承 QTcpServer）
   - `startServer(port)` — 启动服务器监听
   - `stopServer()` — 停止服务器
   - `incomingConnection()` — 重写虚函数，接受新连接
   - 管理所有 ClientHandler 实例

2. **连接管理**
   - 使用 QMap 存储所有活跃连接
   - 处理客户端断开（清理资源）

3. **消息转发**
   - 根据消息类型分发给 AuthService
   - 转发聊天消息给目标用户

---

## 技术决策记录

| 决策 | 选择 | 原因 |
|------|------|------|
| 消息格式 | 16字节固定头 + JSON体 | 固定头便于快速解析，JSON灵活可扩展 |
| 数据库 | SQLite | 轻量级，无需额外服务器 |
| 数据库访问 | 单例模式 DbManager | 全局唯一连接 |
| 粘包处理 | 缓冲区 + 长度前缀 | 可靠的消息边界划分 |
| 连接管理 | ClientHandler per connection | 职责清晰，易于扩展 |

---

## 面试要点备忘

1. **粘包/拆包**：TCP 是流式协议，需要应用层协议划分消息边界
2. **解决方案**：使用长度前缀协议（header 包含 bodyLength）
3. **缓冲区设计**：QByteArray 累积数据，循环提取完整消息
4. **信号槽机制**：readyRead 信号触发数据读取
5. **资源管理**：socket 由 Server 创建，Handler 负责使用

---

## 关键代码片段

### ClientHandler 使用示例
```cpp
// TcpServer 中创建 ClientHandler
void TcpServer::incomingConnection(qintptr socketDescriptor)
{
    QTcpSocket *socket = new QTcpSocket(this);
    socket->setSocketDescriptor(socketDescriptor);
    
    ClientHandler *handler = new ClientHandler(socket, this);
    
    // 连接断开信号
    connect(handler, &ClientHandler::clientDisconnect, 
            this, &TcpServer::onClientDisconnected);
    
    // 存储到映射
    m_clients.insert(socketDescriptor, handler);
}
```

### 消息处理流程
```
TCP接收 → onReadyRead() → 缓冲区累积 → 提取完整消息 
    → deserialize() → handleMessage() → 业务处理
```

---

## 常见问题

### Q: 为什么需要处理粘包？
A: TCP 是流式协议，不保证消息边界。可能一次收到多个消息，或一个消息分多次收到。

### Q: 如何判断消息是否完整？
A: 先读取 16 字节 header，获取 bodyLength，再检查缓冲区是否有足够的 body 数据。

### Q: ClientHandler 何时销毁？
A: 当客户端断开连接时，由 TcpServer 负责销毁对应的 ClientHandler。

---

> 📝 每次完成功能后更新此报告，记录新完成的任务和相关技术决策
