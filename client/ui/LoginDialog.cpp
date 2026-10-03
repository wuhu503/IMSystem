#include "LoginDialog.h"
#include "ui_LoginDialog.h"

#include "ChatSession.h"
#include "RegisterDialog.h"

#include <QDebug>
#include <QIntValidator>
#include <QMessageBox>

LoginDialog::LoginDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    setWindowTitle(QString::fromUtf8("IMSystem - 登录"));

    QIntValidator *portValidator = new QIntValidator(1, 65535, this);
    ui->portEdit->setValidator(portValidator);

    ChatSession &session = ChatSession::instance();
    connect(&session, &ChatSession::connected, this, &LoginDialog::onConnectionEstablished);
    connect(&session, &ChatSession::disconnected, this, &LoginDialog::onConnectionClosed);
    connect(&session, &ChatSession::loginSucceeded, this, &LoginDialog::onLoginSucceeded);
    connect(&session, &ChatSession::loginFailed, this, &LoginDialog::onLoginFailed);
    connect(&session, &ChatSession::transportError, this, &LoginDialog::onTransportError);
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

QString LoginDialog::username() const
{
    return m_username;
}

void LoginDialog::setBusy(bool busy)
{
    ui->loginBtn->setEnabled(!busy);
    ui->registerBtn->setEnabled(!busy);
    ui->loginBtn->setText(busy ? QString::fromUtf8("连接中...") : QString::fromUtf8("登录"));
}

bool LoginDialog::readServerAddress(QString *server, quint16 *port) const
{
    *server = ui->serverEdit->text().trimmed();
    if (server->isEmpty()) {
        QMessageBox::warning(const_cast<LoginDialog *>(this), QString::fromUtf8("提示"),
                             QString::fromUtf8("服务器地址不能为空"));
        return false;
    }

    bool ok = false;
    *port = ui->portEdit->text().trimmed().toUShort(&ok);
    if (!ok || *port == 0) {
        QMessageBox::warning(const_cast<LoginDialog *>(this), QString::fromUtf8("提示"),
                             QString::fromUtf8("端口号无效"));
        return false;
    }
    return true;
}

void LoginDialog::on_loginBtn_clicked()
{
    if (ChatSession::instance().isConnecting()) {
        return;
    }

    const QString username = ui->usernameEdit->text().trimmed();
    const QString password = ui->passwordEdit->text();
    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"),
                             QString::fromUtf8("用户名和密码不能为空"));
        return;
    }

    QString server;
    quint16 port = 0;
    if (!readServerAddress(&server, &port)) {
        return;
    }

    m_username = username;
    m_password = password;
    m_loginPending = true;
    setBusy(true);

    ChatSession &session = ChatSession::instance();
    if (session.isConnected()) {
        session.login(m_username, m_password);
    } else {
        session.connectToServer(server, port);
    }
}

void LoginDialog::on_registerBtn_clicked()
{
    // 注册对话框沿用登录框填写的服务器地址
    QString server;
    quint16 port = 0;
    if (readServerAddress(&server, &port)) {
        ChatSession::instance().setServerAddress(server, port);
    }

    RegisterDialog registerDialog(this);
    connect(&registerDialog, &RegisterDialog::registerSuccess,
            this, &LoginDialog::onRegisterSuccess);
    registerDialog.exec();
}

void LoginDialog::onConnectionEstablished()
{
    // 只有“点了登录但还没连上”的情况才自动补发登录请求
    if (m_loginPending && !m_username.isEmpty() && !m_password.isEmpty()) {
        ChatSession::instance().login(m_username, m_password);
    }
}

void LoginDialog::onConnectionClosed()
{
    // 服务端回收空闲连接等情况：恢复按钮，让用户可以直接再点一次登录，
    // 否则按钮会一直停在“连接中...”
    setBusy(false);
}

void LoginDialog::onLoginSucceeded(const QString &token)
{
    Q_UNUSED(token);  // token 由会话层保存，界面不需要关心
    m_loginPending = false;
    setBusy(false);

    QMessageBox::information(this, QString::fromUtf8("成功"), QString::fromUtf8("登录成功!"));
    accept();
}

void LoginDialog::onLoginFailed(const QString &message)
{
    m_loginPending = false;
    setBusy(false);
    QMessageBox::warning(this, QString::fromUtf8("登录失败"), message);
}

void LoginDialog::onTransportError(const QString &error)
{
    setBusy(false);
    QMessageBox::critical(this, QString::fromUtf8("连接错误"),
                          QString::fromUtf8("无法连接服务器: ") + error);
}

void LoginDialog::onRegisterSuccess(const QString &username)
{
    ui->usernameEdit->setText(username);
    ui->passwordEdit->setFocus();
}
