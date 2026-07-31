# IMSystem — 即时通讯系统

基于 **Qt 6 + C++17** 开发的 C/S 架构即时通讯系统，支持用户注册登录、好友管理、一对一聊天等功能。

## 功能特性

### 用户系统
- 用户注册（SHA256 + 随机盐值加密存储）
- 用户登录（Token 认证、多端登录踢人）
- 在线状态管理

### 好友系统
- 搜索用户
- 发送/接受/拒绝好友请求
- 好友列表、删除好友
- 待处理好友请求查看

### 聊天系统
- 一对一文本消息收发
- 消息确认（ACK）
- 聊天历史记录查询
- 离线消息存储

### 系统特性
- 自定义二进制协议（16 字节消息头 + JSON 消息体）
- 粘包/拆包处理
- Token 验证机制
- **多线程数据库操作**（QThreadPool 线程池异步执行，不阻塞主线程）
- 线程本地 SQLite 连接（WAL 模式）

## 技术栈

| 项目 | 技术 |
|------|------|
| 语言 | C++17 |
| 框架 | Qt 6.5+ |
| 构建 | CMake 3.19+ |
| 数据库 | SQLite |
| 网络 | Qt Network (QTcpServer / QTcpSocket) |
| 并发 | QThreadPool + QRunnable |

## 项目结构

```
IMSystem/
├── CMakeLists.txt
├── main.cpp                          # 客户端入口
├── mainwindow.h/cpp/ui               # 客户端主窗口
│
├── common/                           # 公共模块
│   ├── protocol.h                    # 通信协议（消息头、消息类型）
│   ├── message.h/cpp                 # 消息序列化/反序列化
│   └── utils.h/cpp                   # 工具类（密码哈希、UUID）
│
├── client/                           # 客户端
│   ├── tcpclient.h/cpp               # TCP 客户端（单例）
│   └── ui/
│       ├── logindialog.h/cpp/ui      # 登录界面
│       └── registerdialog.h/cpp/ui   # 注册界面
│
└── server/                           # 服务端
    ├── main.cpp                      # 服务端入口
    ├── tcpserver.h/cpp               # TCP 服务器
    ├── clienthandler.h/cpp           # 客户端连接处理器
    ├── usermanager.h/cpp             # 在线用户管理
    ├── database/
    │   ├── dbmanager.h/cpp           # 数据库管理（同步 + 异步接口）
    │   └── init.sql                  # 建表脚本
    ├── service/
    │   ├── authservice.h/cpp         # 认证服务（注册/登录）
    │   ├── friendservice.h/cpp       # 好友服务
    │   └── chatservice.h/cpp         # 聊天服务
    └── threading/
        ├── taskrunner.h              # 线程池任务执行器
        └── dbconnectionhelper.h      # 线程本地数据库连接
```

## 数据库设计

### users 表
| 字段 | 类型 | 说明 |
|------|------|------|
| id | INTEGER | 主键，自增 |
| username | TEXT | 用户名（唯一） |
| password_hash | TEXT | SHA256 哈希密码 |
| salt | TEXT | 密码盐值 |
| nickname | TEXT | 昵称 |
| status | INTEGER | 0=离线，1=在线 |
| created_at | INTEGER | 创建时间戳 |
| updated_at | INTEGER | 更新时间戳 |

### friendships 表
| 字段 | 类型 | 说明 |
|------|------|------|
| user_id | INTEGER | 用户ID |
| friend_id | INTEGER | 好友ID |
| status | INTEGER | 0=待确认，1=已接受 |

### messages 表
| 字段 | 类型 | 说明 |
|------|------|------|
| msg_id | TEXT | 消息UUID（唯一） |
| sender_id | INTEGER | 发送者ID |
| receiver_id | INTEGER | 接收者ID |
| type | INTEGER | 消息类型 |
| content | TEXT | 消息内容 |
| timestamp | INTEGER | 时间戳 |
| is_read | INTEGER | 0=未读，1=已读 |

## 通信协议

### 消息头（16 字节）

```
+----------+---------+------+----------+------------+----------+
| magic(4) | ver(1)  | type | reserved | bodyLen(4) | seq(4)   |
+----------+---------+------+----------+------------+----------+
| 494D5359 | 01      | 2B   | 1B       | 4B         | 4B       |
+----------+---------+------+----------+------------+----------+
```

- `magic`: 固定值 `0x494D5359`（"IMSY"）
- `version`: 协议版本号
- `type`: 消息类型（参见 `protocol.h`）
- `bodyLength`: 消息体长度
- `sequence`: 序列号（用于请求-响应匹配）

### 主要消息类型

| 类型码 | 名称 | 说明 |
|--------|------|------|
| 1000/1001 | REQ/RSP_REGISTER | 注册 |
| 1002/1003 | REQ/RSP_LOGIN | 登录 |
| 3001/3002 | REQ/RSP_ADD_FRIEND | 添加好友 |
| 3003/3004 | REQ/RSP_FRIEND_LIST | 好友列表 |
| 3006/3007 | REQ/RSP_ACCEPT_FRIEND | 接受好友 |
| 3012/3013 | REQ/RSP_SEARCH_USER | 搜索用户 |
| 4001 | MSG_TEXT | 文本消息 |
| 4004 | MSG_ACK | 消息确认 |
| 4005 | MSG_HISTORY | 聊天历史 |

## 构建与运行

### 环境要求

- Qt 6.5+
- CMake 3.19+
- MinGW 或 MSVC 编译器

### 构建步骤

```bash
# 1. 配置
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="D:/Qt/6.x.x/mingw_64"

# 2. 编译
cmake --build build --target IMServer
cmake --build build --target IMClient

# 3. 运行（先启动服务端）
./build/IMServer.exe

# 4. 再启动客户端
./build/IMClient.exe
```

或直接在 **Qt Creator** 中打开 `CMakeLists.txt`，选择构建套件后一键构建运行。

## 多线程架构

服务端使用 **线程池** 处理数据库操作，避免阻塞主线程的网络事件循环：

```
主线程（事件循环）                Worker 线程（QThreadPool）
┌─────────────────────┐        ┌──────────────────────┐
│ ClientHandler        │        │ DbTask::run()        │
│  → handleMessage()   │        │  → 线程本地DB连接     │
│  → Service::handle*()│───────►│  → 执行 SQL 查询     │
│    → DbManager       │        │  → 返回结果           │
│       ::*Async()     │        └──────────┬───────────┘
│                      │                   │
│ callback(result)  ◄──┼───────────────────┘
│  → sendResponse()    │   (Qt::QueuedConnection)
└─────────────────────┘
```

关键设计：
- **DbConnectionHelper**: 每个 worker 线程创建独立的 SQLite 连接（线程安全）
- **TaskRunner**: 封装 QThreadPool，提供 `runDbTask()` 模板方法
- **QPointer 安全检查**: 回调中检查客户端连接是否仍然存活

## 安全机制

- **密码存储**: SHA256 + 随机盐值，数据库不存明文密码
- **Token 认证**: 登录后生成 UUID Token，非登录/注册请求需携带 Token
- **输入验证**: 服务端对所有请求参数进行校验
- **多端登录**: 同一账号在新设备登录时，旧连接被踢下线

## 待开发功能

- [ ] 群聊系统（协议已预留）
- [ ] 图片/文件消息传输
- [ ] 心跳检测与超时断开
- [ ] 好友状态实时广播
- [ ] 消息已读回执
- [ ] 离线消息推送

## 许可证

MIT License