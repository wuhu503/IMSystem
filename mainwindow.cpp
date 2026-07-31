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
    
    initFriendList();
    
    connect(ui->friendList, &QListWidget::itemClicked, 
            this, &MainWindow::onFriendClicked);
    connect(ui->sendBtn, &QPushButton::clicked, 
            this, &MainWindow::onSendClicked);
    connect(ui->searchEdit, &QLineEdit::textChanged, 
            this, &MainWindow::onSearchTextChanged);
    connect(ui->addFriendBtn, &QPushButton::clicked, 
            this, &MainWindow::onAddFriendClicked);
    connect(ui->refreshFriendsBtn, &QPushButton::clicked, 
            this, &MainWindow::onRefreshFriendsClicked);
    connect(ui->friendRequestsBtn, &QPushButton::clicked, 
            this, &MainWindow::onFriendRequestsClicked);
    connect(ui->deleteFriendBtn, &QPushButton::clicked, 
            this, &MainWindow::onDeleteFriendClicked);
    
    connect(&TcpClient::instance(), &TcpClient::messageReceived,
            this, &MainWindow::onMessageReceived);
    connect(&TcpClient::instance(), &TcpClient::connectionEstablished,
            this, &MainWindow::onConnectionEstablished);
    connect(&TcpClient::instance(), &TcpClient::connectionClosed,
            this, &MainWindow::onConnectionClosed);
    connect(&TcpClient::instance(), &TcpClient::errorOccurred,
            this, &MainWindow::onErrorOccurred);
    
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
    ui->sendBtn->setEnabled(true);
    ui->messageInput->setEnabled(true);
    
    requestChatHistory(nickname);
}

void MainWindow::onSendClicked()
{
    if (currentChatFriend.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("请先选择一个好友"));
        return;
    }
    
    QString message = ui->messageInput->toPlainText().trimmed();
    if (message.isEmpty()) return;
    
    Message msg(MessageType::MSG_TEXT);
    msg.setSequence(m_sequenceCounter.fetch_add(1));
    
    QJsonObject body;
    body["receiver"] = currentChatFriend;
    body["content"] = message;
    msg.setJsonBody(body);
    
    TcpClient::instance().sendMessage(msg);
    
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

void MainWindow::on_actionExit_triggered() { QApplication::quit(); }

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(this, QString::fromUtf8("关于 IMSystem"), 
        QString::fromUtf8("IMSystem 即时通讯系统 v1.0\n基于 Qt 6 + C++17 开发"));
}

void MainWindow::onAddFriendClicked() { showAddFriendDialog(); }
void MainWindow::onRefreshFriendsClicked() { requestFriendList(); }
void MainWindow::onFriendRequestsClicked() { showFriendRequestsDialog(); }

void MainWindow::onDeleteFriendClicked()
{
    if (currentChatFriend.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("请先选择要删除的好友"));
        return;
    }
    
    QMessageBox::StandardButton reply = QMessageBox::question(this, 
        QString::fromUtf8("确认删除"), 
        QString::fromUtf8("确定要删除好友 %1 吗？删除后将同时删除聊天记录。").arg(currentChatFriend),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        Message msg(MessageType::REQ_DELETE_FRIEND);
        msg.setSequence(m_sequenceCounter.fetch_add(1));
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
    case MessageType::RSP_PENDING_REQUESTS:
        handlePendingRequestsResponse(msg.jsonBody());
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
    case MessageType::HEARTBEAT:
        handleHeartbeat(msg.jsonBody());
        break;
    default:
        break;
    }
}

void MainWindow::onConnectionEstablished() { qInfo() << "连接已建立"; }

void MainWindow::onConnectionClosed()
{
    qInfo() << "连接已关闭";
    QMessageBox::warning(this, QString::fromUtf8("连接断开"), 
        QString::fromUtf8("与服务器的连接已断开，请重新登录。"));
    QApplication::quit();
}

void MainWindow::onErrorOccurred(const QString &error) { qWarning() << "连接错误:" << error; }

void MainWindow::requestFriendList()
{
    Message msg(MessageType::REQ_FRIEND_LIST);
    msg.setSequence(m_sequenceCounter.fetch_add(1));
    msg.setJsonBody(QJsonObject());
    TcpClient::instance().sendMessage(msg);
}

