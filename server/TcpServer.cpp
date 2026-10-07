#include "TcpServer.h"
#include "ClientHandler.h"
#include "UserManager.h"
#include "Constants.h"
#include <QDateTime>
#include <QTimer>

TcpServer::TcpServer(QObject *parent)
    : QTcpServer(parent)
    , m_idleTimer(new QTimer(this))
{
    // 周期性回收长时间没有任何报文的连接(没有FIN的客户端断开)
    m_idleTimer->setInterval(IMConstants::kIdleSweepIntervalMs);
    connect(m_idleTimer, &QTimer::timeout, this, &TcpServer::onIdleCheck);
    m_idleTimer->start();

    qInfo() << "TcpServer 创建";
}

TcpServer::~TcpServer()
{
    stopServer();
    qInfo() << "TcpServer 销毁";
}

bool TcpServer::startServer(quint16 port)
{
    // 监听端口
    if (!listen(QHostAddress::Any, port)) {
        qCritical() << "服务器启动失败:" << errorString();
        return false;
    }

    qInfo() << "服务器启动成功，监听端口:" << port;
    return true;
}

void TcpServer::stopServer()
{
    // 停止监听
    close();
    m_idleTimer->stop();

    // 清理所有客户端连接
    clearAllClients();

    qInfo() << "服务器已停止";
}


void TcpServer::incomingConnection(qintptr socketDescriptor)
{
    qInfo() << "新连接请求, socketDescriptor:" << socketDescriptor;

    // 创建socket并设置描述符
    QTcpSocket *socket = new QTcpSocket;
    if (!socket->setSocketDescriptor(socketDescriptor)) {
        qWarning() << "设置 socket 描述符失败";
        delete socket;
        return;
    }

    // 给每一个连接创建ClientHandler(递交socket的生命周期)
    ClientHandler *handler = new ClientHandler(socket, this);

    // 连接客户端断开，执行断开槽函数
    connect(handler, &ClientHandler::clientDisconnect,
            this, &TcpServer::onClientDisconnected);

    // 把客户端存储到映射
    m_clients.insert(socketDescriptor, handler);

    // 发送新连接信号
    emit newClientConnected(socketDescriptor);

    qInfo() << "客户端连接成功, 当前连接数:" << m_clients.size();
}


void TcpServer::onClientDisconnected(qintptr socketDescriptor)
{
    // 从映射中查找并移除
    auto it = m_clients.find(socketDescriptor);
    if (it == m_clients.end()) {
        qWarning() << "断开连接时未找到对应客户端, descriptor:" << socketDescriptor
                   << ", 当前连接数:" << m_clients.size();
        emit clientDisconnected(socketDescriptor);
        return;
    }

    // 获取ClientHandler
    ClientHandler *handler = it.value();

    // 从映射中移除
    m_clients.erase(it);

    // 销毁ClientHandler
    handler->deleteLater();

    qInfo() << "客户端断开, 当前连接数:" << m_clients.size();

    // 发送断开信号
    emit clientDisconnected(socketDescriptor);
}


void TcpServer::clearAllClients()
{
    qInfo() << "清理所有客户端连接, 数量:" << m_clients.size();

    // 遍历所有ClientHandler并销毁
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        ClientHandler *handler = it.value();
        // 同步在线表，避免进程退出后 UserManager 里还留着已销毁的连接
        if (handler && handler->userId() != -1) {
            UserManager::instance().userOffline(handler->userId());
        }
        handler->deleteLater();
    }

    // 清空映射
    m_clients.clear();
}

void TcpServer::onIdleCheck()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 先收集再断链：disconnectFromHost() 可能同步触发 onClientDisconnected，
    QList<ClientHandler *> idleHandlers;
    for (auto it = m_clients.constBegin(); it != m_clients.constEnd(); ++it) {
        if (it.value() && it.value()->isIdle(now)) {
            idleHandlers.append(it.value());
        }
    }

    for (ClientHandler *handler : idleHandlers) {
        qInfo() << "连接空闲超时，主动断开, descriptor:" << handler->descriptor();
        if (handler->socket()) {
            handler->socket()->disconnectFromHost();
        }
    }
}
