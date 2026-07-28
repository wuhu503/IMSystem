#include "logindialog.h"
#include "ui_logindialog.h"
#include "registerdialog.h"
#include <QString>
#include <QMessageBox>
#include <QIntValidator>
#include <QDebug>

LoginDialog::LoginDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    setWindowTitle(QString::fromUtf8("IMSystem - 登录"));
    
    // 为端口输入框添加整数验证器
    QIntValidator *portValidator = new QIntValidator(1, 65535, this);
    ui->portEdit->setValidator(portValidator);
    
    connect(&TcpClient::instance(), &TcpClient::connectionEstablished,
            this, &LoginDialog::onConnectionEstablished);
    connect(&TcpClient::instance(), &TcpClient::messageReceived,
            this, &LoginDialog::onMessageReceived);

    connect(&TcpClient::instance(), &TcpClient::errorOccurred,
            this, &LoginDialog::onErrorOccurred);
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

QString LoginDialog::username() const
{
    return m_username;
}

// 登录按钮点击
void LoginDialog::on_loginBtn_clicked()
{
    // 防止重复点击
    if (!ui->loginBtn->isEnabled() || TcpClient::instance().isConnecting()) {
        return;
    }
    
    QString username = ui->usernameEdit->text().trimmed();
    QString password = ui->passwordEdit->text();
    QString server = ui->serverEdit->text().trimmed();
    QString portStr = ui->portEdit->text().trimmed();

    if (server.isEmpty() || portStr.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("服务器地址和端口不能为空"));
        return;
    }

    bool ok;
    quint16 port = portStr.toUShort(&ok);
    if (!ok || port == 0) {
        QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("端口号无效"));
        return;
    }

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("用户名和密码不能为空"));
        return;
    }

    // 保存登录信息
    m_username = username;
    m_password = password;
    m_server = server;
    m_port = port;
    
    // 禁用按钮
    ui->loginBtn->setEnabled(false);
    ui->loginBtn->setText(QString::fromUtf8("连接中..."));
    ui->registerBtn->setEnabled(false);
    
    // 如果已经连接，直接发送登录请求
    if (TcpClient::instance().isConnect()) {
        sendLoginRequest(m_username, m_password);
    } else {
        TcpClient::instance().connectToServer(server, port);
    }
}

// 注册按钮点击
void LoginDialog::on_registerBtn_clicked()
{
    RegisterDialog registerDialog(this);
    connect(&registerDialog, &RegisterDialog::registerSuccess,
            this, &LoginDialog::onRegisterSuccess);
    
    registerDialog.exec();
}

// 连接成功后发送登录请求
void LoginDialog::onConnectionEstablished()
{
    // 使用保存的用户名和密码
    if (!m_username.isEmpty() && !m_password.isEmpty()) {
        sendLoginRequest(m_username, m_password);
    }
}

// 收到服务器响应
void LoginDialog::onMessageReceived(const Message &msg)
{
    if (msg.type() == MessageType::RSP_LOGIN) {
        handleLoginResponse(msg.jsonBody());
    }
}

// 连接错误
void LoginDialog::onErrorOccurred(const QString &error)
{
    ui->loginBtn->setEnabled(true);
    ui->loginBtn->setText(QString::fromUtf8("登录"));
    ui->registerBtn->setEnabled(true);
    
    QMessageBox::critical(this, QString::fromUtf8("连接错误"), 
                          QString::fromUtf8("无法连接服务器: ") + error);
}

// 发送登录请求
void LoginDialog::sendLoginRequest(const QString &username, const QString &password)
{
    Message msg(MessageType::REQ_LOGIN);
    msg.setSequence(1);

    QJsonObject body;
    body["username"] = username;
    body["password"] = password;
    msg.setJsonBody(body);

    TcpClient::instance().sendMessage(msg);
}

// 处理登录响应
void LoginDialog::handleLoginResponse(const QJsonObject &body)
{
    ui->loginBtn->setEnabled(true);
    ui->loginBtn->setText(QString::fromUtf8("登录"));
    ui->registerBtn->setEnabled(true);
    
    bool success = body["success"].toBool();
    QString message = body["message"].toString();

    if (success) {
        // 保存token
        QString token = body["token"].toString();
        if (!token.isEmpty()) {
            TcpClient::instance().setToken(token);
        }
        
        QMessageBox::information(this, QString::fromUtf8("成功"), QString::fromUtf8("登录成功!"));
        accept();
    } else {
        QMessageBox::warning(this, QString::fromUtf8("登录失败"), message);
    }
}

// 处理注册成功
void LoginDialog::onRegisterSuccess(const QString &username)
{
    // 自动填充注册成功的用户名
    ui->usernameEdit->setText(username);
    ui->passwordEdit->setFocus();
}
