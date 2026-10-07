# IMSystem — 即时通讯系统

基于 **Qt 6 + C++17** 开发的 C/S 架构即时通讯系统，支持用户注册登录、好友管理、一对一聊天等功能。

## 功能特性

### 用户系统
- 用户注册（PBKDF2-HMAC-SHA256 + 随机盐；兼容旧库的单轮 SHA256，登录成功后自动升级）
- 用户登录（Token 认证、多端登录踢人、连续失败限速）
- 在线状态管理（以服务端内存为准，好友列表实时反映；进程异常退出后启动时自动清理残留状态）

### 好友系统
- 搜索用户（LIKE 通配符已转义）
- 发送/接受/拒绝好友请求
- 好友列表、删除好友
- 待处理好友请求查看
- 实时通知：收到好友请求 / 请求被接受 / 请求被拒绝 / 被解除好友关系

### 聊天系统
- 一对一文本消息收发
- 发送结果回执（RSP_TEXT，成功时返回 msg_id）
- 已读回执（打开会话 / 收到当前会话消息时上报）
- 聊天历史记录查询（分页）
- 离线消息：接收者离线时入库，上线后自动补推

### 系统特性
- 自定义二进制协议（16 字节消息头 + JSON 消息体）
- 粘包/拆包处理
- Token 验证机制
- 心跳保活 + 空闲连接回收
- **多线程数据库操作**（QThreadPool 线程池异步执行，不阻塞主线程）
- 线程本地 SQLite 连接（WAL 模式），随工作线程退出而关闭

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
├── MainWindow.h/cpp/ui               # 客户端主窗口
│
├── common/                           # 公共模块
│   ├── Protocol.h                    # 通信协议（消息头、消息类型）
│   ├── Constants.h                   # 全局常量（长度上限、超时、端口等）
│   ├── Message.h/cpp                 # 消息序列化/反序列化
│   └── Utils.h/cpp                   # 工具类（密码哈希、UUID）
│
├── client/                           # 客户端
│   ├── TcpClient.h/cpp               # 传输层：socket、粘包拆包、token 携带
│   ├── ChatSession.h/cpp             # 会话层：登录注册、序列号、心跳、回执、报文→语义信号
│   ├── FriendStore.h/cpp             # 状态层：好友列表与未读数（不依赖任何 widget）
│   └── ui/
│       ├── UiKit.h/cpp               # 公共绘制：头像、红点、聊天气泡
│       ├── ClientDialogs.h/cpp       # 搜索结果选择、好友请求对话框
│       ├── LoginDialog.h/cpp/ui      # 登录界面
│       └── RegisterDialog.h/cpp/ui   # 注册界面
│
└── server/                           # 服务端
    ├── main.cpp                      # 服务端入口
    ├── TcpServer.h/cpp               # TCP 服务器
    ├── ClientHandler.h/cpp           # 客户端连接处理器
    ├── UserManager.h/cpp             # 在线用户管理
    ├── database/
    │   ├── DbManager.h/cpp           # 连接、建表、结构迁移、启动状态清理
    │   ├── DbTypes.h                 # 仓储层结果类型（登录/注册/加好友）
    │   ├── UserRepository.h/cpp      # users 表：注册、登录校验、口令升级、状态
    │   ├── FriendRepository.h/cpp    # friendships 表：列表、搜索、申请/接受/拒绝/删除
    │   ├── MessageRepository.h/cpp   # messages 表：入库、历史、离线投递、已读
    │   └── init.sql                  # 建表脚本
    ├── service/
    │   ├── AuthService.h/cpp         # 认证服务（注册/登录）
    │   ├── FriendService.h/cpp       # 好友服务
    │   ├── ChatService.h/cpp         # 聊天服务
    │   └── Reply.h                   # 统一的响应构造与发送
    └── threading/
        ├── TaskRunner.h/cpp          # 线程池任务执行器
        └── DbConnectionHelper.h/cpp  # 线程本地数据库连接与数据库路径
