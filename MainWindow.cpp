#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "ChatSession.h"
#include "ClientDialogs.h"
#include "Message.h"
#include "Protocol.h"
#include "Constants.h"
#include "UiKit.h"
#include <QMessageBox>
#include <QTime>
#include <QScrollBar>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <QCloseEvent>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QDateTime>
#include <QIcon>
#include <QLineEdit>

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
    
    // 好友与未读状态集中在 FriendStore，界面只做渲染
    connect(&m_friends, &FriendStore::listChanged, this, &MainWindow::renderFriendList);
    connect(&m_friends, &FriendStore::unreadChanged, this, &MainWindow::renderUnreadBadge);

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

    // 新会话：先清掉上一轮登录留下的好友与红点，避免旧数据串到新账号
    m_unreadFriendRequests = 0;
    m_friendListLoaded = false;
    updateFriendRequestButton();
    resetChatView();
    m_friends.setFriends(QJsonArray());

    ui->userAccountLabel->setText(QString::fromUtf8("当前用户：%1").arg(username));
    setWindowTitle(QString::fromUtf8("IMSystem - %1").arg(username));

    // 登录成功的那一刻服务端就会补推离线消息，而这时主窗口还没构造，
    // 会话层先把它们缓存住了，这里取出来补渲染——否则消息既不上屏也不计未读
    const QList<ChatSession::IncomingText> pending =
        ChatSession::instance().takePendingTexts();
    for (const ChatSession::IncomingText &text : pending) {
        handleTextReceived(text.sender, text.content, text.timestamp);
    }

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
    
    const QString username = item->data(Qt::UserRole).toString();
    if (username.isEmpty()) return;

    currentChatFriend = username;
    // 打开会话即清红点（未读变化会顺带重画该项）
    m_friends.clearUnread(username);

    ui->messageBrowser->clear();
    ui->sendBtn->setEnabled(true);
    ui->messageInput->setEnabled(true);
    renderChatHeader();
    
    ChatSession::instance().requestHistory(username, IMConstants::kDefaultHistoryLimit, 0);
    ChatSession::instance().markConversationRead(username);
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
    m_friendListLoaded = true;
    m_friends.setFriends(body["friends"].toArray());
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

    const QJsonArray requests = body["requests"].toArray();

    if (m_silentPendingFetch) {
        // 静默模式：只更新按钮红点，不弹对话框
        m_silentPendingFetch = false;
        m_unreadFriendRequests = requests.size();
        updateFriendRequestButton();
        return;
    }

    // 非静默说明是用户主动打开的，视为已查看：红点清零并弹出请求列表
    m_unreadFriendRequests = 0;
    updateFriendRequestButton();
    ClientDialogs::showFriendRequests(requests, this);
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
    
    // 结果展示与选择逻辑在 ClientDialogs 里，这里只负责把选中的用户发出去
    const QString username = ClientDialogs::pickSearchResult(users, this);
    if (!username.isEmpty()) {
        ChatSession::instance().addFriend(username);
    }
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
        const QString removed = currentChatFriend;
        resetChatView();
        m_friends.clearUnread(removed);
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
        resetChatView();
    }
    m_friends.clearUnread(username);

    QMessageBox::information(this, QString::fromUtf8("好友关系变更"),
        QString::fromUtf8("对方解除了与您的好友关系%1")
            .arg(username.isEmpty() ? QString()
                                    : QString::fromUtf8("（%1）").arg(username)));
    ChatSession::instance().requestFriendList();
}

void MainWindow::handleTextReceived(const QString &sender, const QString &content, qint64 timestamp)
{
    if (sender.isEmpty()) {
        return;
    }

    if (sender == currentChatFriend) {
        appendMessage(content, false, timestamp);
        // 消息已在当前会话展示，上报已读（会话层内部去抖）
        ChatSession::instance().markConversationRead(sender);
        return;
    }

    // 非当前会话：一律先记未读——即使好友列表还没就绪也照记，
    // 红点由 FriendStore 统一渲染，列表到齐后自然补齐，不再依赖"此刻 item 是否存在"
    m_friends.addUnread(sender);

    if (m_friendListLoaded && !m_friends.contains(sender)) {
        // 列表已经加载过却没有这个人（例如刚接受好友请求），拉一次列表补上；
        // 列表还没加载时不做任何事——setUsername 里那次请求已经带着未读数一起刷新了
        qWarning() << "收到未知好友的消息，已请求刷新好友列表:" << sender;
        ChatSession::instance().requestFriendList();
    }
}

