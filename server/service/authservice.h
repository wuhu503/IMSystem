/**
 * authservice.h — 认证服务类
 * 
 * 职责：
 * 1. 处理用户注册请求
 * 2. 处理用户登录请求
 * 3. 与数据库交互，验证用户信息
 * 
 * 设计模式：单例模式（Singleton）
 */

#ifndef AUTHSERVICE_H
#define AUTHSERVICE_H

#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>

class ClientHandler;
class Message;
enum class MessageType : uint16_t;

class AuthService : public QObject
{
    Q_OBJECT

public:
    /**
     * 获取单例实例
     */
    static AuthService& instance();
    
    // 禁止拷贝和赋值
    AuthService(const AuthService&) = delete;
    AuthService& operator=(const AuthService&) = delete;
    
    /**
     * 处理注册请求
     * @param client 客户端处理器
     * @param msg 请求消息
     * 
     * 请求格式：{username: "xxx", password: "xxx"}
     * 响应格式：{success: true/false, user_id: xxx, message: "xxx"}
     */
    void handleRegister(ClientHandler *client, const Message &msg);
    
    /**
     * 处理登录请求
     * @param client 客户端处理器
     * @param msg 请求消息
     * 
     * 请求格式：{username: "xxx", password: "xxx"}
     * 响应格式：{success: true/false, token: "xxx", user_id: xxx, message: "xxx"}
     */
    void handleLogin(ClientHandler *client, const Message &msg);

signals:
    /**
     * 发送响应消息给客户端
     * @param client 客户端处理器
     * @param msg 响应消息
     */
    void sendResponse(ClientHandler *client, const Message &msg);

private:
    /**
     * 私有构造函数（单例模式）
     */
    AuthService();
    
    /**
     * 析构函数
     */
    ~AuthService();
    
    /**
     * 创建响应消息
     * @param type 消息类型
     * @param sequence 序列号（与请求匹配）
     * @param body 响应体 JSON
     * @return 响应消息对象
     */
    Message createResponse(MessageType type, uint32_t sequence, 
                           const QJsonObject &body);
    
    /**
     * 发送错误响应
     * @param client 客户端处理器
     * @param type 响应消息类型
     * @param sequence 序列号
     * @param errorMessage 错误信息
     */
    void sendErrorResponse(ClientHandler *client, MessageType type, 
                          uint32_t sequence, const QString &errorMessage);
};

#endif // AUTHSERVICE_H
