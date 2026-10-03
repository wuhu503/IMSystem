#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "ChatSession.h"
#include "Message.h"
#include "Protocol.h"
#include "Constants.h"
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
#include <QCloseEvent>
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
    
    // 只连会话层的语义信号，协议细节（序列号、心跳、回执）都在 ChatSession 里
    ChatSession &session = ChatSession::instance();
    connect(&session, &ChatSession::connected, this, &MainWindow::onConnectionEstablished);
    connect(&session, &ChatSession::disconnected, this, &MainWindow::onConnectionClosed);
    connect(&session, &ChatSession::transportError, this, &MainWindow::onErrorOccurred);
    connect(&session, &ChatSession::kickedOffline, this, &MainWindow::handleKickedOffline);

    connect(&session, &ChatSession::friendListReceived, this, &MainWindow::handleFriendListResponse);
    connect(&session, &ChatSession::pendingRequestsReceived, this, &MainWindow::handlePendingRequestsResponse);
    connect(&session, &ChatSession::searchResultReceived, this, &MainWindow::handleSearchUserResponse);
    connect(&session, &ChatSession::addFriendResultReceived, this, &MainWindow::handleAddFriendResponse);
    connect(&session, &ChatSession::acceptFriendResultReceived, this, &MainWindow::handleAcceptFriendResponse);
    connect(&session, &ChatSession::rejectFriendResultReceived, this, &MainWindow::handleRejectFriendResponse);
    connect(&session, &ChatSession::deleteFriendResultReceived, this, &MainWindow::handleDeleteFriendResponse);

    connect(&session, &ChatSession::newFriendRequestReceived, this, &MainWindow::handleFriendRequestNotification);
    connect(&session, &ChatSession::friendAcceptedNotification, this, &MainWindow::handleFriendAcceptedNotification);
    connect(&session, &ChatSession::friendRejectedNotification, this, &MainWindow::handleFriendRejectedNotification);
    connect(&session, &ChatSession::friendRemovedNotification, this, &MainWindow::handleFriendRemovedNotification);

    connect(&session, &ChatSession::textReceived, this, &MainWindow::handleTextReceived);
    connect(&session, &ChatSession::textSendSucceeded, this, &MainWindow::handleTextSendSucceeded);
    connect(&session, &ChatSession::textSendFailed, this, &MainWindow::handleTextSendFailed);
    connect(&session, &ChatSession::historyReceived, this, &MainWindow::handleHistoryReceived);
    connect(&session, &ChatSession::historyFailed, this, &MainWindow::handleHistoryFailed);

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
    ChatSession::instance().requestFriendList();

    // 登录后静默拉取一次好友请求，用于点亮红点（不弹列表）
    m_silentPendingFetch = true;
    ChatSession::instance().requestPendingRequests();
}

void MainWindow::initFriendList()
{
    ui->friendList->setIconSize(QSize(40, 40));
    ui->friendList->setSpacing(2);
}

void MainWindow::onFriendClicked(QListWidgetItem *item)
{
    if (!item) return;
    
    const QString nickname = item->data(Qt::UserRole).toString();
    const QString status = item->data(Qt::UserRole + 1).toString();

    currentChatFriend = nickname;
    // 打开会话，清除该好友的未读红点
    m_unreadCounts.remove(nickname);
    refreshFriendItemUnread(item);

    ui->chatTitleLabel->setText(QString("%1 (%2)").arg(nickname, status));
    ui->messageBrowser->clear();
    ui->sendBtn->setEnabled(true);
    ui->messageInput->setEnabled(true);
    
    ChatSession::instance().requestHistory(nickname, IMConstants::kDefaultHistoryLimit, 0);
    ChatSession::instance().markConversationRead(nickname);
}

void MainWindow::onSendClicked()
{
    if (currentChatFriend.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("请先选择一个好友"));
        return;
    }
    
    QString message = ui->messageInput->toPlainText().trimmed();
    if (message.isEmpty()) return;

    if (message.length() > IMConstants::kMaxTextContentLength) {
        QMessageBox::warning(this, QString::fromUtf8("提示"),
                             QString::fromUtf8("消息内容过长，最多 %1 个字符")
                                 .arg(IMConstants::kMaxTextContentLength));
        return;
    }

    // 未连接时不改动输入框，避免消息在界面上凭空消失；
    // 发送成功与否由会话层在服务端确认后回报（见 handleTextSendSucceeded/Failed）
    if (!ChatSession::instance().sendText(currentChatFriend, message)) {
        QMessageBox::warning(this, QString::fromUtf8("提示"),
                             QString::fromUtf8("与服务器的连接已断开，消息未发送"));
        return;
    }

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

