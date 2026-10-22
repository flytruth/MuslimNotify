// icons.cpp — QPainter-drawn icons.
// SPDX-License-Identifier: MIT
#include "icons.h"

#include <QColor>
#include <QPainter>
#include <QPainterPath>

namespace Icons {

QPixmap appPixmap(int size) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r(0, 0, size, size);
    // Emerald disc with subtle rim.
    QRadialGradient grad(r.center(), size * 0.6);
    grad.setColorAt(0.0, QColor("#128f6e"));
    grad.setColorAt(1.0, QColor("#0b5f4a"));
    p.setPen(Qt::NoPen);
    p.setBrush(grad);
    p.drawEllipse(r);

    // Crescent: gold circle minus an offset disc.
    QPainterPath moon;
    moon.addEllipse(QRectF(size * 0.22, size * 0.16, size * 0.58, size * 0.58));
    QPainterPath bite;
    bite.addEllipse(QRectF(size * 0.40, size * 0.08, size * 0.55, size * 0.55));
    p.setBrush(QColor("#e7c14d"));
    p.drawPath(moon.subtracted(bite));

    // Four-point star in the crescent's embrace.
    QPainterPath star;
    const QPointF c(size * 0.68, size * 0.38);
    const qreal rad = size * 0.11;
    star.moveTo(c + QPointF(0, -rad));
    star.quadTo(c + QPointF(0.12 * rad, -0.12 * rad), c + QPointF(rad, 0));
    star.quadTo(c + QPointF(0.12 * rad, 0.12 * rad), c + QPointF(0, rad));
    star.quadTo(c + QPointF(-0.12 * rad, 0.12 * rad), c + QPointF(-rad, 0));
    star.quadTo(c + QPointF(-0.12 * rad, -0.12 * rad), c + QPointF(0, -rad));
    p.setBrush(QColor("#f5e6a8"));
    p.drawPath(star);

    return pm;
}

QIcon appIcon(int size) {
    return QIcon(appPixmap(size));
}

QPixmap prayerDot(int size, const QString &arabicInitial, const QColor &base,
                  const QColor &ink) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(base);
    p.drawEllipse(0, 0, size, size);
    p.setPen(ink);
    QFont f = p.font();
    f.setPixelSize(static_cast<int>(size * 0.62));
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, arabicInitial.left(1));
    return pm;
}

}  // namespace Icons
