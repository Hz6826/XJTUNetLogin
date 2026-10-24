//
// Created by Hz6826 on 2026/10/24.
// MainWindow 的主题切换：扫描 <AppData>/themes 下的主题文件夹，
// 应用其中的 styles.qss（或按明暗分开的 styles-light/dark.qss），
// 并把选择持久化到 Config::themeId。
//

#include "MainWindow.h"

#include "ThemeUtils.h"

#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>

namespace {

    // 主题 ID 就是 themes 下的文件夹名；"default" 表示系统原生样式。
    const auto kDefaultThemeId = QStringLiteral("default");

    // manifest.json 中的主题元信息
    struct ThemeManifest {
        QString name;      // 主题名
        QString version;   // 版本
        QString author;    // 作者
        QString updatedAt; // 更新时间
        QString link;      // 链接
        // 图标线条颜色：可以是单个颜色字符串，也可以是 {light, dark} 对象
        QColor iconColor;
        QColor iconColorLight;
        QColor iconColorDark;
    };

    ThemeManifest readManifest(const QString &themeDir) {
        ThemeManifest manifest;

        QFile file(themeDir + QStringLiteral("/manifest.json"));
        if (!file.open(QIODevice::ReadOnly))
            return manifest;

        const QJsonObject obj =
            QJsonDocument::fromJson(file.readAll()).object();
        manifest.name      = obj.value(QStringLiteral("name")).toString();
        manifest.version   = obj.value(QStringLiteral("version")).toString();
        manifest.author    = obj.value(QStringLiteral("author")).toString();
        manifest.updatedAt = obj.value(QStringLiteral("updatedAt")).toString();
        manifest.link      = obj.value(QStringLiteral("link")).toString();

        const QJsonValue iconColor = obj.value(QStringLiteral("iconColor"));
        if (iconColor.isObject()) {
            const QJsonObject colors = iconColor.toObject();
            manifest.iconColorLight =
                QColor(colors.value(QStringLiteral("light")).toString());
            manifest.iconColorDark =
                QColor(colors.value(QStringLiteral("dark")).toString());
        } else {
            manifest.iconColor = QColor(iconColor.toString());
        }
        return manifest;
    }

    QString themeDescription(const ThemeManifest &manifest) {
        QStringList info;
        if (!manifest.author.isEmpty())
            info << QStringLiteral("作者：%1").arg(manifest.author);
        if (!manifest.version.isEmpty())
            info << QStringLiteral("版本：%1").arg(manifest.version);
        if (!manifest.updatedAt.isEmpty())
            info << QStringLiteral("更新时间：%1").arg(manifest.updatedAt);
        if (!manifest.link.isEmpty())
            info << QStringLiteral("链接：%1").arg(manifest.link);
        return info.join(QLatin1Char('\n'));
    }

