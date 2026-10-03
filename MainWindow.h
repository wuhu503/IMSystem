#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>

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
    void on_searchUserBtn_clicked();
    void on_actionLogout_triggered();
    void on_actionExit_triggered();
    void on_actionAbout_triggered();
    void onAddFriendClicked();
    void onRefreshFriendsClicked();
    void onFriendRequestsClicked();
    void onDeleteFriendClicked();
    void onConnectionEstablished();
    void onConnectionClosed();
    void onErrorOccurred(const QString &error);
    void closeEvent(QCloseEvent *event) override;

private:
    void initFriendList();
    void appendMessage(const QString &nickname, const QString &message, bool isSelf,
                       qint64 timestamp = 0);

    // 会话层信号的处理：只做展示与界面状态维护
    void handleKickedOffline(const QString &message);
    void handleFriendListResponse(const QJsonObject &body);
    void handlePendingRequestsResponse(const QJsonObject &body);
    void handleAddFriendResponse(const QJsonObject &body);
    void handleSearchUserResponse(const QJsonObject &body);
    void handleAcceptFriendResponse(const QJsonObject &body);
    void handleRejectFriendResponse(const QJsonObject &body);
    void handleDeleteFriendResponse(const QJsonObject &body);
    void handleFriendRequestNotification();
    void handleFriendAcceptedNotification();
    void handleFriendRejectedNotification(const QString &username);
    void handleFriendRemovedNotification(const QString &username);
    void handleTextReceived(const QString &sender, const QString &content, qint64 timestamp);
    void handleTextSendSucceeded(const QString &receiver, const QString &content);
    void handleTextSendFailed(const QString &receiver, const QString &content,
                              const QString &message);
    void handleHistoryReceived(const QString &friendUsername, const QJsonArray &messages);
    void handleHistoryFailed(const QString &friendUsername, const QString &message);

    void applyFriendFilter();
    void resetChatView();
    void updateFriendList(const QJsonArray &friends);
    QListWidgetItem* findFriendItem(const QString &username) const;
    void refreshFriendItemUnread(QListWidgetItem *item);
    void setFriendItemAvatar(QListWidgetItem *item, const QString &displayName, bool unread);
    void updateFriendRequestButton();
    void showAddFriendDialog();
    void showFriendRequestsDialog();
    void showPendingRequestsDialog(const QJsonArray &requests);

    Ui::MainWindow *ui;
    QString currentChatFriend;
    QString m_username;
    bool m_loggingOut = false;    // 主动退出登录/关闭窗口时置位，抑制断线弹窗
    QHash<QString, int> m_unreadCounts;  // 好友用户名 -> 未读消息数（用于列表红点）
    int m_unreadFriendRequests = 0;      // 未查看的新好友请求数（用于按钮红点）
    bool m_silentPendingFetch = false;   // 登录后静默拉取好友请求（只亮红点不弹列表）
};

#endif // MAINWINDOW_H

