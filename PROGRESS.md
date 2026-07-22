# IMSystem 开发进度报告

> 项目：Qt6 即时通讯系统
> 技术栈：Qt 6.11 + C++17 + SQLite + CMake
> 更新时间：2026-07-22

---

## 项目概述

构建可维护、可扩展的 C/S 架构即时通讯系统，支持用户注册登录、好友系统、一对一聊天、群聊、文件传输等功能。

---

## 已完成任务

### 2026-07-22

#### ✅ 8. 认证服务 - AuthService
- [x] 创建 server/service/authservice.h（类声明）
- [x] 创建 server/service/authservice.cpp（类实现）
- [x] 实现单例模式（instance()）
- [x] 实现注册处理（handleRegister）
  - 解析请求 JSON
  - 验证参数（用户名、密码长度）
  - 检查用户名是否已存在
  - 生成盐值和密码哈希（SHA256）
  - 存入数据库
  - 返回成功响应
- [x] 实现登录处理（handleLogin）
  - 解析请求 JSON
  - 检查用户是否存在
  - 验证密码
  - 生成 Token（UUID）
  - 设置用户ID
  - 更新在线状态
  - 返回成功响应
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
│
├── common/                         # ✅ 公共模块（已完成）
│   ├── protocol.h                  # 协议定义
│   ├── message.h/cpp               # 消息封装
│   └── utils.h/cpp                 # 工具函数
│
└── server/                         # ✅ 服务端（基本完成）
    ├── main.cpp                    # 服务端入口（待实现）
    ├── tcpserver.h/cpp             # ✅ TCP服务器（已完成）
    ├── clienthandler.h/cpp         # ✅ 客户端连接处理（已完成）
    ├── database/
    │   ├── init.sql                # 建表脚本
    │   └── dbmanager.h/cpp         # ✅ 数据库管理（已完成）
    └── service/
        └── authservice.h/cpp       # ✅ 认证服务（已完成）
```

---

## 下一步任务

### 待实现：server/main.cpp（服务端入口）

**功能清单：**
1. **初始化数据库**
   - 调用 DbManager::instance().init()
   
2. **创建并启动服务器**
   - 创建 TcpServer 实例
   - 连接信号槽（新连接、断开）
   - 调用 startServer(port)
   
3. **事件循环**
   - 创建 QApplication
   - 进入事件循环

**代码框架：**
```cpp
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    // 初始化数据库
    DbManager::instance().init("imsystem.db");
    
    // 创建并启动服务器
    TcpServer server;
    if (!server.startServer(8080)) {
        return -1;
    }
    
    qInfo() << "服务器已启动";
    
    return app.exec();
}
```

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

---

## 面试要点备忘

1. **注册流程**：参数验证 → 用户名检查 → 盐值生成 → 密码加密 → 数据库存储
2. **登录流程**：用户查询 → 密码验证 → Token 生成 → 在线状态更新
3. **密码安全**：SHA256 + 随机盐值，数据库不存明文密码
4. **单例模式**：使用 static 局部变量实现线程安全
5. **错误处理**：统一的错误响应格式

---

## 关键代码片段

### AuthService 使用示例
```cpp
// 在 ClientHandler::handleMessage() 中调用
case MessageType::REQ_REGISTER:
    AuthService::instance().handleRegister(this, msg);
    break;

case MessageType::REQ_LOGIN:
    AuthService::instance().handleLogin(this, msg);
    break;
```

### 注册请求/响应格式
```json
// 请求
{
    "type": 1000,
    "username": "zhangsan",
    "password": "123456",
    "sequence": 1
}

// 响应
{
    "type": 1001,
    "success": true,
    "user_id": 10001,
    "message": "注册成功",
    "sequence": 1
}
```

### 登录请求/响应格式
```json
// 请求
{
    "type": 1002,
    "username": "zhangsan",
    "password": "123456",
    "sequence": 2
}

// 响应
{
    "type": 1003,
    "success": true,
    "token": "550e8400-e29b-41d4-a716-446655440000",
    "user_id": 10001,
    "message": "登录成功",
    "sequence": 2
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

---

> 📝 每次完成功能后更新此报告，记录新完成的任务和相关技术决策
