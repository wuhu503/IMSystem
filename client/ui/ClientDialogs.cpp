#include "ClientDialogs.h"

#include "ChatSession.h"
#include "UiKit.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QJsonObject>
#include <QObject>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

namespace ClientDialogs {

QString pickSearchResult(const QJsonArray &users, QWidget *parent)
{
    QStringList labels;
    QStringList usernames;

    for (const QJsonValue &value : users) {
        const QJsonObject user = value.toObject();
        const QString username = user["username"].toString();
        const QString nickname = user["nickname"].toString();
        const QString display = nickname.isEmpty() ? username : nickname;

        labels << QStringLiteral("%1 (%2) - %3")
                      .arg(display, username,
                           user["status"].toInt() == 1 ? QString::fromUtf8("在线")
                                                       : QString::fromUtf8("离线"));
        usernames << username;
    }

    if (labels.isEmpty()) {
        return QString();
    }

    bool ok = false;
    const QString selected = QInputDialog::getItem(parent, QString::fromUtf8("搜索结果"),
                                                   QString::fromUtf8("选择要添加的用户："),
                                                   labels, 0, false, &ok);
    if (!ok || selected.isEmpty()) {
        return QString();
    }

    const int index = labels.indexOf(selected);
    return (index >= 0 && index < usernames.size()) ? usernames.at(index) : QString();
}

void showFriendRequests(const QJsonArray &requests, QWidget *parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QString::fromUtf8("好友请求"));
    dialog.setMinimumSize(400, 300);

    auto *mainLayout = new QVBoxLayout(&dialog);

    auto *titleLabel = new QLabel(
        QString::fromUtf8("待处理的好友请求 (%1)").arg(requests.size()));
    titleLabel->setStyleSheet(
        QStringLiteral("font-size: 16px; font-weight: bold; padding: 10px;"));
    mainLayout->addWidget(titleLabel);

    auto *requestList = new QListWidget();
    requestList->setIconSize(QSize(40, 40));

    for (const QJsonValue &value : requests) {
        const QJsonObject request = value.toObject();
        const QString username = request["username"].toString();
        const QString nickname = request["nickname"].toString();
        const QString display = nickname.isEmpty() ? username : nickname;

        auto *item = new QListWidgetItem(requestList);
        item->setData(Qt::UserRole, username);
        item->setText(QStringLiteral("%1 (%2)").arg(display, username));
        item->setSizeHint(QSize(0, 50));
        item->setIcon(QIcon(UiKit::avatar(display, UiKit::kAvatarOrange)));
    }

    if (requests.isEmpty()) {
        // 空列表时不显示列表控件，但仍然保留底部按钮（关闭）
        auto *emptyLabel = new QLabel(QString::fromUtf8("暂无待处理的好友请求"));
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet(QStringLiteral("color: #999; padding: 20px;"));
        mainLayout->addWidget(emptyLabel);
    } else {
        mainLayout->addWidget(requestList);
    }

    auto *buttonLayout = new QHBoxLayout();

    auto *acceptBtn = new QPushButton(QString::fromUtf8("接受"));
    acceptBtn->setStyleSheet(QStringLiteral(
        "background-color: #4CAF50; color: white; padding: 8px 16px;"));

    auto *rejectBtn = new QPushButton(QString::fromUtf8("拒绝"));
    rejectBtn->setStyleSheet(QStringLiteral(
        "background-color: #f44336; color: white; padding: 8px 16px;"));

    auto *closeBtn = new QPushButton(QString::fromUtf8("关闭"));
    closeBtn->setStyleSheet(QStringLiteral(
        "background-color: #9E9E9E; color: white; padding: 8px 16px;"));

    acceptBtn->setEnabled(!requests.isEmpty());
    rejectBtn->setEnabled(!requests.isEmpty());

    buttonLayout->addWidget(acceptBtn);
    buttonLayout->addWidget(rejectBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeBtn);
    mainLayout->addLayout(buttonLayout);

    // 处理后立即把该条从列表里移除，避免用户对同一条重复操作
    const auto handleSelection = [&dialog, requestList](bool accept) {
        QListWidgetItem *currentItem = requestList->currentItem();
        if (!currentItem) {
            QMessageBox::warning(&dialog, QString::fromUtf8("提示"),
                                 QString::fromUtf8("请先选择一个好友请求"));
            return;
        }

        const QString username = currentItem->data(Qt::UserRole).toString();
        if (accept) {
            ChatSession::instance().acceptFriend(username);
        } else {
            ChatSession::instance().rejectFriend(username);
        }
        delete requestList->takeItem(requestList->row(currentItem));
    };

    QObject::connect(acceptBtn, &QPushButton::clicked, &dialog,
                     [handleSelection]() { handleSelection(true); });
    QObject::connect(rejectBtn, &QPushButton::clicked, &dialog,
                     [handleSelection]() { handleSelection(false); });
    QObject::connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    dialog.exec();
}

} // namespace ClientDialogs