    // QSS 里的相对 url() 会按进程工作目录解析，这里统一改成绝对路径，
    // 这样主题目录中的图片资产才能被正确加载。
    QString resolveStyleSheetUrls(const QString &qss, const QString &themeDir) {
        static const QRegularExpression urlRe(
            QStringLiteral(R"(url\(\s*(['"]?)([^'")]+)\1\s*\))"));

        QList<QRegularExpressionMatch> matches;
        for (auto it = urlRe.globalMatch(qss); it.hasNext();)
            matches.append(it.next());

        QString result = qss;
        // 倒序替换，避免前半部分替换后影响后半部分匹配的下标
        for (int i = matches.size() - 1; i >= 0; --i) { // NOLINT(*-narrowing-conversions)
            const QRegularExpressionMatch &match = matches.at(i);
            const QString ref = match.captured(2).trimmed();
            if (ref.isEmpty() || ref.startsWith(QLatin1Char(':'))
                || QFileInfo(ref).isAbsolute()) {
                continue;
            }
            const QString abs =
                QDir::cleanPath(themeDir + QLatin1Char('/') + ref)
                    .replace(QLatin1Char('\\'), QLatin1Char('/'));
            result.replace(match.capturedStart(), match.capturedLength(),
                           QStringLiteral("url(\"%1\")").arg(abs));
        }
        return result;
    }

    QString colorSchemeName() {
        return Theme::colorScheme() == Qt::ColorScheme::Dark
                   ? QStringLiteral("dark")
                   : QStringLiteral("light");
    }

    // 读取主题样式（已改写 url）：优先当前明暗变体，其次通用文件，最后另一套变体。
    bool readThemeStyle(const QString &themeDir, const QString &baseName,
                        QString *qss) {
        const QString scheme = colorSchemeName();
        const QString other =
            scheme == QLatin1String("dark") ? QStringLiteral("light")
                                            : QStringLiteral("dark");
        const QStringList candidates = {
            baseName + QLatin1Char('-') + scheme + QStringLiteral(".qss"),
            baseName + QStringLiteral(".qss"),
            baseName + QLatin1Char('-') + other + QStringLiteral(".qss"),
        };

        for (const QString &name : candidates) {
            QFile file(themeDir + QLatin1Char('/') + name);
            if (file.open(QIODevice::ReadOnly)) {
                *qss = resolveStyleSheetUrls(
                    QString::fromUtf8(file.readAll()), themeDir);
                return true;
            }
        }
        return false;
    }

    bool themeHasStyleSheet(const QString &themeDir) {
        return QFile::exists(themeDir + QStringLiteral("/styles.qss"))
            || QFile::exists(themeDir + QStringLiteral("/styles-light.qss"))
            || QFile::exists(themeDir + QStringLiteral("/styles-dark.qss"));
    }

} // namespace

QString MainWindow::themesDirPath() {
    return Config::appDataDir() + QStringLiteral("/themes");
}

QColor MainWindow::themeIconColor() const {
    if (m_config.themeId.isEmpty() || m_config.themeId == kDefaultThemeId)
        return QColor();

    const ThemeManifest manifest =
        readManifest(themesDirPath() + QLatin1Char('/') + m_config.themeId);
    if (Theme::colorScheme() == Qt::ColorScheme::Dark)
        return manifest.iconColorDark.isValid() ? manifest.iconColorDark
                                                : manifest.iconColor;
    return manifest.iconColorLight.isValid() ? manifest.iconColorLight
                                             : manifest.iconColor;
}

void MainWindow::refreshTheme() {
    if (!m_themeCombo || m_themeCombo->count() == 0)
        return;

    m_config.themeId = m_themeCombo->currentData().toString();
    saveConfigSilently();
    applyTheme(m_config.themeId);
    refreshUiIcons();
}

void MainWindow::updateThemeList() {
    if (!m_themeCombo)
        return;

    const QDir themesDir(themesDirPath());
    if (!themesDir.exists())
        // ReSharper disable once CppExpressionWithoutSideEffects
        QDir().mkpath(themesDir.absolutePath()); // 先建好目录，方便用户放主题

    m_themeCombo->clear();

    // 默认主题：Qt 原生样式（Windows 11 上即 windows11）
    m_themeCombo->addItem(m_defaultStyleName.isEmpty()
                              ? QStringLiteral("默认")
                              : QStringLiteral("默认（%1）").arg(m_defaultStyleName),
                          kDefaultThemeId);
    m_themeCombo->setItemData(
        0, QStringLiteral("不使用自定义主题，切换回系统原生样式"), Qt::ToolTipRole);

    // 每个包含 styles.qss（或 styles-light/dark.qss）的子文件夹是一套主题
    const QFileInfoList entries = themesDir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries) {
        if (!themeHasStyleSheet(entry.absoluteFilePath()))
            continue;

        const ThemeManifest manifest = readManifest(entry.absoluteFilePath());

        m_themeCombo->addItem(
            manifest.name.isEmpty() ? entry.fileName() : manifest.name,
            entry.fileName());

        const QString hint = themeDescription(manifest);
        if (!hint.isEmpty())
            m_themeCombo->setItemData(m_themeCombo->count() - 1, hint,
                                      Qt::ToolTipRole);
    }

    // 恢复配置中的选择；主题已被删除时回落到默认
    int index = m_themeCombo->findData(m_config.themeId);
    if (index < 0) {
        index = 0;
        m_config.themeId = kDefaultThemeId;
    }
    m_themeCombo->setCurrentIndex(index);
}

void MainWindow::applyTheme(const QString &themeId) {
    if (themeId.isEmpty() || themeId == kDefaultThemeId) {
        qApp->setStyleSheet(QString());
        m_logTextEdit->setStyleSheet(defaultLogStyleSheet());
        appendLog("INFO", QString("当前主题：默认（%1）").arg(m_defaultStyleName));
        return;
    }

    const QString themeDir = themesDirPath() + QLatin1Char('/') + themeId;

    QString qss;
    if (!readThemeStyle(themeDir, QStringLiteral("styles"), &qss)) {
        // 主题目录被删/改名：清掉旧的 QSS，避免残留上一次的样式
        qApp->setStyleSheet(QString());
        m_logTextEdit->setStyleSheet(defaultLogStyleSheet());
        appendLog("WARN",
                  QString("无法读取主题 %1 的 styles.qss，已恢复默认样式")
                      .arg(themeId));
        return;
    }
    qApp->setStyleSheet(qss);

    // 日志区的内联样式优先级高于全局 QSS，主题可选地用 log_textedit.qss 单独覆盖；
    // 没有这个文件就沿用内置的默认配色。
    QString logQss;
    if (readThemeStyle(themeDir, QStringLiteral("log_textedit"), &logQss)) {
        m_logTextEdit->setStyleSheet(logQss);
    } else {
        m_logTextEdit->setStyleSheet(defaultLogStyleSheet());
    }

    appendLog("INFO", QString("当前主题：%1（%2）")
                         .arg(themeId, colorSchemeName()));
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void MainWindow::openThemesDir() { // NOLINT(*-convert-member-functions-to-static)
    const QString dir = themesDirPath();
    // ReSharper disable once CppExpressionWithoutSideEffects
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}