```

> 命名约定：类对应的文件用大驼峰（`TcpClient.h/cpp`），入口与资源文件保持小写
> （`main.cpp`、`init.sql`、`README.md`）。

### 分层约定

```
客户端：UI（MainWindow / 对话框） → FriendStore（好友与未读状态） + ChatSession（会话·协议） → TcpClient（传输） → 网络
服务端：ClientHandler（连接） → Service（业务） → Repository（数据） → DbManager（库）/ SQLite
```

- 界面层不出现 `MessageType`、序列号、token；只接收 `ChatSession` 的语义信号
- Service 层只做业务编排与通知，不写 SQL
- Repository 层只做单表读写，返回值是 `bool / QJsonArray / 结构体`
- `DbManager` 只管连接与结构迁移，不含任何业务 SQL

## 数据库设计

### users 表
| 字段 | 类型 | 说明 |
|------|------|------|
| id | INTEGER | 主键，自增 |
| username | TEXT | 用户名（唯一） |
| password_hash | TEXT | `pbkdf2$<迭代次数>$<哈希>`；旧数据为单轮 SHA256 十六进制 |
| salt | TEXT | 密码盐值 |
| nickname | TEXT | 昵称 |
| status | INTEGER | 0=离线，1=在线（仅作快照，在线判定以服务端内存为准） |
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
| is_read | INTEGER | 0=未读，1=已读（由客户端已读回执更新） |
| delivered | INTEGER | 0=未投递，1=已投递（离线消息补推依据） |

索引：`idx_messages_pair(sender_id, receiver_id, timestamp)` 用于聊天历史检索。

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
| 1004/1005 | REQ/RSP_LOGOUT | 退出登录 |
| 3001/3002 | REQ/RSP_ADD_FRIEND | 添加好友 |
| 3003/3004 | REQ/RSP_FRIEND_LIST | 好友列表 |
| 3006/3007 | REQ/RSP_ACCEPT_FRIEND | 接受好友 |
| 3008/3009 | REQ/RSP_REJECT_FRIEND | 拒绝好友 |
| 3010/3011 | REQ/RSP_DELETE_FRIEND | 删除好友 |
| 3012/3013 | REQ/RSP_SEARCH_USER | 搜索用户 |
| 3014/3015 | REQ/RSP_PENDING_REQUESTS | 待处理好友请求 |
| 3016 | NTF_FRIEND_REQUEST | 推送：收到好友请求 |
| 3017 | NTF_FRIEND_ACCEPTED | 推送：好友请求被接受 |
| 3018 | NTF_FRIEND_REJECTED | 推送：好友请求被拒绝 |
| 3019 | NTF_FRIEND_REMOVED | 推送：被解除好友关系 |
| 4001 | MSG_TEXT | C→S 发送文本消息 |
| 4004 | RSP_TEXT | S→C 文本发送结果（成功带 msg_id） |
| 4005 | MSG_ACK | C→S 已读回执 |
| 4006/4007 | REQ/RSP_HISTORY | 聊天历史请求/响应 |
| 9001 | HEARTBEAT | 心跳（保持连接活跃） |

`4002/4003`（图片/文件）与 `5xxx`（群聊）为预留类型，当前未实现。

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

### 数据与配置

- 服务端可用第一个命令行参数指定端口：`IMServer.exe 9000`（默认 8080）
- 数据库默认位于用户数据目录（Windows 为 `%APPDATA%/IMSystem/imsystem.db`），
  首次启动会自动把旧的“可执行文件同目录”数据库迁移过去，不会丢历史数据
- 需要自定义位置时设置环境变量 `IM_DB_PATH`，例如用独立库做测试：
  `set IM_DB_PATH=D:\tmp\test.db`
- 建表脚本 `init.sql` 会随构建复制到输出目录；启动时还会做一次结构迁移（补列、补索引）

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
- **DbConnectionHelper**: 每个 worker 线程创建独立的 SQLite 连接，并在该线程退出时关闭（连接始终在创建它的线程里销毁）
- **TaskRunner**: 封装 QThreadPool，提供 `runDbTask()` 模板方法
- **QPointer 生命周期检查**: 任务提交时即建立弱引用，接收者被销毁后自动丢弃结果，回调不会打到悬垂对象上
- **慢操作不占主线程**: 口令哈希（PBKDF2）与所有 SQL 都在工作线程内完成

## 安全机制

- **密码存储**: PBKDF2-HMAC-SHA256（12 万次迭代）+ 系统级随机盐，定长比较；旧库单轮 SHA256 哈希在登录成功后自动升级
- **Token 认证**: 登录后生成 UUID Token，非登录/注册请求需携带 Token
- **输入验证**: 用户名 3-20 字符、密码 6-64 字符、单条消息 ≤2000 字符，服务端校验
- **登录限速**: 同一账号连续失败 5 次后锁 60 秒
- **多端登录**: 同一账号在新设备登录时，旧连接被踢下线
- **连接治理**: 心跳保活 + 空闲连接回收；发送缓冲超过 4 MB 主动断开，避免内存被拖垮

## 待开发功能

- [ ] 群聊系统（协议已预留）
- [ ] 图片/文件消息传输
- [ ] 好友状态实时广播
- [ ] 聊天记录下拉分页加载
- [ ] 传输层加密（TLS）

## 许可证

MIT License
