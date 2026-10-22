// icons.h — programmatically painted pixmaps (no binary icon assets needed).
// SPDX-License-Identifier: MIT
#pragma once

#include <QIcon>
#include <QPixmap>

namespace Icons {

// Crescent-and-star app icon on an emerald disc with gold moon.
QIcon appIcon(int size = 64);
QPixmap appPixmap(int size);

// Small circular badge with the prayer's Arabic initial (row markers).
QPixmap prayerDot(int size, const QString &arabicInitial, const QColor &base,
                  const QColor &ink);

}  // namespace Icons
