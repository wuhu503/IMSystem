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
};

#endif // REGISTERDIALOG_H
