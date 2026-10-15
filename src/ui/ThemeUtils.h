//
// Created by Hz6826 on 2026/10/15.
//

#ifndef THEMEUTILS_H
#define THEMEUTILS_H

#include <QApplication>
#include <QColor>
#include <QPalette>

namespace Theme {
    inline bool isDark() {
        const QColor bg = QApplication::palette().color(QPalette::Window);
        return bg.lightness() < 128;
    }
}

#endif // THEMEUTILS_H
