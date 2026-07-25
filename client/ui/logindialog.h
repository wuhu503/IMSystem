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

private slots:
    void on_loginBtn_clicked();
    void on_registerBtn_clicked();
    void onConnectionEstablished();
    void onMessageReceived(const Message &msg);
    void onErrorOccurred(const QString &error);

private:
    void sendLoginRequest(const QString &username, const QString &password);
    void handleLoginResponse(const QJsonObject &body);

    Ui::LoginDialog *ui;
};

#endif // LOGINDIALOG_H
