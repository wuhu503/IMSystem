#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "tcpclient.h"
#include "message.h"
#include "protocol.h"
#include <QMessageBox>
#include <QTime>
#include <QPainter>
#include <QScrollBar>
#include <QTimer>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    
    // 初始化好友列表
    initFriendList();
    
    // 连接信号槽
    connect(ui->friendList, &QListWidget::itemClicked, 
            this, &MainWindow::onFriendClicked);
    connect(ui->sendBtn, &QPushButton::clicked, 
            this, &MainWindow::onSendClicked);
    connect(ui->searchEdit, &QLineEdit::textChanged, 
            this, &MainWindow::onSearchTextChanged);
    
    // 好友按钮信号
    connect(ui->addFriendBtn, &QPushButton::clicked, 
            this, &MainWindow::onAddFriendClicked);
    connect(ui->refreshFriendsBtn, &QPushButton::clicked, 
            this, &MainWindow::onRefreshFriendsClicked);
    connect(ui->friendRequestsBtn, &QPushButton::clicked, 
            this, &MainWindow::onFriendRequestsClicked);
    connect(ui->deleteFriendBtn, &QPushButton::clicked, 
            this, &MainWindow::onDeleteFriendClicked);
    
    // 连接TcpClient信号
    connect(&TcpClient::instance(), &TcpClient::messageReceived,
            this, &MainWindow::onMessageReceived);
    connect(&TcpClient::instance(), &TcpClient::connectionEstablished,
            this, &MainWindow::onConnectionEstablished);
    connect(&TcpClient::instance(), &TcpClient::connectionClosed,
            this, &MainWindow::onConnectionClosed);
    connect(&TcpClient::instance(), &TcpClient::errorOccurred,
            this, &MainWindow::onErrorOccurred);
    
    // 初始状态
    ui->chatTitleLabel->setText(QString::fromUtf8("选择好友开始聊天"));
    ui->messageBrowser->setHtml("<html><body style='background-color:#f5f5f5; color:#999; text-align:center; padding:50px;'><h3>欢迎使用IMSystem</h3><p>请从左侧选择好友开始聊天</p></body></html>");
    ui->sendBtn->setEnabled(false);
    ui->messageInput->setEnabled(false);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setUsername(const QString &username)
{
    m_username = username;
    ui->userAccountLabel->setText(QString::fromUtf8("当前用户：%1").arg(username));
    setWindowTitle(QString::fromUtf8("IMSystem - %1").arg(username));
    
    // 登录成功后请求好友列表
    requestFriendList();
}

void MainWindow::initFriendList()
{
    ui->friendList->setIconSize(QSize(40, 40));
    ui->friendList->setSpacing(2);
}

void MainWindow::onFriendClicked(QListWidgetItem *item)
{
    if (!item) return;
    
    QString nickname = item->data(Qt::UserRole).toString();
    QString status = item->data(Qt::UserRole + 1).toString();
    
    currentChatFriend = nickname;
    ui->chatTitleLabel->setText(QString("%1 (%2)").arg(nickname, status));
    ui->messageBrowser->clear();
    
    // 启用发送功能
    ui->sendBtn->setEnabled(true);
    ui->messageInput->setEnabled(true);
    
    // 请求历史消息
    requestChatHistory(nickname);
}

void MainWindow::onSendClicked()
{
    if (currentChatFriend.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("请先选择一个好友"));
        return;
    }
    
    QString message = ui->messageInput->toPlainText().trimmed();
    if (message.isEmpty()) {
        return;
    }
    
    // 发送消息到服务端
    Message msg(MessageType::MSG_TEXT);
    msg.setSequence(1);
    
    QJsonObject body;
    body["receiver"] = currentChatFriend;
    body["content"] = message;
    msg.setJsonBody(body);
    
    TcpClient::instance().sendMessage(msg);
    
    // 本地显示消息（自己发送的在右边，绿色背景）
    appendMessage(m_username, message, true);
    ui->messageInput->clear();
}

void MainWindow::onSearchTextChanged(const QString &text)
{
    for (int i = 0; i < ui->friendList->count(); ++i) {
        QListWidgetItem *item = ui->friendList->item(i);
        QString nickname = item->data(Qt::UserRole).toString();
        bool visible = text.isEmpty() || nickname.contains(text, Qt::CaseInsensitive);
        item->setHidden(!visible);
    }
}

