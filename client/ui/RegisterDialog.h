#ifndef REGISTERDIALOG_H
#define REGISTERDIALOG_H

#include <QDialog>

namespace Ui {
class RegisterDialog;
}

// 注册界面：只管输入校验与界面状态，协议交互全部交给 ChatSession
class RegisterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisterDialog(QWidget *parent = nullptr);
    ~RegisterDialog();

signals:
    void registerSuccess(const QString &username);

private slots:
    void on_registerBtn_clicked();
    void on_backBtn_clicked();
    void onConnectionEstablished();
    void onConnectionClosed();
    void onRegisterSucceeded();
    void onRegisterFailed(const QString &message);
    void onTransportError(const QString &error);

private:
    void setBusy(bool busy);

    Ui::RegisterDialog *ui;
    QString m_username;
    QString m_password;
    bool m_registerPending = false;  // 已点注册、等待连接建立后自动发起请求
};

#endif // REGISTERDIALOG_H
