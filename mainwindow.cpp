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
    appendMessage(nickname, QString::fromUtf8("你好，有什么可以帮你的吗？"), false);
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
    
    appendMessage(m_username, message, true);
    ui->messageInput->clear();
    
    QTimer::singleShot(1000, this, [this, message]() {
        QString reply = QString::fromUtf8("收到你的消息: ") + message;
        appendMessage(currentChatFriend, reply, false);
    });
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
        QString::fromUtf8("确定要删除好友 %1 吗？").arg(currentChatFriend),
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

// 处理好友列表响应
void MainWindow::handleFriendListResponse(const QJsonObject &body)
{
    bool success = body["success"].toBool();
    if (!success) {
        QString message = body["message"].toString();
        qWarning() << "获取好友列表失败:" << message;
        return;
    }
    
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
        currentChatFriend.clear();
        ui->chatTitleLabel->setText(QString::fromUtf8("选择好友开始聊天"));
        requestFriendList(); // 刷新好友列表
    } else {
        QMessageBox::warning(this, QString::fromUtf8("失败"), message);
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
        
        QString displayName = nickname.isEmpty() ? username : nickname;
        QString displayText = QString("<b>%1</b> <span style='color:%2;'>● %3</span>")
            .arg(displayName)
            .arg(status == 1 ? "#4CAF50" : "#999")
            .arg(status == 1 ? "在线" : "离线");
        
        item->setText(displayText);
        item->setSizeHint(QSize(0, 60));
        
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
    // TODO: 实现好友请求列表对话框
    QMessageBox::information(this, QString::fromUtf8("好友请求"), 
        QString::fromUtf8("好友请求功能待实现"));
}

// 添加消息到浏览器
void MainWindow::appendMessage(const QString &nickname, const QString &message, bool isSelf)
{
    QString time = QTime::currentTime().toString("hh:mm");
    QString html = createBubbleHtml(message, isSelf, time);
    ui->messageBrowser->append(html);
    
    ui->messageBrowser->verticalScrollBar()->setValue(
        ui->messageBrowser->verticalScrollBar()->maximum());
}

// 创建气泡HTML
QString MainWindow::createBubbleHtml(const QString &message, bool isSelf, const QString &time)
{
    QString escapedMessage = message.toHtmlEscaped();
    escapedMessage.replace("\n", "<br>");
    
    if (isSelf) {
        return QString(
            "<div style='text-align:right; margin:10px 0;'>"
            "  <span style='color:#888; font-size:11px;'>%1</span>"
            "  <div style='display:inline-block; background-color:#95EC69; color:#000; "
            "       padding:8px 12px; border-radius:8px; max-width:60%; "
            "       text-align:left; word-wrap:break-word;'>"
            "    %2"
            "  </div>"
            "</div>"
        ).arg(time, escapedMessage);
    } else {
        return QString(
            "<div style='text-align:left; margin:10px 0;'>"
            "  <div style='display:inline-block; background-color:#FFFFFF; color:#000; "
            "       padding:8px 12px; border-radius:8px; max-width:60%; "
            "       text-align:left; word-wrap:break-word;'>"
            "    %1"
            "  </div>"
            "  <span style='color:#888; font-size:11px;'> %2</span>"
            "</div>"
        ).arg(escapedMessage, time);
    }
}