void MainWindow::on_actionLogout_triggered()
{
    // 会话层负责上报离线并断开连接，界面只负责回到登录页
    m_loggingOut = true;
    ChatSession::instance().logout();
    emit logoutRequested();
}

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(this, QString::fromUtf8("关于 IMSystem"), 
        QString::fromUtf8("IMSystem 即时通讯系统 v1.0\n基于 Qt 6 + C++17 开发"));
}

void MainWindow::onAddFriendClicked() { showAddFriendDialog(); }
void MainWindow::onRefreshFriendsClicked() { ChatSession::instance().requestFriendList(); }
void MainWindow::onFriendRequestsClicked() { showFriendRequestsDialog(); }

void MainWindow::on_searchUserBtn_clicked()
{
    bool ok = false;
    QString keyword = QInputDialog::getText(this, QString::fromUtf8("搜索用户"),
                                            QString::fromUtf8("请输入用户名关键字："),
                                            QLineEdit::Normal, QString(), &ok);
    if (!ok || keyword.trimmed().isEmpty()) return;

    ChatSession::instance().searchUser(keyword.trimmed());
}

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
        ChatSession::instance().deleteFriend(currentChatFriend);
    }
}

void MainWindow::onConnectionEstablished() { qInfo() << "连接已建立"; }

void MainWindow::onConnectionClosed()
{
    qInfo() << "连接已关闭";
    if (m_loggingOut) {
        return;  // 主动退出登录/关闭窗口，不再弹窗
    }
    QMessageBox::warning(this, QString::fromUtf8("连接断开"),
        QString::fromUtf8("与服务器的连接已断开，请重新登录。"));
    emit logoutRequested();
    close();
}

