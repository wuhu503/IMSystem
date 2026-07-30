#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QJsonObject>
#include <QJsonArray>
#include "message.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    
    void setUsername(const QString &username);

private slots:
    void onFriendClicked(QListWidgetItem *item);
    void onSendClicked();
    void onSearchTextChanged(const QString &text);
    void on_actionExit_triggered();
    void on_actionAbout_triggered();
    
    // 好友相关槽函数
    void onAddFriendClicked();
    void onRefreshFriendsClicked();
    void onFriendRequestsClicked();
    void onDeleteFriendClicked();
    
    // 网络消息处理
    void onMessageReceived(const Message &msg);
    void onConnectionEstablished();
    void onConnectionClosed();
    void onErrorOccurred(const QString &error);

private:
    void initFriendList();
    void appendMessage(const QString &nickname, const QString &message, bool isSelf);
    QString createBubbleHtml(const QString &message, bool isSelf, const QString &time);
    
    // 好友功能
    void requestFriendList();
    void handleFriendListResponse(const QJsonObject &body);
    void handleAddFriendResponse(const QJsonObject &body);
    void handleSearchUserResponse(const QJsonObject &body);
    void handleAcceptFriendResponse(const QJsonObject &body);
    void handleRejectFriendResponse(const QJsonObject &body);
    void handleDeleteFriendResponse(const QJsonObject &body);
    
    // UI更新
    void updateFriendList(const QJsonArray &friends);
    void showAddFriendDialog();
    void showFriendRequestsDialog();

    Ui::MainWindow *ui;
    QString currentChatFriend;
    QString m_username;
};

#endif // MAINWINDOW_H
