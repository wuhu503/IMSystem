#include "registerdialog.h"
#include "ui_registerdialog.h"
#include <QMessageBox>

RegisterDialog::RegisterDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RegisterDialog)
{
    ui->setupUi(this);
    setWindowTitle("用户注册");
    
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
    QString username = ui->usernameEdit->text();
    QString password = ui->passwordEdit->text();
    QString confirmPassword = ui->confirmPasswordEdit->text();
    
    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "提示", "用户名和密码不能为空");
        return;
    }
    
    if (username.length() < 3 || username.length() > 20) {
        QMessageBox::warning(this, "提示", "用户名长度必须在3-20之间");
        return;
    }
    
    if (password.length() < 6) {
        QMessageBox::warning(this, "提示", "密码长度不能少于6位");
        return;
    }
    
    if (password != confirmPassword) {
        QMessageBox::warning(this, "提示", "两次输入的密码不一致");
        return;
    }
    
    TcpClient::instance().connectToServer("127.0.0.1", 8080);
}

// 返回按钮
void RegisterDialog::on_backBtn_clicked()
{
    reject();  // 关闭对话框，返回登录界面
}

// 连接成功后发送注册请求
void RegisterDialog::onConnectionEstablished()
{
    QString username = ui->usernameEdit->text();
    QString password = ui->passwordEdit->text();
    sendRegisterRequest(username, password);
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
    QMessageBox::critical(this, "连接错误", "无法连接服务器: " + error);
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
    bool success = body["success"].toBool();
    QString message = body["message"].toString();
    
    if (success) {
        QMessageBox::information(this, "成功", "注册成功! 请返回登录");
        ui->usernameEdit->clear();
        ui->passwordEdit->clear();
        ui->confirmPasswordEdit->clear();
    } else {
        QMessageBox::warning(this, "注册失败", message);
    }
}
