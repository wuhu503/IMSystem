#ifndef CLIENTDIALOGS_H
#define CLIENTDIALOGS_H

#include <QJsonArray>
#include <QString>

class QWidget;

// 主窗口里临时拼装的对话框集中在这里：
// 主窗口只负责"什么时候弹"，弹什么、点了之后怎么处理都放在本文件内，
// 免得 MainWindow.cpp 同时承担网络语义、列表渲染和对话框布局三件事。
namespace ClientDialogs {

// 展示搜索结果并让用户选一个，返回选中的用户名；取消或无功而返时返回空串
QString pickSearchResult(const QJsonArray &users, QWidget *parent);

// 待处理好友请求列表：可在对话框内直接接受/拒绝
void showFriendRequests(const QJsonArray &requests, QWidget *parent);

} // namespace ClientDialogs

#endif // CLIENTDIALOGS_H
