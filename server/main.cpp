#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <csignal>
#include "DbManager.h"
#include "Constants.h"
#include "TcpServer.h"

namespace {

// 信号处理函数里只能做异步信号安全的操作，因此这里仅置标志位，
// 真正的收尾工作交给事件循环里的定时器完成
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
    
    // 1. 初始化数据库
    qInfo() << "正在初始化数据库...";
    if (!DbManager::instance().init()) {
        qCritical() << "数据库初始化失败!";
        return -1;
    }
    qInfo() << "数据库初始化成功";
    
    // 上次进程被强杀/崩溃时客户端不会走断开回调，数据库里会残留 status=1，
    // 导致好友列表把离线用户显示成在线，这里统一重置
    DbManager::instance().resetOnlineStatus();

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
    quint16 port = IMConstants::kDefaultServerPort;
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

    // 6. 进入事件循环
    return app.exec();
}
