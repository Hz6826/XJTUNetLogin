//
// Created by Hz6826 on 2026/10/15.
//

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include "AutoStart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QString>

#ifdef Q_OS_WIN
#include <QSettings>
#include <windows.h>
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
    // Run 键按命令行解析。安装目录默认在 Program Files，路径含空格时必须加引号，
    // 否则登录阶段会去启动被截断的 C:\Program.exe。
    // 任务管理器里的开关不改 Run 值，而是写 StartupApproved\Run 的 REG_BINARY：
    // 首字节 02/06 为启用，03/07 为禁用。只改 Run 值无法重新打开被关掉的项。

    namespace {

        constexpr wchar_t kStartupApprovedRun[] =
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";

        QSettings runSettings() {
            return QSettings(
                R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)",
                QSettings::NativeFormat);
        }

        QString quotedCommand() {
            const QString exePath = QDir::toNativeSeparators(
                QCoreApplication::applicationFilePath());
            return QLatin1Char('"') + exePath + QLatin1Char('"');
        }

        bool startupApprovedDisabled() {
            HKEY key = nullptr;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, kStartupApprovedRun, 0,
                              KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
                return false;
            }

            BYTE data[16] = {};
            DWORD size = sizeof(data);
            DWORD type = 0;
            const QString name = identifier();
            const LONG rc = RegQueryValueExW(
                key, reinterpret_cast<const wchar_t *>(name.utf16()),
                nullptr, &type, data, &size);
            RegCloseKey(key);

            if (rc != ERROR_SUCCESS || type != REG_BINARY || size < 1)
                return false;
            return data[0] == 0x03 || data[0] == 0x07;
        }

        bool clearStartupApproved() {
            HKEY key = nullptr;
            const LONG openRc = RegOpenKeyExW(
                HKEY_CURRENT_USER, kStartupApprovedRun, 0, KEY_SET_VALUE, &key);
            if (openRc == ERROR_FILE_NOT_FOUND)
                return true;
            if (openRc != ERROR_SUCCESS)
                return false;

            const QString name = identifier();
            const LONG delRc = RegDeleteValueW(
                key, reinterpret_cast<const wchar_t *>(name.utf16()));
            RegCloseKey(key);
            return delRc == ERROR_SUCCESS || delRc == ERROR_FILE_NOT_FOUND;
        }

        bool commandMatches(const QSettings &settings) {
            return settings.value(identifier()).toString().compare(
                       quotedCommand(), Qt::CaseInsensitive) == 0;
        }

    } // namespace

    bool isEnabled() {
        QSettings settings = runSettings();
        if (!settings.contains(identifier()))
            return false;
        // 任务管理器禁用后 Run 值还在。这时不要改写路径，避免把用户关掉的项重新打开。
        if (startupApprovedDisabled())
            return false;

        if (commandMatches(settings))
            return true;

        // 旧版本写入的是未加引号的路径，或程序换过目录。项仍然启用时，改成当前 exe 的带引号命令。
        settings.setValue(identifier(), quotedCommand());
        settings.sync();
        return settings.status() == QSettings::NoError && commandMatches(settings);
    }

    bool setEnabled(bool enabled) {
        QSettings settings = runSettings();

        if (enabled)
            settings.setValue(identifier(), quotedCommand());
        else
            settings.remove(identifier());

        settings.sync();
        if (settings.status() != QSettings::NoError)
            return false;
        // 启用和关闭都清掉禁用标记，否则任务管理器里关过一次之后，再勾选也不会启动。
        if (!clearStartupApproved())
            return false;

        QSettings verify = runSettings();
        if (enabled)
            return commandMatches(verify);
        return !verify.contains(identifier());
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

        // 写入 plist 的 <string> 内容前做 XML 转义（& 必须最先处理）
        QString xmlEscape(const QString &text) {
            QString escaped = text;
            escaped.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
            escaped.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
            escaped.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
            escaped.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
            escaped.replace(QLatin1Char('\''), QStringLiteral("&apos;"));
            return escaped;
        }
    } // namespace

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
        ).arg(xmlEscape(label), xmlEscape(exePath));

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

        // Desktop Entry 规范：含空格的路径要加双引号，\ " ` $ 需反斜杠转义
        QString desktopExecEscape(const QString &path) {
            QString escaped = path;
            escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
            escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
            escaped.replace(QLatin1Char('`'), QStringLiteral("\\`"));
            escaped.replace(QLatin1Char('$'), QStringLiteral("\\$"));
            return QStringLiteral("\"") + escaped + QStringLiteral("\"");
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
        ).arg(AutoStart::identifier(), desktopExecEscape(exePath));

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
