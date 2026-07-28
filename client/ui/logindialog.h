#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QJsonObject>
#include "message.h"
#include "tcpclient.h"

namespace Ui {
class LoginDialog;
}

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
    void onMessageReceived(const Message &msg);
    void onErrorOccurred(const QString &error);
    void onRegisterSuccess(const QString &username);

private:
    void sendLoginRequest(const QString &username, const QString &password);
    void handleLoginResponse(const QJsonObject &body);

    Ui::LoginDialog *ui;
    QString m_username;
    QString m_password;
    QString m_server;
    quint16 m_port;
};

#endif // LOGINDIALOG_H
