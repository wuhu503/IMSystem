-- init.sql — 数据库建表脚本
-- 数据库：SQLite
-- 用途：存储用户信息、好友关系、聊天消息

-- ========== 用户表 ==========
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,  -- 用户ID（自增主键）
    username TEXT NOT NULL UNIQUE,          -- 用户名（唯一）
    password_hash TEXT NOT NULL,            -- SHA256哈希后的密码
    salt TEXT NOT NULL,                     -- 密码盐值
    nickname TEXT DEFAULT '',               -- 昵称
    avatar TEXT DEFAULT '',                 -- 头像URL
    status INTEGER DEFAULT 0,              -- 在线状态：0离线，1在线
    created_at INTEGER NOT NULL,            -- 创建时间（时间戳）
    updated_at INTEGER NOT NULL             -- 更新时间（时间戳）
);

-- ========== 好友关系表 ==========
CREATE TABLE IF NOT EXISTS friendships (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL,               -- 用户ID
    friend_id INTEGER NOT NULL,             -- 好友ID
    status INTEGER DEFAULT 0,              -- 状态：0待确认，1已接受，2已拒绝
    created_at INTEGER NOT NULL,            -- 创建时间
    FOREIGN KEY (user_id) REFERENCES users(id),
    FOREIGN KEY (friend_id) REFERENCES users(id),
    UNIQUE(user_id, friend_id)              -- 唯一约束，防止重复好友关系
);

-- ========== 消息表 ==========
CREATE TABLE IF NOT EXISTS messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    msg_id TEXT NOT NULL UNIQUE,            -- 消息UUID（用于去重）
    sender_id INTEGER NOT NULL,             -- 发送者ID
    receiver_id INTEGER NOT NULL,           -- 接收者ID（用户ID或群组ID）
    type INTEGER NOT NULL,                  -- 消息类型（MessageType枚举值）
    content TEXT NOT NULL,                  -- 消息内容（JSON格式）
    timestamp INTEGER NOT NULL,             -- 消息时间戳
    is_read INTEGER DEFAULT 0,             -- 是否已读：0未读，1已读
    FOREIGN KEY (sender_id) REFERENCES users(id),
    FOREIGN KEY (receiver_id) REFERENCES users(id)
);

-- ========== 群组表（后续实现） ==========
-- CREATE TABLE IF NOT EXISTS groups (
--     id INTEGER PRIMARY KEY AUTOINCREMENT,
--     name TEXT NOT NULL,
--     owner_id INTEGER NOT NULL,
--     created_at INTEGER NOT NULL,
--     FOREIGN KEY (owner_id) REFERENCES users(id)
-- );

-- ========== 索引 ==========
CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
CREATE INDEX IF NOT EXISTS idx_friendships_user ON friendships(user_id);
CREATE INDEX IF NOT EXISTS idx_friendships_friend ON friendships(friend_id);
CREATE INDEX IF NOT EXISTS idx_messages_sender ON messages(sender_id);
CREATE INDEX IF NOT EXISTS idx_messages_receiver ON messages(receiver_id);
CREATE INDEX IF NOT EXISTS idx_messages_timestamp ON messages(timestamp);
