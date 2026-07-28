#include "registerdialog.h"
#include "ui_registerdialog.h"
#include <QMessageBox>
#include <QDebug>

RegisterDialog::RegisterDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RegisterDialog)
{
    ui->setupUi(this);
    setWindowTitle(QString::fromUtf8("用户注册"));

    connect(&TcpClient::instance(), &TcpClient::connectionEstablished,
            this, &RegisterDialog::onConnectionEstablished);
    connect(&TcpClient::instance(), &TcpClient::messageReceived,
            this, &RegisterDialog::onMessageReceived);
    connect(&TcpClient::instance(), &TcpClient::errorOccurred,
            this, &RegisterDialog::onErrorOccurred);
}

RegisterDialog::~RegisterDialog()
{
    delete ui;
}

// 注册按钮点击
void RegisterDialog::on_registerBtn_clicked()
{
    if (!ui->registerBtn->isEnabled() || TcpClient::instance().isConnecting()) {
        return;
    }
    
    QString username = ui->usernameEdit->text().trimmed();
    QString password = ui->passwordEdit->text();
    QString confirmPassword = ui->confirmPasswordEdit->text();
    
    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("用户名和密码不能为空"));
        return;
    }

    if (username.length() < 3 || username.length() > 20) {
        QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("用户名长度必须在3-20之间"));
        return;
    }

    if (password.length() < 6) {
        QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("密码长度不能少于6位"));
        return;
    }

    if (password != confirmPassword) {
        QMessageBox::warning(this, QString::fromUtf8("提示"), QString::fromUtf8("两次输入的密码不一致"));
        return;
    }

    m_username = username;
    m_password = password;
    
    ui->registerBtn->setEnabled(false);
    ui->registerBtn->setText(QString::fromUtf8("连接中..."));
    ui->backBtn->setEnabled(false);
    
    if (TcpClient::instance().isConnect()) {
        sendRegisterRequest(m_username, m_password);
    } else {
        TcpClient::instance().connectToServer("127.0.0.1", 8080);
    }
}

// 返回按钮
void RegisterDialog::on_backBtn_clicked()
{
    reject();
}

// 连接成功后发送注册请求
void RegisterDialog::onConnectionEstablished()
{
    if (!m_username.isEmpty() && !m_password.isEmpty()) {
        sendRegisterRequest(m_username, m_password);
    }
}

// 收到服务器响应
void RegisterDialog::onMessageReceived(const Message &msg)
{
    if (msg.type() == MessageType::RSP_REGISTER) {
        handleRegisterResponse(msg.jsonBody());
    }
}

// 连接错误
void RegisterDialog::onErrorOccurred(const QString &error)
{
    ui->registerBtn->setEnabled(true);
    ui->registerBtn->setText(QString::fromUtf8("注册"));
    ui->backBtn->setEnabled(true);
    
    QMessageBox::critical(this, QString::fromUtf8("连接错误"), 
                          QString::fromUtf8("无法连接服务器: ") + error);
}

// 发送注册请求
void RegisterDialog::sendRegisterRequest(const QString &username, const QString &password)
{
    Message msg(MessageType::REQ_REGISTER);
    msg.setSequence(2);

    QJsonObject body;
    body["username"] = username;
    body["password"] = password;
    msg.setJsonBody(body);

    TcpClient::instance().sendMessage(msg);
}

// 处理注册响应
void RegisterDialog::handleRegisterResponse(const QJsonObject &body)
{
    ui->registerBtn->setEnabled(true);
    ui->registerBtn->setText(QString::fromUtf8("注册"));
    ui->backBtn->setEnabled(true);
    
    bool success = body["success"].toBool();
    QString message = body["message"].toString();

    if (success) {
        QMessageBox::information(this, QString::fromUtf8("成功"), 
                                 QString::fromUtf8("注册成功! 请返回登录"));
        emit registerSuccess(m_username);
        accept();
    } else {
        QMessageBox::warning(this, QString::fromUtf8("注册失败"), message);
    }
}
