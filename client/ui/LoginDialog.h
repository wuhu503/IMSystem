#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>

namespace Ui {
class LoginDialog;
}

// 登录界面：只管输入校验与界面状态，协议交互全部交给 ChatSession
class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

    QString username() const;

private slots:
    void on_loginBtn_clicked();
    void on_registerBtn_clicked();
    void onConnectionEstablished();
    void onConnectionClosed();
    void onLoginSucceeded(const QString &token);
    void onLoginFailed(const QString &message);
    void onTransportError(const QString &error);
    void onRegisterSuccess(const QString &username);

private:
    void setBusy(bool busy);
    bool readServerAddress(QString *server, quint16 *port) const;

    Ui::LoginDialog *ui;
    QString m_username;
    QString m_password;
    bool m_loginPending = false;  // 已点登录、等待连接建立后自动发起登录
};

#endif // LOGINDIALOG_H