void MainWindow::on_actionExit_triggered()
{
    QApplication::quit();
}

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(this, QString::fromUtf8("关于 IMSystem"), 
        QString::fromUtf8("IMSystem 即时通讯系统 v1.0\n\n"
        "基于 Qt 6 + C++17 开发\n"
        "支持好友聊天、群聊等功能"));
}

// ========== 好友功能 ==========

void MainWindow::onAddFriendClicked()
{
    showAddFriendDialog();
}

void MainWindow::onRefreshFriendsClicked()
{
    requestFriendList();
}

void MainWindow::onFriendRequestsClicked()
{
    showFriendRequestsDialog();
}

void MainWindow::onDeleteFriendClicked()
{
    if (currentChatFriend.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("请先选择要删除的好友"));
        return;
    }
    
    QMessageBox::StandardButton reply = QMessageBox::question(this, 
        QString::fromUtf8("确认删除"), 
        QString::fromUtf8("确定要删除好友 %1 吗？\n删除后将同时删除聊天记录。").arg(currentChatFriend),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        Message msg(MessageType::REQ_DELETE_FRIEND);
        msg.setSequence(1);
        
        QJsonObject body;
        body["username"] = currentChatFriend;
        msg.setJsonBody(body);
        
        TcpClient::instance().sendMessage(msg);
    }
}

void MainWindow::onMessageReceived(const Message &msg)
{
    switch (msg.type()) {
    case MessageType::RSP_FRIEND_LIST:
        handleFriendListResponse(msg.jsonBody());
        break;
    case MessageType::RSP_ADD_FRIEND:
        handleAddFriendResponse(msg.jsonBody());
        break;
    case MessageType::RSP_SEARCH_USER:
        handleSearchUserResponse(msg.jsonBody());
        break;
    case MessageType::RSP_ACCEPT_FRIEND:
        handleAcceptFriendResponse(msg.jsonBody());
        break;
    case MessageType::RSP_REJECT_FRIEND:
        handleRejectFriendResponse(msg.jsonBody());
        break;
    case MessageType::RSP_DELETE_FRIEND:
        handleDeleteFriendResponse(msg.jsonBody());
        break;
    case MessageType::MSG_TEXT:
        handleTextMessageReceived(msg.jsonBody());
        break;
    case MessageType::MSG_ACK:
        handleMessageAckResponse(msg.jsonBody());
        break;
    case MessageType::MSG_HISTORY:
        handleHistoryResponse(msg.jsonBody());
        break;
    default:
        break;
    }
}

void MainWindow::onConnectionEstablished()
{
    qInfo() << "MainWindow: 连接已建立";
}

void MainWindow::onConnectionClosed()
{
    qInfo() << "MainWindow: 连接已关闭";
}

void MainWindow::onErrorOccurred(const QString &error)
{
    qWarning() << "MainWindow: 连接错误:" << error;
}

// 请求好友列表
void MainWindow::requestFriendList()
{
    Message msg(MessageType::REQ_FRIEND_LIST);
    msg.setSequence(1);
    msg.setJsonBody(QJsonObject());
    TcpClient::instance().sendMessage(msg);
}

// 请求待处理的好友请求
void MainWindow::requestPendingFriendRequests()
{
    Message msg(MessageType::REQ_PENDING_REQUESTS);
    msg.setSequence(1);
    msg.setJsonBody(QJsonObject());
    TcpClient::instance().sendMessage(msg);
}

// 请求聊天历史
void MainWindow::requestChatHistory(const QString &friendUsername)
{
    Message msg(MessageType::MSG_HISTORY);
    msg.setSequence(1);
    
    QJsonObject body;
    body["username"] = friendUsername;
    body["limit"] = 50;
    body["offset"] = 0;
    msg.setJsonBody(body);
    
    TcpClient::instance().sendMessage(msg);
}

// 处理好友列表响应
void MainWindow::handleFriendListResponse(const QJsonObject &body)
{
    bool success = body["success"].toBool();
    if (!success) {
        QString message = body["message"].toString();
        qWarning() << "获取好友列表失败:" << message;
        return;
    }
    
    // 检查是否有requests字段（待处理的好友请求）
    if (body.contains("requests")) {
        QJsonArray requests = body["requests"].toArray();
        showPendingRequestsDialog(requests);
        return;
    }
    
    // 否则是好友列表
    QJsonArray friends = body["friends"].toArray();
    updateFriendList(friends);
}

