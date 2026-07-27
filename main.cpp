#include "mainwindow.h"
#include "logindialog.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    // 显示登录对话框
    LoginDialog loginDialog;
    if (loginDialog.exec() == QDialog::Accepted) {
        // 登录成功，显示主界面
        MainWindow w;
        w.show();
        return a.exec();
    }
    
    // 登录取消或失败，退出程序
    return 0;
}
