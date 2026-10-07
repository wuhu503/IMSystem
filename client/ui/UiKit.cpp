#include "UiKit.h"

#include <QBrush>
#include <QFont>
#include <QPainter>
#include <QPen>
#include <QRectF>

namespace UiKit {

const QColor kAvatarBlue(100, 149, 237);
const QColor kAvatarOrange(255, 152, 0);
const QColor kUnreadRed(0xE5, 0x39, 0x35);

QPixmap avatar(const QString &displayName, const QColor &background, bool unread, int size)
{
    QPixmap pixmap(size, size);
    pixmap.fill(background);

    {
        // 先结束画笔再叠加徽标，保证两步绘制不会互相干扰
        QPainter painter(&pixmap);
        painter.setPen(Qt::white);
        painter.setFont(QFont(QStringLiteral("Arial"), size * 2 / 5, QFont::Bold));
        painter.drawText(pixmap.rect(), Qt::AlignCenter, displayName.left(1));
    }

    if (unread) {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(Qt::white, 1.5));
        painter.setBrush(kUnreadRed);

        const qreal badgeSize = size * 0.3;
        const qreal margin = 2.0;
        painter.drawEllipse(QRectF(size - badgeSize - margin,
                                   size - badgeSize - margin,
                                   badgeSize, badgeSize));
    }

    return pixmap;
}

QPixmap dot(int size, const QColor &color)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(0, 0, size, size);

    return pixmap;
}

QString messageBubble(const QString &text, bool isSelf, const QString &timeText)
{
    const QString body = text.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"));

    if (isSelf) {
        return QStringLiteral(
                   "<span style='font-size:10px; color:#999;'>%1 </span>"
                   "<span style='background-color:#95EC69; padding:8px 12px; "
                   "border-radius:10px;'>%2</span>")
            .arg(timeText, body);
    }

    return QStringLiteral(
               "<span style='background-color:#FFFFFF; padding:8px 12px; "
               "border-radius:10px;'>%1</span>"
               "<span style='font-size:10px; color:#999;'> %2</span>")
        .arg(body, timeText);
}

} // namespace UiKit
