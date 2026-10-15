//
// Created by Hz6826 on 2026/10/15.
//

#include "AutoStart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QString>

#ifdef Q_OS_WIN
#include <QSettings>
#elif defined(Q_OS_MACOS)
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#else // Q_OS_LINUX
#include <QFile>
#include <QStandardPaths>
#endif

namespace AutoStart {
    QString identifier() {
        return "XJTUNetLogin";
    }

#ifdef Q_OS_WIN
    // ========== Windows 实现 ==========
#include <QSettings>

    bool isEnabled() {
        QSettings settings(
            R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)",
            QSettings::NativeFormat);
        return settings.contains(identifier());
    }

    bool setEnabled(bool enabled) {
        QSettings settings(
            R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)",
            QSettings::NativeFormat);

        if (enabled) {
            const QString exePath = QDir::toNativeSeparators(
                QCoreApplication::applicationFilePath());
            settings.setValue(identifier(), exePath);
        } else {
            settings.remove(identifier());
        }

        settings.sync();
        return settings.status() == QSettings::NoError;
    }
#elif defined(Q_OS_MACOS)
    namespace {
        QString plistPath() {
            return QDir::homePath()
                + "/Library/LaunchAgents/"
                + AutoStart::identifier() + ".plist";
        }

        QString launchLabel() {
            return "com.hz6826." + AutoStart::identifier().toLower();
        }
    }

    bool isEnabled() {
        return QFile::exists(plistPath());
    }

    bool setEnabled(bool enabled) {
        const QString path = plistPath();

        if (!enabled) {
            // 先从 launchd 卸载，再删文件
            QProcess::execute("launchctl", {"unload", path});
            QFile::remove(path);
            return !QFile::exists(path);
        }

        // 确保父目录存在
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
            return false;
        }

        const QString exePath = QCoreApplication::applicationFilePath();
        const QString label = launchLabel();

        const QString content = QString(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
            "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
            "<plist version=\"1.0\">\n"
            "<dict>\n"
            "    <key>Label</key>\n"
            "    <string>%1</string>\n"
            "    <key>ProgramArguments</key>\n"
            "    <array>\n"
            "        <string>%2</string>\n"
            "    </array>\n"
            "    <key>RunAtLoad</key>\n"
            "    <true/>\n"
            "</dict>\n"
            "</plist>\n"
        ).arg(label, exePath);

        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        f.write(content.toUtf8());
        f.close();

        // 通知 launchd 加载
        QProcess::execute("launchctl", {"load", path});

        return QFile::exists(path);
    }
#else   // Linux / 其他 Unix
    namespace {

        QString desktopPath() {
            const QString configHome = QStandardPaths::writableLocation(
                QStandardPaths::GenericConfigLocation);
            return configHome + "/autostart/" + AutoStart::identifier() + ".desktop";
        }

    } // namespace

    bool isEnabled() {
        return QFile::exists(desktopPath());
    }

    bool setEnabled(bool enabled) {
        const QString path = desktopPath();

        if (!enabled) {
            QFile::remove(path);
            return !QFile::exists(path);
        }

        if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
            return false;
        }

        const QString exePath = QCoreApplication::applicationFilePath();

        // 遵循 XDG Autostart 规范
        const QString content = QString(
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Name=%1\n"
            "Exec=%2\n"
            "Terminal=false\n"
            "X-GNOME-Autostart-enabled=true\n"
        ).arg(AutoStart::identifier(), exePath);

        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        f.write(content.toUtf8());
        f.close();

        return QFile::exists(path);
    }
#endif
}
