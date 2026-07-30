#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
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
    void onAddFriendClicked();
    void onRefreshFriendsClicked();
    void onFriendRequestsClicked();
    void onDeleteFriendClicked();
    void onMessageReceived(const Message &msg);
    void onConnectionEstablished();
    void onConnectionClosed();
    void onErrorOccurred(const QString &error);

private:
    void initFriendList();
    void appendMessage(const QString &nickname, const QString &message, bool isSelf);
    void requestFriendList();
    void requestPendingFriendRequests();
    void requestChatHistory(const QString &friendUsername);
    void handleHeartbeat(const QJsonObject &body);
    void handleFriendListResponse(const QJsonObject &body);
    void handlePendingRequestsResponse(const QJsonObject &body);
    void handleAddFriendResponse(const QJsonObject &body);
    void handleSearchUserResponse(const QJsonObject &body);
    void handleAcceptFriendResponse(const QJsonObject &body);
    void handleRejectFriendResponse(const QJsonObject &body);
    void handleDeleteFriendResponse(const QJsonObject &body);
    void handleTextMessageReceived(const QJsonObject &body);
    void handleMessageAckResponse(const QJsonObject &body);
    void handleHistoryResponse(const QJsonObject &body);
    void updateFriendList(const QJsonArray &friends);
    void showAddFriendDialog();
    void showFriendRequestsDialog();
    void showPendingRequestsDialog(const QJsonArray &requests);

    Ui::MainWindow *ui;
    QString currentChatFriend;
    QString m_username;
};

#endif // MAINWINDOW_H