// 处理添加好友响应
void MainWindow::handleAddFriendResponse(const QJsonObject &body)
{
    bool success = body["success"].toBool();
    QString message = body["message"].toString();
    
    if (success) {
        QMessageBox::information(this, QString::fromUtf8("成功"), message);
        requestFriendList(); // 刷新好友列表
    } else {
        QMessageBox::warning(this, QString::fromUtf8("失败"), message);
    }
}

// 处理搜索用户响应
void MainWindow::handleSearchUserResponse(const QJsonObject &body)
{
    bool success = body["success"].toBool();
    if (!success) {
        QString message = body["message"].toString();
        QMessageBox::warning(this, QString::fromUtf8("搜索失败"), message);
        return;
    }
    
    QJsonArray users = body["users"].toArray();
    if (users.isEmpty()) {
        QMessageBox::information(this, QString::fromUtf8("搜索结果"), QString::fromUtf8("未找到相关用户"));
        return;
    }
    
    // 显示搜索结果
    QString resultText;
    for (const QJsonValue &value : users) {
        QJsonObject user = value.toObject();
        QString username = user["username"].toString();
        QString nickname = user["nickname"].toString();
        int status = user["status"].toInt();
        
        resultText += QString("%1 (%2) - %3\n")
            .arg(username)
            .arg(nickname.isEmpty() ? username : nickname)
            .arg(status == 1 ? "在线" : "离线");
    }
    
    QMessageBox::information(this, QString::fromUtf8("搜索结果"), resultText);
}

// 处理接受好友响应
void MainWindow::handleAcceptFriendResponse(const QJsonObject &body)
{
    bool success = body["success"].toBool();
    QString message = body["message"].toString();
    
    if (success) {
        QMessageBox::information(this, QString::fromUtf8("成功"), message);
        requestFriendList(); // 刷新好友列表
    } else {
        QMessageBox::warning(this, QString::fromUtf8("失败"), message);
    }
}

// 处理拒绝好友响应
void MainWindow::handleRejectFriendResponse(const QJsonObject &body)
{
    bool success = body["success"].toBool();
    QString message = body["message"].toString();
    
    if (success) {
        QMessageBox::information(this, QString::fromUtf8("成功"), message);
    } else {
        QMessageBox::warning(this, QString::fromUtf8("失败"), message);
    }
}

// 处理删除好友响应
void MainWindow::handleDeleteFriendResponse(const QJsonObject &body)
{
    bool success = body["success"].toBool();
    QString message = body["message"].toString();
    
    if (success) {
        QMessageBox::information(this, QString::fromUtf8("成功"), message);
        
        // 清空聊天窗口
        currentChatFriend.clear();
        ui->chatTitleLabel->setText(QString::fromUtf8("选择好友开始聊天"));
        ui->messageBrowser->clear();
        ui->sendBtn->setEnabled(false);
        ui->messageInput->setEnabled(false);
        
        requestFriendList(); // 刷新好友列表
    } else {
        QMessageBox::warning(this, QString::fromUtf8("失败"), message);
    }
}

// 处理接收到的文本消息
void MainWindow::handleTextMessageReceived(const QJsonObject &body)
{
    QString sender = body["sender"].toString();
    QString content = body["content"].toString();
    
    // 如果当前正在和发送者聊天，直接显示（好友发送的在左边）
    if (sender == currentChatFriend) {
        appendMessage(sender, content, false);
    } else {
        // 显示未读消息提示
        qInfo() << "收到消息，但当前聊天对象不是发送者:" << sender;
        QMessageBox::information(this, QString::fromUtf8("新消息"), 
            QString::fromUtf8("收到 %1 的消息").arg(sender));
    }
}

// 处理消息确认响应
void MainWindow::handleMessageAckResponse(const QJsonObject &body)
{
    bool success = body["success"].toBool();
    if (!success) {
        QString message = body["message"].toString();
        qWarning() << "消息发送失败:" << message;
        QMessageBox::warning(this, QString::fromUtf8("发送失败"), message);
    }
}

