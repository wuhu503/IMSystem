#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>

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
    
    void setUsername(const QString &username);  // 设置当前登录用户

private slots:
    void onFriendClicked(QListWidgetItem *item);
    void onSendClicked();
    void onSearchTextChanged(const QString &text);
    void on_actionExit_triggered();
    void on_actionAbout_triggered();

private:
    void initFriendList();
    void addTestFriends();
    void appendMessage(const QString &nickname, const QString &message, bool isSelf);
    QString createBubbleHtml(const QString &message, bool isSelf, const QString &time);

    Ui::MainWindow *ui;
    QString currentChatFriend;  // 当前聊天的好友昵称
    QString m_username;         // 当前登录用户
};

#endif // MAINWINDOW_H