void MainWindow::requestPendingFriendRequests()
{
    Message msg(MessageType::REQ_PENDING_REQUESTS);
    msg.setSequence(m_sequenceCounter.fetch_add(1));
    msg.setJsonBody(QJsonObject());
    TcpClient::instance().sendMessage(msg);
}

void MainWindow::requestChatHistory(const QString &friendUsername)
{
    Message msg(MessageType::MSG_HISTORY);
    msg.setSequence(m_sequenceCounter.fetch_add(1));
    QJsonObject body;
    body["username"] = friendUsername;
    body["limit"] = 50;
    body["offset"] = 0;
    msg.setJsonBody(body);
    TcpClient::instance().sendMessage(msg);
}

void MainWindow::handleHeartbeat(const QJsonObject &body)
{
    QString message = body["message"].toString();
    if (message.contains("其他地方登录") || body["type"].toString() == "kicked") {
        QMessageBox::warning(this, QString::fromUtf8("提示"), message);
        TcpClient::instance().disconnectToServer();
        QApplication::quit();
    }
}

void MainWindow::handleFriendListResponse(const QJsonObject &body)
{
    if (!body["success"].toBool()) return;
    updateFriendList(body["friends"].toArray());
}

void MainWindow::handlePendingRequestsResponse(const QJsonObject &body)
{
    if (!body["success"].toBool()) return;
    showPendingRequestsDialog(body["requests"].toArray());
}

void MainWindow::handleAddFriendResponse(const QJsonObject &body)
{
    if (body["success"].toBool()) {
        QMessageBox::information(this, QString::fromUtf8("成功"), body["message"].toString());
        requestFriendList();
    } else {
        QMessageBox::warning(this, QString::fromUtf8("失败"), body["message"].toString());
    }
}

void MainWindow::handleSearchUserResponse(const QJsonObject &body)
{
    if (!body["success"].toBool()) {
        QMessageBox::warning(this, QString::fromUtf8("搜索失败"), body["message"].toString());
        return;
    }
    
    QJsonArray users = body["users"].toArray();
    if (users.isEmpty()) {
        QMessageBox::information(this, QString::fromUtf8("搜索结果"), QString::fromUtf8("未找到相关用户"));
        return;
    }
    
    QString resultText;
    for (const QJsonValue &value : users) {
        QJsonObject user = value.toObject();
        QString username = user["username"].toString();
        QString nickname = user["nickname"].toString();
        int status = user["status"].toInt();
        resultText += QString("%1 (%2) - %3\n")
            .arg(username, nickname.isEmpty() ? username : nickname)
            .arg(status == 1 ? "在线" : "离线");
    }
    QMessageBox::information(this, QString::fromUtf8("搜索结果"), resultText);
}

void MainWindow::handleAcceptFriendResponse(const QJsonObject &body)
{
    if (body["success"].toBool()) {
        QMessageBox::information(this, QString::fromUtf8("成功"), body["message"].toString());
        requestFriendList();
    } else {
        QMessageBox::warning(this, QString::fromUtf8("失败"), body["message"].toString());
    }
}

void MainWindow::handleRejectFriendResponse(const QJsonObject &body)
{
    if (body["success"].toBool()) {
        QMessageBox::information(this, QString::fromUtf8("成功"), body["message"].toString());
    } else {
        QMessageBox::warning(this, QString::fromUtf8("失败"), body["message"].toString());
    }
}

void MainWindow::handleDeleteFriendResponse(const QJsonObject &body)
{
    if (body["success"].toBool()) {
        QMessageBox::information(this, QString::fromUtf8("成功"), body["message"].toString());
        currentChatFriend.clear();
        ui->chatTitleLabel->setText(QString::fromUtf8("选择好友开始聊天"));
        ui->messageBrowser->clear();
        ui->sendBtn->setEnabled(false);
        ui->messageInput->setEnabled(false);
        requestFriendList();
    } else {
        QMessageBox::warning(this, QString::fromUtf8("失败"), body["message"].toString());
    }
}

void MainWindow::handleTextMessageReceived(const QJsonObject &body)
{
    QString sender = body["sender"].toString();
    QString content = body["content"].toString();
    
    if (sender == currentChatFriend) {
        appendMessage(sender, content, false);
    } else {
        QMessageBox::information(this, QString::fromUtf8("新消息"), 
            QString::fromUtf8("收到 %1 的消息: %2").arg(sender, content.left(50)));
    }
}

