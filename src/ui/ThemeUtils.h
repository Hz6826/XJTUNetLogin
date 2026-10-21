#ifndef THEMEUTILS_H
#define THEMEUTILS_H

#include <QApplication>
#include <QColor>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>

namespace Theme {

    // 优先使用 Qt 6.5+ 的 colorScheme()；Unknown 或旧 Qt 时回退到亮度启发式。
    inline Qt::ColorScheme colorScheme() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        if (const QStyleHints *hints = QGuiApplication::styleHints()) {
            const Qt::ColorScheme s = hints->colorScheme();
            if (s != Qt::ColorScheme::Unknown)
                return s;
        }
#endif
        // fallback：亮度启发式（兼容旧 Qt / 某些平台 Unknown）
        if (!qApp)
            return Qt::ColorScheme::Light;
        const QColor bg = QApplication::palette().color(QPalette::Window);
        return bg.lightness() < 128 ? Qt::ColorScheme::Dark
                                    : Qt::ColorScheme::Light;
    }

    inline bool isDark() {
        return colorScheme() == Qt::ColorScheme::Dark;
    }
} // namespace Theme

#endif // THEMEUTILS_H
