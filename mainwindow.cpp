#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QTime>
#include <QPainter>
#include <QScrollBar>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    
    // 初始化好友列表
    initFriendList();
    addTestFriends();
    
    // 连接信号槽
    connect(ui->friendList, &QListWidget::itemClicked, 
            this, &MainWindow::onFriendClicked);
    connect(ui->sendBtn, &QPushButton::clicked, 
            this, &MainWindow::onSendClicked);
    connect(ui->searchEdit, &QLineEdit::textChanged, 
            this, &MainWindow::onSearchTextChanged);
    
    // 初始状态
    ui->chatTitleLabel->setText(QString::fromUtf8("选择好友开始聊天"));
    ui->messageBrowser->setHtml("<html><body style='background-color:#f5f5f5; color:#999; text-align:center; padding:50px;'><h3>欢迎使用IMSystem</h3><p>请从左侧选择好友开始聊天</p></body></html>");
}

MainWindow::~MainWindow()
{
    delete ui;
}

// 设置当前登录用户
void MainWindow::setUsername(const QString &username)
{
    m_username = username;
    ui->userAccountLabel->setText(QString::fromUtf8("当前用户：%1").arg(username));
    setWindowTitle(QString::fromUtf8("IMSystem - %1").arg(username));
}

// 初始化好友列表
void MainWindow::initFriendList()
{
    ui->friendList->setIconSize(QSize(40, 40));
    ui->friendList->setSpacing(2);
}

// 添加测试好友数据
void MainWindow::addTestFriends()
{
    QStringList friends = {
        QString::fromUtf8("张三|在线|今天天气不错啊"),
        QString::fromUtf8("李四|离线|明天见"),
        QString::fromUtf8("王五|在线|收到，谢谢"),
        QString::fromUtf8("赵六|离线|好的"),
        QString::fromUtf8("IMSystem官方|在线|欢迎使用IMSystem")
    };
    
    for (const QString &friendInfo : friends) {
        QStringList parts = friendInfo.split("|");
        if (parts.size() >= 3) {
            QString nickname = parts[0];
            QString status = parts[1];
            QString lastMsg = parts[2];
            
            QListWidgetItem *item = new QListWidgetItem(ui->friendList);
            
            item->setData(Qt::UserRole, nickname);
            item->setData(Qt::UserRole + 1, status);
            
            QString displayText = QString("<b>%1</b> <span style='color:%2;'>● %3</span><br><span style='color:#888; font-size:12px;'>%4</span>")
                .arg(nickname)
                .arg(status == QString::fromUtf8("在线") ? "#4CAF50" : "#999")
                .arg(status)
                .arg(lastMsg);
            
            item->setText(displayText);
            item->setSizeHint(QSize(0, 60));
            
            QPixmap avatar(40, 40);
            avatar.fill(QColor(100, 149, 237));
            QPainter painter(&avatar);
            painter.setPen(Qt::white);
            painter.setFont(QFont("Arial", 16, QFont::Bold));
            painter.drawText(avatar.rect(), Qt::AlignCenter, nickname.left(1));
            item->setIcon(QIcon(avatar));
        }
    }
}

// 好友点击事件
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

// 发送消息
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

// 搜索好友
void MainWindow::onSearchTextChanged(const QString &text)
{
    for (int i = 0; i < ui->friendList->count(); ++i) {
        QListWidgetItem *item = ui->friendList->item(i);
        QString nickname = item->data(Qt::UserRole).toString();
        
        bool visible = text.isEmpty() || nickname.contains(text, Qt::CaseInsensitive);
        item->setHidden(!visible);
    }
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

// 退出
void MainWindow::on_actionExit_triggered()
{
    QApplication::quit();
}

// 关于
void MainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(this, QString::fromUtf8("关于 IMSystem"), 
        QString::fromUtf8("IMSystem 即时通讯系统 v1.0\n\n"
        "基于 Qt 6 + C++17 开发\n"
        "支持好友聊天、群聊等功能"));
}