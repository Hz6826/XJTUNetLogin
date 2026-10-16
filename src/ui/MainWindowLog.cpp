//
// Created by Hz6826 on 2026/10/15.
// MainWindow 的日志面板 + 日志文件相关实现。
//

#include "MainWindow.h"

#include "AppInfo.h"

#include <QCheckBox>
#include <QClipboard>
#include <QDate>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QPlainTextEdit>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QUrl>

void MainWindow::onToggleLogClicked() {
    m_config.logExpanded = !m_config.logExpanded;
    applyLogExpanded(m_config.logExpanded);
}

void MainWindow::onClearLogClicked() {
    m_logTextEdit->clear();
}

void MainWindow::onOpenLogDirClicked() {
    const QString dir = logDirPath();
    if (!QDir().mkpath(dir)) {
        qWarning() << "无法创建日志目录:" << dir;
        return;
    }

    const QUrl url = QUrl::fromLocalFile(dir);
    if (!QDesktopServices::openUrl(url)) {
        qWarning() << "无法打开日志目录:" << dir;
        QGuiApplication::clipboard()->setText(QDir::toNativeSeparators(dir));
        appendLog("WARN", "无法自动打开，路径已复制到剪贴板：" + dir);
    }
}

void MainWindow::logWelcome() {
    appendLog("SUCCESS", QString("启动成功！欢迎来到%1(%2) by %3")
                             .arg(AppInfo::Description, AppInfo::Name, AppInfo::Author));
    appendLog("INFO", QString("当前版本: v%1").arg(AppInfo::Version));
    appendLog("INFO", QString("项目地址: %1").arg(AppInfo::RepoUrl));
    appendLog("INFO", QString("本项目以%1开源，完全免费！如果你认为该软件帮助到了你，"
                              "麻烦在GitHub点个Star，或将软件分享给朋友，"
                              "你的支持是我继续创作的动力~").arg(AppInfo::License));
}

void MainWindow::appendLog(const QString &level, const QString &msg) {
    writeLogToFile(level, msg);

    if (m_config.filterHttpLog && level == "HTTP") {
        return;
    }

    QColor color;
    if (level == "ERROR")        color = QColor(0xF4, 0x47, 0x47);
    else if (level == "WARN")    color = QColor(0xDC, 0xDC, 0xAA);
    else if (level == "SUCCESS") color = QColor(0x4E, 0xC9, 0x6B);
    else if (level == "STEP")    color = QColor(0x56, 0x9C, 0xD6);
    else if (level == "HTTP")    color = QColor(0x80, 0x80, 0x80);
    else                         color = QColor(0xD4, 0xD4, 0xD4);

    QTextCharFormat fmt;
    fmt.setForeground(color);

    QTextCursor cursor(m_logTextEdit->document());
    cursor.movePosition(QTextCursor::End);
    if (!m_logTextEdit->document()->isEmpty()) {
        cursor.insertBlock();
    }
    cursor.setCharFormat(fmt);
    cursor.insertText(QString("[%1] %2").arg(level, msg));

    m_logTextEdit->setTextCursor(cursor);
    m_logTextEdit->ensureCursorVisible();
}

void MainWindow::writeLogToFile(const QString &level, const QString &msg) {
    ensureLogFileForToday();
    if (!m_logFile.isOpen()) return;

    const QString ts = QDateTime::currentDateTime()
                           .toString("yyyy-MM-dd HH:mm:ss.zzz");
    const QString line = QString("[%1] [%2] %3\n").arg(ts, level, msg);
    m_logFile.write(line.toUtf8());
    m_logFile.flush();
}

void MainWindow::ensureLogFileForToday() {
    const QDate today = QDate::currentDate();
    if (m_logFile.isOpen() && m_logFileDate == today) return;
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }

    const QString dir = logDirPath();
    if (!QDir().mkpath(dir)) {
        qWarning() << "无法创建日志目录:" << dir;
        return;
    }

    const QString fullPath = QString("%1/xjtu-netlogin-%2.log")
                                 .arg(dir, today.toString("yyyyMMdd"));
    m_logFile.setFileName(fullPath);
    if (!m_logFile.open(QIODevice::Append | QIODevice::Text)) {
        qWarning() << "无法打开日志文件:" << fullPath
                   << "错误:" << m_logFile.errorString();
        return;
    }

    m_logFileDate = today;
}

QString MainWindow::logDirPath() {
    return Config::appDataDir() + "/logs";
}