void MainWindow::onErrorOccurred(const QString &error) { qWarning() << "连接错误:" << error; }

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 关闭窗口前通知服务器下线（尽力而为，不阻塞关闭流程）
    if (ChatSession::instance().isConnected() && !m_loggingOut) {
        m_loggingOut = true;
        ChatSession::instance().logout();
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::handleKickedOffline(const QString &message)
{
    // 账号在别处登录：提示后回到登录页（不再重复弹窗，由会话层保证只发一次）
    m_loggingOut = true;
    QMessageBox::warning(this, QString::fromUtf8("提示"),
                         message.isEmpty() ? QString::fromUtf8("登录已失效") : message);
    emit logoutRequested();
    close();
}

void MainWindow::handleFriendListResponse(const QJsonObject &body)
{
    if (!body["success"].toBool()) {
        QString message = body["message"].toString();
        qWarning() << "获取好友列表失败:" << message;
        QMessageBox::warning(this, QString::fromUtf8("提示"),
                             message.isEmpty() ? QString::fromUtf8("获取好友列表失败") : message);
        return;
    }
    updateFriendList(body["friends"].toArray());
}

void MainWindow::handlePendingRequestsResponse(const QJsonObject &body)
{
    if (!body["success"].toBool()) {
        QString message = body["message"].toString();
        qWarning() << "获取好友请求失败:" << message;
        QMessageBox::warning(this, QString::fromUtf8("提示"),
                             message.isEmpty() ? QString::fromUtf8("获取好友请求失败") : message);
        return;
    }

    if (m_silentPendingFetch) {
        // 静默模式：只更新按钮红点，不弹对话框
        m_silentPendingFetch = false;
        m_unreadFriendRequests = body["requests"].toArray().size();
        updateFriendRequestButton();
        return;
    }
    showPendingRequestsDialog(body["requests"].toArray());
}

void MainWindow::handleAddFriendResponse(const QJsonObject &body)
{
    if (body["success"].toBool()) {
        QMessageBox::information(this, QString::fromUtf8("成功"), body["message"].toString());
        ChatSession::instance().requestFriendList();
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
    
    // 展示搜索结果，选中后直接发送添加好友请求
    QStringList items;
    QStringList usernames;
    for (const QJsonValue &value : users) {
        QJsonObject user = value.toObject();
        QString username = user["username"].toString();
        QString nickname = user["nickname"].toString();
        int status = user["status"].toInt();
        QString display = nickname.isEmpty() ? username : nickname;
        items << QString("%1 (%2) - %3").arg(display, username, status == 1 ? "在线" : "离线");
        usernames << username;
    }

    bool ok = false;
    QString selected = QInputDialog::getItem(this, QString::fromUtf8("搜索结果"),
                                             QString::fromUtf8("选择要添加的用户："),
                                             items, 0, false, &ok);
    if (!ok || selected.isEmpty()) return;

    int index = items.indexOf(selected);
    if (index < 0 || index >= usernames.size()) return;

    ChatSession::instance().addFriend(usernames[index]);
}

void MainWindow::handleAcceptFriendResponse(const QJsonObject &body)
{
    if (body["success"].toBool()) {
        QMessageBox::information(this, QString::fromUtf8("成功"), body["message"].toString());
        ChatSession::instance().requestFriendList();
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
        m_unreadCounts.remove(currentChatFriend);
        resetChatView();
        ChatSession::instance().requestFriendList();
    } else {
        QMessageBox::warning(this, QString::fromUtf8("失败"), body["message"].toString());
    }
}

void MainWindow::handleFriendRequestNotification()
{
    // 收到新的好友请求推送：好友请求按钮点亮红点
    ++m_unreadFriendRequests;
    updateFriendRequestButton();
}

void MainWindow::handleFriendAcceptedNotification()
{
    // 对方已接受好友请求：自动刷新好友列表
    ChatSession::instance().requestFriendList();
}

void MainWindow::handleFriendRejectedNotification(const QString &username)
{
    QMessageBox::information(this, QString::fromUtf8("好友请求"),
        QString::fromUtf8("对方拒绝了您的好友请求%1")
            .arg(username.isEmpty() ? QString()
                                    : QString::fromUtf8("（%1）").arg(username)));
}

void MainWindow::handleFriendRemovedNotification(const QString &username)
{
    // 被解除的正好是当前会话：先关掉会话再刷新列表
    if (!username.isEmpty() && username == currentChatFriend) {
        m_unreadCounts.remove(username);
        resetChatView();
    }

    QMessageBox::information(this, QString::fromUtf8("好友关系变更"),
        QString::fromUtf8("对方解除了与您的好友关系%1")
            .arg(username.isEmpty() ? QString()
                                    : QString::fromUtf8("（%1）").arg(username)));
    ChatSession::instance().requestFriendList();
}

void MainWindow::handleTextReceived(const QString &sender, const QString &content, qint64 timestamp)
{
    if (sender == currentChatFriend) {
        appendMessage(sender, content, false, timestamp);
        // 消息已在当前会话展示，上报已读（会话层内部去抖）
        ChatSession::instance().markConversationRead(sender);
        return;
    }

    // 非当前会话：累计未读，在好友列表显示红点，不再弹窗打扰
    QListWidgetItem *item = findFriendItem(sender);
    if (!item) {
        // 好友列表可能还没刷新（例如刚接受好友请求），先刷新一次；
        // 该消息已在服务端入库，打开会话后仍能从历史记录里看到
        qWarning() << "收到未知好友的消息，已请求刷新好友列表:" << sender;
        ChatSession::instance().requestFriendList();
        return;
    }
    m_unreadCounts[sender] = m_unreadCounts.value(sender, 0) + 1;
    refreshFriendItemUnread(item);
}

void MainWindow::handleTextSendSucceeded(const QString &receiver, const QString &content)
{
    // 确认成功后再上屏；只有目标会话仍是当前会话才显示，避免消息串到别的聊天窗口
    if (receiver == currentChatFriend && !currentChatFriend.isEmpty()) {
        appendMessage(m_username, content, true);
    }
}

void MainWindow::handleTextSendFailed(const QString &receiver, const QString &content,
                                      const QString &message)
{
    QMessageBox::warning(this, QString::fromUtf8("发送失败"), message);
    // 发送失败时把内容还回输入框，避免用户白打一遍（仅在原会话且输入框为空时）
    if (receiver == currentChatFriend && ui->messageInput->toPlainText().isEmpty()) {
        ui->messageInput->setPlainText(content);
    }
}
    
void MainWindow::handleHistoryReceived(const QString &friendUsername, const QJsonArray &messages)
{
    Q_UNUSED(friendUsername);
    for (int i = messages.size() - 1; i >= 0; --i) {
        QJsonObject msgObj = messages[i].toObject();
        QString senderName = msgObj["sender_name"].toString();
        QString content = msgObj["content"].toString();
        qint64 timestamp = msgObj["timestamp"].toVariant().toLongLong();
        bool isSelf = (senderName == m_username);
        appendMessage(senderName, content, isSelf, timestamp);
    }
}

void MainWindow::handleHistoryFailed(const QString &friendUsername, const QString &message)
{
    Q_UNUSED(friendUsername);
    qWarning() << "获取聊天记录失败:" << message;
    QMessageBox::warning(this, QString::fromUtf8("提示"),
                         message.isEmpty() ? QString::fromUtf8("获取聊天记录失败") : message);
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
        item->setData(Qt::UserRole + 2, displayName);
        item->setSizeHint(QSize(0, 60));

        refreshFriendItemUnread(item);
    }

    // 列表重建后重新应用当前的搜索过滤，避免输入框里的关键字被忽略
    applyFriendFilter();
}

void MainWindow::applyFriendFilter()
{
    onSearchTextChanged(ui->searchEdit->text());
}

void MainWindow::resetChatView()
{
    currentChatFriend.clear();
    ui->chatTitleLabel->setText(QString::fromUtf8("选择好友开始聊天"));
    ui->messageBrowser->clear();
    ui->sendBtn->setEnabled(false);
    ui->messageInput->setEnabled(false);
}

QListWidgetItem* MainWindow::findFriendItem(const QString &username) const
{
    for (int i = 0; i < ui->friendList->count(); ++i) {
        QListWidgetItem *item = ui->friendList->item(i);
        if (item->data(Qt::UserRole).toString() == username) {
            return item;
        }
    }
    return nullptr;
}

void MainWindow::refreshFriendItemUnread(QListWidgetItem *item)
{
    if (!item) return;

    QString username = item->data(Qt::UserRole).toString();
    QString status = item->data(Qt::UserRole + 1).toString();
    QString displayName = item->data(Qt::UserRole + 2).toString();
    if (displayName.isEmpty()) {
        displayName = username;
    }

    bool hasUnread = m_unreadCounts.value(username, 0) > 0;
    item->setText(QString("%1 - %2").arg(displayName, status));
    // 红点绘制在头像右下角徽标上，避免在文本里拼 HTML（QListWidgetItem 不支持富文本）
    setFriendItemAvatar(item, displayName, hasUnread);
}

void MainWindow::setFriendItemAvatar(QListWidgetItem *item, const QString &displayName, bool unread)
{
    QPixmap avatar(40, 40);
    avatar.fill(QColor(100, 149, 237));

    {
        QPainter painter(&avatar);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 16, QFont::Bold));
        painter.drawText(avatar.rect(), Qt::AlignCenter, displayName.left(1));
    }

    if (unread) {
        // 未读红点徽标：头像右下角的红色圆点
        QPainter painter(&avatar);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(Qt::white, 1.5));
        painter.setBrush(QColor(0xE5, 0x39, 0x35));
        painter.drawEllipse(QRectF(25.5, 25.5, 12, 12));
    }

    item->setIcon(QIcon(avatar));
}

