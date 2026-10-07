#ifndef UIKIT_H
#define UIKIT_H

#include <QColor>
#include <QPixmap>
#include <QString>

// 客户端里反复出现的绘制与文案拼接统一收在这里。
// 之前"画头像""画红点""拼气泡"在主窗口里各写了一份，改配色要改好几处。
namespace UiKit {

// 统一样式
extern const QColor kAvatarBlue;    // 好友列表头像底色
extern const QColor kAvatarOrange;  // 好友请求头像底色
extern const QColor kUnreadRed;     // 未读红点颜色

// 列表头像：首字母色块，unread=true 时右下角叠加红点徽标
QPixmap avatar(const QString &displayName,
               const QColor &background = kAvatarBlue,
               bool unread = false,
               int size = 40);

// 独立的圆形色点（按钮角标用）
QPixmap dot(int size = 12, const QColor &color = kUnreadRed);

// 单条消息的 HTML 片段（纯行内内容，已转义）。
// 左/右对齐不在这里做：QTextBrowser 的段落对齐必须由调用方按段落设置，
// 写在 HTML 里从第二条消息起就会失效（详见 MainWindow::appendMessage）。
QString messageBubble(const QString &text, bool isSelf, const QString &timeText);

} // namespace UiKit

#endif // UIKIT_H