void MainWindow::handleTextSendSucceeded(const QString &receiver, const QString &content)
{
    // 确认成功后再上屏；只有目标会话仍是当前会话才显示，避免消息串到别的聊天窗口
    if (receiver == currentChatFriend && !currentChatFriend.isEmpty()) {
        appendMessage(content, true);
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
    // 会话层已按序列号丢弃过期响应，这里再兜一层：只渲染当前会话的记录，
    // 避免切换会话后旧响应把别人的聊天记录画进来
    if (friendUsername != currentChatFriend) {
        return;
    }

    // 服务端按时间倒序返回，反向追加即为正序
    for (int i = messages.size() - 1; i >= 0; --i) {
        const QJsonObject msgObj = messages.at(i).toObject();
        appendMessage(msgObj["content"].toString(),
                      msgObj["sender_name"].toString() == m_username,
                      msgObj["timestamp"].toVariant().toLongLong());
    }
}

void MainWindow::handleHistoryFailed(const QString &friendUsername, const QString &message)
{
    Q_UNUSED(friendUsername);
    qWarning() << "获取聊天记录失败:" << message;
    QMessageBox::warning(this, QString::fromUtf8("提示"),
                         message.isEmpty() ? QString::fromUtf8("获取聊天记录失败") : message);
}

void MainWindow::renderFriendList()
{
    ui->friendList->clear();

    for (const FriendInfo &info : m_friends.friends()) {
        QListWidgetItem *item = new QListWidgetItem(ui->friendList);
        item->setData(Qt::UserRole, info.username);
        item->setSizeHint(QSize(0, 60));
        item->setText(QStringLiteral("%1 - %2").arg(info.displayName(), info.statusText()));
        item->setIcon(QIcon(UiKit::avatar(info.displayName(), UiKit::kAvatarBlue,
                                          m_friends.hasUnread(info.username))));
    }

    // 列表重建后重新应用搜索过滤，并同步当前会话标题（在线状态不再停留在旧值）
    applyFriendFilter();
    renderChatHeader();
}

void MainWindow::renderUnreadBadge(const QString &username)
{
    QListWidgetItem *item = findFriendItem(username);
    if (!item) {
        return;  // 列表里还没有这个人：等列表刷新时按未读数统一渲染
    }

    const FriendInfo info = m_friends.find(username);
    item->setIcon(QIcon(UiKit::avatar(info.displayName(), UiKit::kAvatarBlue,
                                      m_friends.hasUnread(username))));
}

void MainWindow::renderChatHeader()
{
    if (currentChatFriend.isEmpty()) {
        ui->chatTitleLabel->setText(QString::fromUtf8("选择好友开始聊天"));
        return;
    }

    const FriendInfo info = m_friends.find(currentChatFriend);
    if (!info.isValid()) {
        // 好友已被删除、或列表里已经没有这个人：收掉会话，不留过期标题
        resetChatView();
        return;
    }

    ui->chatTitleLabel->setText(QStringLiteral("%1 (%2)")
                                    .arg(info.displayName(), info.statusText()));
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

void MainWindow::updateFriendRequestButton()
{
    if (m_unreadFriendRequests > 0) {
        ui->friendRequestsBtn->setIcon(QIcon(UiKit::dot(12)));
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

void MainWindow::appendMessage(const QString &text, bool isSelf, qint64 timestamp)
{
    const QString timeText = timestamp > 0
        ? QDateTime::fromSecsSinceEpoch(timestamp).toString(QStringLiteral("MM-dd hh:mm"))
        : QTime::currentTime().toString(QStringLiteral("hh:mm"));

    // 不能用 QTextBrowser::append()：它会把上一段的块格式复制给新段落，
    // 于是只要第一条消息是右对齐，后面收到的消息也会跟着右对齐
    //（写在 HTML 里的 text-align 从第二条消息起就不再生效）。
    // 这里自己建段并显式设置对齐，让每条消息的左右只由 isSelf 决定。
    QTextDocument *doc = ui->messageBrowser->document();
    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::End);
    if (!doc->isEmpty()) {
        cursor.insertBlock();
    }

    cursor.insertHtml(UiKit::messageBubble(text, isSelf, timeText));

    QTextBlockFormat format;
    format.setAlignment(isSelf ? Qt::AlignRight : Qt::AlignLeft);
    format.setTopMargin(6);
    format.setBottomMargin(6);
    cursor.setBlockFormat(format);

    ui->messageBrowser->verticalScrollBar()->setValue(
        ui->messageBrowser->verticalScrollBar()->maximum());
}

