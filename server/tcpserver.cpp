#include "tcpserver.h"
#include "clienthandler.h"

TcpServer::TcpServer(QObject *parent)
    : QTcpServer(parent)
{
    qInfo() << "TcpServer 创建";
}

TcpServer::~TcpServer()
{
    stopServer();
    qInfo() << "TcpServer 销毁";
}

bool TcpServer::startServer(quint16 port)
{
    // 尝试监听指定端口
    if (!listen(QHostAddress::Any, port)) {
        qCritical() << "服务器启动失败:" << errorString();
        return false;
    }

    qInfo() << "服务器启动成功，监听端口:" << port;
    return true;
}

void TcpServer::stopServer()
{
    // 1. 停止监听
    close();

    // 2. 清理所有客户端连接
    clearAllClients();

    qInfo() << "服务器已停止";
}


int TcpServer::clientCount() const
{
    return m_clients.size();
}

void TcpServer::incomingConnection(qintptr socketDescriptor)
{
    qInfo() << "新连接请求, socketDescriptor:" << socketDescriptor;

    //创建 socket 并设置描述符
    QTcpSocket *socket = new QTcpSocket(this);
    if (!socket->setSocketDescriptor(socketDescriptor)) {
        qWarning() << "设置 socket 描述符失败";
        delete socket;
        return;
    }

    //给每一个连接创建ClientHandler
    ClientHandler *handler = new ClientHandler(socket, this);

    //连接客户端断开，执行断开槽函数
    connect(handler, &ClientHandler::clientDisconnect,
            this, &TcpServer::onClientDisconnected);

    //把客户端存储到映射
    m_clients.insert(socketDescriptor, handler);

    //发送新连接信号
    emit newClientConnected(socketDescriptor);

    qInfo() << "客户端连接成功, 当前连接数:" << m_clients.size();
}


void TcpServer::onClientDisconnected(qintptr socketDescriptor)
{
    // 从映射中查找并移除
    auto it = m_clients.find(socketDescriptor);
    if (it != m_clients.end()) {
        // 获取 ClientHandler
        ClientHandler *handler = it.value();

        // 从映射中移除
        m_clients.erase(it);

        // 销毁 ClientHandler
        handler->deleteLater();

        qInfo() << "客户端断开, 当前连接数:" << m_clients.size();
    }

    // 发送断开信号
    emit clientDisconnected(socketDescriptor);
}


void TcpServer::clearAllClients()
{
    qInfo() << "清理所有客户端连接, 数量:" << m_clients.size();

    // 遍历所有 ClientHandler 并销毁
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        ClientHandler *handler = it.value();
        handler->deleteLater();
    }

    // 清空映射
    m_clients.clear();
}