void MainWindow::handleMessageAckResponse(const QJsonObject &body)
{
    if (!body["success"].toBool()) {
        QMessageBox::warning(this, QString::fromUtf8("发送失败"), body["message"].toString());
    }
}

void MainWindow::handleHistoryResponse(const QJsonObject &body)
{
    if (!body["success"].toBool()) return;
    
    QJsonArray messages = body["messages"].toArray();
    for (int i = messages.size() - 1; i >= 0; --i) {
        QJsonObject msgObj = messages[i].toObject();
        QString senderName = msgObj["sender_name"].toString();
        QString content = msgObj["content"].toString();
        bool isSelf = (senderName == m_username);
        appendMessage(senderName, content, isSelf);
    }
}

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
        item->setText(QString("%1 - %2").arg(displayName, status == 1 ? "在线" : "离线"));
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

void MainWindow::showAddFriendDialog()
{
    bool ok;
    QString username = QInputDialog::getText(this, 
        QString::fromUtf8("添加好友"), 
        QString::fromUtf8("请输入用户名："), 
        QLineEdit::Normal, "", &ok);
    
    if (ok && !username.isEmpty()) {
        Message msg(MessageType::REQ_ADD_FRIEND);
        msg.setSequence(m_sequenceCounter.fetch_add(1));
        QJsonObject body;
        body["username"] = username;
        msg.setJsonBody(body);
        TcpClient::instance().sendMessage(msg);
    }
}

void MainWindow::showFriendRequestsDialog() { requestPendingFriendRequests(); }

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
        
        QHBoxLayout *buttonLayout = new QHBoxLayout();
        
        QPushButton *acceptBtn = new QPushButton(QString::fromUtf8("接受"));
        acceptBtn->setStyleSheet("background-color: #4CAF50; color: white; padding: 8px 16px;");
        
        QPushButton *rejectBtn = new QPushButton(QString::fromUtf8("拒绝"));
        rejectBtn->setStyleSheet("background-color: #f44336; color: white; padding: 8px 16px;");
        
        QPushButton *closeBtn = new QPushButton(QString::fromUtf8("关闭"));
        closeBtn->setStyleSheet("background-color: #9E9E9E; color: white; padding: 8px 16px;");
        
        buttonLayout->addWidget(acceptBtn);
        buttonLayout->addWidget(rejectBtn);
        buttonLayout->addStretch();
        buttonLayout->addWidget(closeBtn);
        mainLayout->addLayout(buttonLayout);
        
        connect(acceptBtn, &QPushButton::clicked, [&]() {
            QListWidgetItem *currentItem = requestList->currentItem();
            if (!currentItem) {
                QMessageBox::warning(&dialog, QString::fromUtf8("提示"), QString::fromUtf8("请先选择一个好友请求"));
                return;
            }
            QString username = currentItem->data(Qt::UserRole).toString();
            Message msg(MessageType::REQ_ACCEPT_FRIEND);
            msg.setSequence(m_sequenceCounter.fetch_add(1));
            QJsonObject body;
            body["username"] = username;
            msg.setJsonBody(body);
            TcpClient::instance().sendMessage(msg);
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
            msg.setSequence(m_sequenceCounter.fetch_add(1));
            QJsonObject body;
            body["username"] = username;
            msg.setJsonBody(body);
            TcpClient::instance().sendMessage(msg);
            delete requestList->takeItem(requestList->row(currentItem));
        });
        
        connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    }
    
    dialog.exec();
}

void MainWindow::appendMessage(const QString &nickname, const QString &message, bool isSelf)
{
    Q_UNUSED(nickname);
    QString time = QTime::currentTime().toString("hh:mm");
    
    QString html;
    if (isSelf) {
        html = QString(
            "<div style='text-align:right; margin:8px;'>"
            "<span style='font-size:10px; color:#999;'>%1 </span>"
            "<span style='background-color:#95EC69; padding:8px 12px; border-radius:10px; display:inline-block; max-width:70%;'>%2</span>"
            "</div>"
        ).arg(time, message.toHtmlEscaped().replace("\n", "<br>"));
    } else {
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