// 处理历史消息响应
void MainWindow::handleHistoryResponse(const QJsonObject &body)
{
    bool success = body["success"].toBool();
    if (!success) {
        QString message = body["message"].toString();
        qWarning() << "获取历史消息失败:" << message;
        return;
    }
    
    QJsonArray messages = body["messages"].toArray();
    
    // 显示历史消息（按时间顺序）
    for (int i = messages.size() - 1; i >= 0; --i) {
        QJsonObject msgObj = messages[i].toObject();
        QString senderName = msgObj["sender_name"].toString();
        QString content = msgObj["content"].toString();
        bool isSelf = (senderName == m_username);
        
        appendMessage(senderName, content, isSelf);
    }
}

// 更新好友列表
void MainWindow::updateFriendList(const QJsonArray &friends)
{
    ui->friendList->clear();
    
    for (const QJsonValue &value : friends) {
        QJsonObject friendObj = value.toObject();
        QString username = friendObj["username"].toString();
        QString nickname = friendObj["nickname"].toString();
        int status = friendObj["status"].toInt();
        
        QListWidgetItem *item = new QListWidgetItem(ui->friendList);
        item->setData(Qt::UserRole, username);
        item->setData(Qt::UserRole + 1, status == 1 ? "在线" : "离线");
        
        // 使用纯文本显示，避免HTML标签显示问题
        QString displayName = nickname.isEmpty() ? username : nickname;
        QString statusText = status == 1 ? "在线" : "离线";
        item->setText(QString("%1 - %2").arg(displayName, statusText));
        item->setSizeHint(QSize(0, 60));
        
        // 创建头像
        QPixmap avatar(40, 40);
        avatar.fill(QColor(100, 149, 237));
        QPainter painter(&avatar);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 16, QFont::Bold));
        painter.drawText(avatar.rect(), Qt::AlignCenter, displayName.left(1));
        item->setIcon(QIcon(avatar));
    }
}

// 显示添加好友对话框
void MainWindow::showAddFriendDialog()
{
    bool ok;
    QString username = QInputDialog::getText(this, 
        QString::fromUtf8("添加好友"), 
        QString::fromUtf8("请输入用户名："), 
        QLineEdit::Normal, 
        "", 
        &ok);
    
    if (ok && !username.isEmpty()) {
        Message msg(MessageType::REQ_ADD_FRIEND);
        msg.setSequence(1);
        
        QJsonObject body;
        body["username"] = username;
        msg.setJsonBody(body);
        
        TcpClient::instance().sendMessage(msg);
    }
}

// 显示好友请求对话框
void MainWindow::showFriendRequestsDialog()
{
    // 请求待处理的好友请求
    requestPendingFriendRequests();
}

