#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <csignal>
#include "DbManager.h"
#include "Constants.h"
#include "TcpServer.h"

namespace {

// 标志位

volatile std::sig_atomic_t g_terminationRequested = 0;

void handleTerminationSignal(int)
{
    g_terminationRequested = 1;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    // 设置应用程序信息
    QCoreApplication::setApplicationName("IMServer");
    QCoreApplication::setApplicationVersion("1.0.0");
    
    qInfo() << "===========================================";
    qInfo() << "  IMSystem Server v1.0.0";
    qInfo() << "===========================================";
    
    // 初始化数据库
    qInfo() << "正在初始化数据库...";
    if (!DbManager::instance().init()) {
        qCritical() << "数据库初始化失败!";
        return -1;
    }
    qInfo() << "数据库初始化成功";
    
    // 统一重置用户状态
    DbManager::instance().resetOnlineStatus();

    // 创建TCP服务器
    TcpServer server;
    
    // 连接日志
    QObject::connect(&server, &TcpServer::newClientConnected, 
                     [](qintptr descriptor) {
        qInfo() << "新客户端连接, descriptor:" << descriptor;
    });
    
    QObject::connect(&server, &TcpServer::clientDisconnected,
                     [](qintptr descriptor) {
        qInfo() << "客户端断开, descriptor:" << descriptor;
    });
    
    // 获取端口号（从命令行参数或使用默认值）
    quint16 port = IMConstants::kDefaultServerPort;
    if (argc > 1) {
        bool ok;
        quint16 cmdPort = QString(argv[1]).toUShort(&ok);
        if (ok && cmdPort > 0) {
            port = cmdPort;
        }
    }
    
    // 启动服务器
    qInfo() << "正在启动服务器, 端口:" << port;
    if (!server.startServer(port)) {
        qCritical() << "服务器启动失败!";
        return -1;
    }
    
    qInfo() << "服务器启动成功!";
    qInfo() << "监听地址: 0.0.0.0:" << port;
    qInfo() << "等待客户端连接...";
    qInfo() << "===========================================";
    
    // 优雅退出：Ctrl+C / kill 时先停止监听、清理连接，再退出事件循环，
    // 避免留下残留的在线状态与未释放的连接
    std::signal(SIGINT, handleTerminationSignal);
    std::signal(SIGTERM, handleTerminationSignal);

    QTimer terminationWatcher;
    QObject::connect(&terminationWatcher, &QTimer::timeout, [&app, &server]() {
        if (g_terminationRequested) {
            qInfo() << "收到退出信号，正在优雅关闭...";
            server.stopServer();
            app.quit();
        }
    });
    terminationWatcher.start(200);

    // 进入事件循环
    return app.exec();
}
