#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <atomic>
#include <QHash>
#include "message.h"

class QCloseEvent;

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

signals:
    void logoutRequested();

private slots:
    void onFriendClicked(QListWidgetItem *item);
    void onSendClicked();
    void onSearchTextChanged(const QString &text);
    void on_actionLogout_triggered();
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
    void closeEvent(QCloseEvent *event) override;

private:
    void initFriendList();
    void appendMessage(const QString &nickname, const QString &message, bool isSelf,
                       qint64 timestamp = 0);
    void requestFriendList();
    void requestPendingFriendRequests();
    void requestChatHistory(const QString &friendUsername);
    void handleLoginResponse(const QJsonObject &body);
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
    QListWidgetItem* findFriendItem(const QString &username) const;
    void refreshFriendItemUnread(QListWidgetItem *item);
    void showAddFriendDialog();
    void showFriendRequestsDialog();
    void showPendingRequestsDialog(const QJsonArray &requests);

    Ui::MainWindow *ui;
    QString currentChatFriend;
    QString m_username;
    std::atomic<uint32_t> m_sequenceCounter{1};  // 消息序列号计数器
    int m_chatGeneration = 0;     // 当前会话代次，点击好友时自增
    int m_historyGeneration = 0;  // 最近一次历史请求对应的会话代次
    bool m_loggingOut = false;    // 主动退出登录/关闭窗口时置位，抑制断线弹窗
    QHash<QString, int> m_unreadCounts;  // 好友用户名 -> 未读消息数（用于列表红点）
};

#endif // MAINWINDOW_H