// 显示待处理的好友请求列表
void MainWindow::showPendingRequestsDialog(const QJsonArray &requests)
{
    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromUtf8("好友请求"));
    dialog.setMinimumSize(400, 300);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    
    QLabel *titleLabel = new QLabel(QString::fromUtf8("待处理的好友请求 (%1)").arg(requests.size()));
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; padding: 10px;");
    mainLayout->addWidget(titleLabel);
    
    QListWidget *requestList = new QListWidget();
    requestList->setIconSize(QSize(40, 40));
    
    if (requests.isEmpty()) {
        QLabel *emptyLabel = new QLabel(QString::fromUtf8("暂无待处理的好友请求"));
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet("color: #999; padding: 20px;");
        mainLayout->addWidget(emptyLabel);
    } else {
        for (const QJsonValue &value : requests) {
            QJsonObject request = value.toObject();
            QString username = request["username"].toString();
            QString nickname = request["nickname"].toString();
            QString displayName = nickname.isEmpty() ? username : nickname;
            
            QListWidgetItem *item = new QListWidgetItem(requestList);
            item->setData(Qt::UserRole, username);
            
            // 使用纯文本显示
            item->setText(QString("%1 (%2)").arg(displayName, username));
            item->setSizeHint(QSize(0, 50));
            
            QPixmap avatar(40, 40);
            avatar.fill(QColor(255, 152, 0));
            QPainter painter(&avatar);
            painter.setPen(Qt::white);
            painter.setFont(QFont("Arial", 16, QFont::Bold));
            painter.drawText(avatar.rect(), Qt::AlignCenter, displayName.left(1));
            item->setIcon(QIcon(avatar));
        }
        
        mainLayout->addWidget(requestList);
        
        // 按钮布局
        QHBoxLayout *buttonLayout = new QHBoxLayout();
        
        QPushButton *acceptBtn = new QPushButton(QString::fromUtf8("接受"));
        acceptBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; padding: 8px 16px; border: none; border-radius: 4px; } QPushButton:hover { background-color: #45a049; }");
        
        QPushButton *rejectBtn = new QPushButton(QString::fromUtf8("拒绝"));
        rejectBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; padding: 8px 16px; border: none; border-radius: 4px; } QPushButton:hover { background-color: #d32f2f; }");
        
        QPushButton *closeBtn = new QPushButton(QString::fromUtf8("关闭"));
        closeBtn->setStyleSheet("QPushButton { background-color: #9E9E9E; color: white; padding: 8px 16px; border: none; border-radius: 4px; } QPushButton:hover { background-color: #757575; }");
        
        buttonLayout->addWidget(acceptBtn);
        buttonLayout->addWidget(rejectBtn);
        buttonLayout->addStretch();
        buttonLayout->addWidget(closeBtn);
        
        mainLayout->addLayout(buttonLayout);
        
        // 连接按钮信号
        connect(acceptBtn, &QPushButton::clicked, [&]() {
            QListWidgetItem *currentItem = requestList->currentItem();
            if (!currentItem) {
                QMessageBox::warning(&dialog, QString::fromUtf8("提示"), QString::fromUtf8("请先选择一个好友请求"));
                return;
            }
            
            QString username = currentItem->data(Qt::UserRole).toString();
            
            Message msg(MessageType::REQ_ACCEPT_FRIEND);
            msg.setSequence(1);
            
            QJsonObject body;
            body["username"] = username;
            msg.setJsonBody(body);
            
            TcpClient::instance().sendMessage(msg);
            
            // 从列表中移除
            delete requestList->takeItem(requestList->row(currentItem));
        });
        
        connect(rejectBtn, &QPushButton::clicked, [&]() {
            QListWidgetItem *currentItem = requestList->currentItem();
            if (!currentItem) {
                QMessageBox::warning(&dialog, QString::fromUtf8("提示"), QString::fromUtf8("请先选择一个好友请求"));
                return;
            }
            
            QString username = currentItem->data(Qt::UserRole).toString();
            
            Message msg(MessageType::REQ_REJECT_FRIEND);
            msg.setSequence(1);
            
            QJsonObject body;
            body["username"] = username;
            msg.setJsonBody(body);
            
            TcpClient::instance().sendMessage(msg);
            
            // 从列表中移除
            delete requestList->takeItem(requestList->row(currentItem));
        });
        
        connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    }
    
    dialog.exec();
}

// 添加消息到浏览器
void MainWindow::appendMessage(const QString &nickname, const QString &message, bool isSelf)
{
    QString time = QTime::currentTime().toString("hh:mm");
    
    // 构建HTML消息
    QString html;
    if (isSelf) {
        // 自己发送的消息 - 右对齐，绿色背景
        html = QString(
            "<div style='text-align:right; margin:8px;'>"
            "<span style='font-size:10px; color:#999;'>%1 </span>"
            "<span style='background-color:#95EC69; padding:8px 12px; border-radius:10px; display:inline-block; max-width:70%;'>%2</span>"
            "</div>"
        ).arg(time, message.toHtmlEscaped().replace("\n", "<br>"));
    } else {
        // 好友发送的消息 - 左对齐，白色背景
        html = QString(
            "<div style='text-align:left; margin:8px;'>"
            "<span style='background-color:#FFFFFF; padding:8px 12px; border-radius:10px; display:inline-block; max-width:70%;'>%1</span>"
            "<span style='font-size:10px; color:#999;'> %2</span>"
            "</div>"
        ).arg(message.toHtmlEscaped().replace("\n", "<br>"), time);
    }
    
    ui->messageBrowser->append(html);
    
    ui->messageBrowser->verticalScrollBar()->setValue(
        ui->messageBrowser->verticalScrollBar()->maximum());
}
