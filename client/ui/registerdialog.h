#ifndef REGISTERDIALOG_H
#define REGISTERDIALOG_H

#include <QDialog>
#include <QJsonObject>
#include "message.h"
#include "tcpclient.h"

namespace Ui {
class RegisterDialog;
}

class RegisterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisterDialog(QWidget *parent = nullptr);
    ~RegisterDialog();

signals:
    // 注册成功信号，传递用户名给登录界面
    void registerSuccess(const QString &username);

private slots:
    void on_registerBtn_clicked();
    void on_backBtn_clicked();
    void onConnectionEstablished();
    void onMessageReceived(const Message &msg);
    void onErrorOccurred(const QString &error);

private:
    void sendRegisterRequest(const QString &username, const QString &password);
    void handleRegisterResponse(const QJsonObject &body);

    Ui::RegisterDialog *ui;
    QString m_username;  // 保存注册的用户名
    QString m_password;  // 保存注册的密码
};

#endif // REGISTERDIALOG_H