void MainWindow::updateFriendRequestButton()
{
    if (m_unreadFriendRequests > 0) {
        QPixmap dot(12, 12);
        dot.fill(Qt::transparent);
        QPainter painter(&dot);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0xE5, 0x39, 0x35));
        painter.drawEllipse(0, 0, 12, 12);
        ui->friendRequestsBtn->setIcon(QIcon(dot));
        ui->friendRequestsBtn->setIconSize(QSize(12, 12));
    } else {
        ui->friendRequestsBtn->setIcon(QIcon());
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
        ChatSession::instance().addFriend(username);
    }
}

void MainWindow::showFriendRequestsDialog()
{
    // 手动打开列表即视为已查看：清除静默模式与红点
    m_silentPendingFetch = false;
    m_unreadFriendRequests = 0;
    updateFriendRequestButton();
    ChatSession::instance().requestPendingRequests();
}

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
            ChatSession::instance().acceptFriend(username);
            delete requestList->takeItem(requestList->row(currentItem));
        });
        
        connect(rejectBtn, &QPushButton::clicked, [&]() {
            QListWidgetItem *currentItem = requestList->currentItem();
            if (!currentItem) {
                QMessageBox::warning(&dialog, QString::fromUtf8("提示"), QString::fromUtf8("请先选择一个好友请求"));
                return;
            }
            QString username = currentItem->data(Qt::UserRole).toString();
            ChatSession::instance().rejectFriend(username);
            delete requestList->takeItem(requestList->row(currentItem));
        });
        
        connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    }
    
    dialog.exec();
}

void MainWindow::appendMessage(const QString &nickname, const QString &message, bool isSelf,
                               qint64 timestamp)
{
    Q_UNUSED(nickname);
    QString time;
    if (timestamp > 0) {
        time = QDateTime::fromSecsSinceEpoch(timestamp).toString("MM-dd hh:mm");
    } else {
        time = QTime::currentTime().toString("hh:mm");
    }
    
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

