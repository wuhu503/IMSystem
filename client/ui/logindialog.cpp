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

void LoginDialog::on_loginBtn_clicked()
{
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

    m_username = username;
    m_password = password;
    m_server = server;
    m_port = port;
    
    ui->loginBtn->setEnabled(false);
    ui->loginBtn->setText(QString::fromUtf8("连接中..."));
    ui->registerBtn->setEnabled(false);
    
    if (TcpClient::instance().isConnect()) {
        sendLoginRequest(m_username, m_password);
    } else {
        TcpClient::instance().connectToServer(server, port);
    }
}

void LoginDialog::on_registerBtn_clicked()
{
    RegisterDialog registerDialog(this);
    connect(&registerDialog, &RegisterDialog::registerSuccess,
            this, &LoginDialog::onRegisterSuccess);
    registerDialog.exec();
}

void LoginDialog::onConnectionEstablished()
{
    if (!m_username.isEmpty() && !m_password.isEmpty()) {
        sendLoginRequest(m_username, m_password);
    }
}

void LoginDialog::onMessageReceived(const Message &msg)
{
    if (msg.type() == MessageType::RSP_LOGIN) {
        handleLoginResponse(msg.jsonBody());
    }
}

void LoginDialog::onErrorOccurred(const QString &error)
{
    ui->loginBtn->setEnabled(true);
    ui->loginBtn->setText(QString::fromUtf8("登录"));
    ui->registerBtn->setEnabled(true);
    
    QMessageBox::critical(this, QString::fromUtf8("连接错误"), 
                          QString::fromUtf8("无法连接服务器: ") + error);
}

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

void LoginDialog::handleLoginResponse(const QJsonObject &body)
{
    ui->loginBtn->setEnabled(true);
    ui->loginBtn->setText(QString::fromUtf8("登录"));
    ui->registerBtn->setEnabled(true);
    
    bool success = body["success"].toBool();
    QString message = body["message"].toString();

    if (success) {
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

void LoginDialog::onRegisterSuccess(const QString &username)
{
    ui->usernameEdit->setText(username);
    ui->passwordEdit->setFocus();
}
