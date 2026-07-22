/**
 * main.cpp — 服务端程序入口
 * 
 * 功能：
 * 1. 初始化数据库
 * 2. 创建并启动 TCP 服务器
 * 3. 进入事件循环
 * 
 * 使用方法：
 * ./IMServer [port]
 * 默认端口：8080
 */

#include <QCoreApplication>
#include <QDebug>
#include "dbmanager.h"
#include "tcpserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    // 设置应用程序信息
    QCoreApplication::setApplicationName("IMServer");
    QCoreApplication::setApplicationVersion("1.0.0");
    
    qInfo() << "===========================================";
    qInfo() << "  IMSystem Server v1.0.0";
    qInfo() << "===========================================";
    
    // 1. 初始化数据库
    qInfo() << "正在初始化数据库...";
    if (!DbManager::instance().init("imsystem.db")) {
        qCritical() << "数据库初始化失败!";
        return -1;
    }
    qInfo() << "数据库初始化成功";
    
    // 2. 创建 TCP 服务器
    TcpServer server;
    
    // 3. 连接信号槽（用于日志）
    QObject::connect(&server, &TcpServer::newClientConnected, 
                     [](qintptr descriptor) {
        qInfo() << "新客户端连接, descriptor:" << descriptor;
    });
    
    QObject::connect(&server, &TcpServer::clientDisconnected,
                     [](qintptr descriptor) {
        qInfo() << "客户端断开, descriptor:" << descriptor;
    });
    
    // 4. 获取端口号（从命令行参数或使用默认值）
    quint16 port = 8080;
    if (argc > 1) {
        bool ok;
        quint16 cmdPort = QString(argv[1]).toUShort(&ok);
        if (ok && cmdPort > 0) {
            port = cmdPort;
        }
    }
    
    // 5. 启动服务器
    qInfo() << "正在启动服务器, 端口:" << port;
    if (!server.startServer(port)) {
        qCritical() << "服务器启动失败!";
        return -1;
    }
    
    qInfo() << "服务器启动成功!";
    qInfo() << "监听地址: 0.0.0.0:" << port;
    qInfo() << "等待客户端连接...";
    qInfo() << "===========================================";
    
    // 6. 进入事件循环
    return app.exec();
}
