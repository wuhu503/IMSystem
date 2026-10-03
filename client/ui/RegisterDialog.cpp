#include "RegisterDialog.h"
#include "ui_RegisterDialog.h"

#include "ChatSession.h"
#include "Constants.h"

#include <QDebug>
#include <QMessageBox>

RegisterDialog::RegisterDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RegisterDialog)
{
    ui->setupUi(this);
    setWindowTitle(QString::fromUtf8("用户注册"));

    ChatSession &session = ChatSession::instance();
    connect(&session, &ChatSession::connected, this, &RegisterDialog::onConnectionEstablished);
    connect(&session, &ChatSession::disconnected, this, &RegisterDialog::onConnectionClosed);
    connect(&session, &ChatSession::registerSucceeded, this, &RegisterDialog::onRegisterSucceeded);
    connect(&session, &ChatSession::registerFailed, this, &RegisterDialog::onRegisterFailed);
    connect(&session, &ChatSession::transportError, this, &RegisterDialog::onTransportError);
}

RegisterDialog::~RegisterDialog()
{
    delete ui;
}

void RegisterDialog::setBusy(bool busy)
{
    ui->registerBtn->setEnabled(!busy);
    ui->backBtn->setEnabled(!busy);
    ui->registerBtn->setText(busy ? QString::fromUtf8("连接中...") : QString::fromUtf8("注册"));
}

void RegisterDialog::on_registerBtn_clicked()
{
    if (ChatSession::instance().isConnecting()) {
        return;
    }

    const QString username = ui->usernameEdit->text().trimmed();
    const QString password = ui->passwordEdit->text();
    const QString confirmPassword = ui->confirmPasswordEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"),
                             QString::fromUtf8("用户名和密码不能为空"));
        return;
    }

    if (username.length() < IMConstants::kUsernameMinLength
        || username.length() > IMConstants::kUsernameMaxLength) {
        QMessageBox::warning(this, QString::fromUtf8("提示"),
            QString::fromUtf8("用户名长度必须在 %1-%2 之间")
                .arg(IMConstants::kUsernameMinLength)
                .arg(IMConstants::kUsernameMaxLength));
        return;
    }

    if (password.length() < IMConstants::kPasswordMinLength) {
        QMessageBox::warning(this, QString::fromUtf8("提示"),
            QString::fromUtf8("密码长度不能少于 %1 位")
                .arg(IMConstants::kPasswordMinLength));
        return;
    }

    if (password != confirmPassword) {
        QMessageBox::warning(this, QString::fromUtf8("提示"),
                             QString::fromUtf8("两次输入的密码不一致"));
        return;
    }

    m_username = username;
    m_password = password;
    m_registerPending = true;
    setBusy(true);

    ChatSession &session = ChatSession::instance();
    if (session.isConnected()) {
        session.registerAccount(m_username, m_password);
    } else {
        // 沿用登录界面配置的服务器地址
        QString server = session.host();
        quint16 port = session.port();
        if (server.isEmpty() || port == 0) {
            server = QStringLiteral("127.0.0.1");
            port = IMConstants::kDefaultServerPort;
        }
        session.connectToServer(server, port);
    }
}

void RegisterDialog::on_backBtn_clicked()
{
    reject();
}

void RegisterDialog::onConnectionEstablished()
{
    if (m_registerPending && !m_username.isEmpty() && !m_password.isEmpty()) {
        ChatSession::instance().registerAccount(m_username, m_password);
    }
}

void RegisterDialog::onConnectionClosed()
{
    // 连接被服务端回收时恢复按钮，避免界面卡在“连接中...”
    setBusy(false);
}

void RegisterDialog::onRegisterSucceeded()
{
    m_registerPending = false;
    setBusy(false);

    QMessageBox::information(this, QString::fromUtf8("成功"),
                             QString::fromUtf8("注册成功! 请返回登录"));
    emit registerSuccess(m_username);
    accept();
}

void RegisterDialog::onRegisterFailed(const QString &message)
{
    m_registerPending = false;
    setBusy(false);
    QMessageBox::warning(this, QString::fromUtf8("注册失败"), message);
}

void RegisterDialog::onTransportError(const QString &error)
{
    setBusy(false);
    QMessageBox::critical(this, QString::fromUtf8("连接错误"),
                          QString::fromUtf8("无法连接服务器: ") + error);
}
