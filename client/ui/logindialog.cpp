#include "logindialog.h"
#include "ui_logindialog.h"
#include "registerdialog.h"
#include <QString>
#include <QMessageBox>

LoginDialog::LoginDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
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

// 登录按钮点击
void LoginDialog::on_loginBtn_clicked()
{
    QString username = ui->usernameEdit->text();
    QString password = ui->passwordEdit->text();
    QString server = ui->serverEdit->text().trimmed();
    QString portStr = ui->portEdit->text().trimmed();

    if (server.isEmpty() || portStr.isEmpty()) {
        QMessageBox::warning(this, "提示", "服务器地址和端口不能为空");
        return;
    }

    bool ok;
    quint16 port = portStr.toUShort(&ok);
    if (!ok || port == 0) {
        QMessageBox::warning(this, "提示", "端口号无效，请输入1-65535之间的数字");
        return;
    }

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "提示", "用户名和密码不能为空");
        return;
    }

    TcpClient::instance().connectToServer(server, port);
}

// 注册按钮点击 - 跳转到注册界面
void LoginDialog::on_registerBtn_clicked()
{
    RegisterDialog registerDialog(this);
    registerDialog.exec();  // 模态显示注册对话框
}

// 连接成功后发送登录请求
void LoginDialog::onConnectionEstablished()
{
    QString username = ui->usernameEdit->text();
    QString password = ui->passwordEdit->text();
    sendLoginRequest(username, password);
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
    QMessageBox::critical(this, "连接错误", "无法连接服务器: " + error);
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
    bool success = body["success"].toBool();
    QString message = body["message"].toString();

    if (success) {
        QMessageBox::information(this, "成功", "登录成功!");
        accept();  // 关闭对话框，返回 QDialog::Accepted
    } else {
        QMessageBox::warning(this, "登录失败", message);
    }
}