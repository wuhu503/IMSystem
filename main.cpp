#include "MainWindow.h"
#include "LoginDialog.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 登录成功后进入主界面；退出登录或连接断开时回到登录界面，关闭窗口则退出程序
    bool backToLogin = false;
    while (true) {
        LoginDialog loginDialog;
        if (loginDialog.exec() != QDialog::Accepted) {
            break;
        }

        MainWindow w;
        QObject::connect(&w, &MainWindow::logoutRequested, &w, &QWidget::close);
        QObject::connect(&w, &MainWindow::logoutRequested, [&backToLogin]() {
            backToLogin = true;
        });
        w.setUsername(loginDialog.username());  // 传递用户名
        w.show();

        a.exec();

        if (!backToLogin) {
            break;
        }
        backToLogin = false;
    }

    return 0;
}
