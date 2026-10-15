//
// Created by Hz6826 on 2026/10/12.
//

// ReSharper disable CppDFAMemoryLeak
// ReSharper disable CppMemberFunctionMayBeConst
#include "MainWindow.h"
#include "net/SrunClient.h"
#include "AppInfo.h"
#include "platform/AutoStart.h"

#include <QCheckBox>
// ReSharper disable once CppUnusedIncludeDirective
#include <QCloseEvent>
#include <QColor>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QIcon>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
// ReSharper disable once CppUnusedIncludeDirective
#include <QApplication>
#include <QTimer>
#include <QWidget>
#include <QTabWidget>
#include <QJsonDocument>
#include <QEvent>
#include <QToolTip>
#include <QDate>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QUrl>
#include <QClipboard>
#include <QSystemTrayIcon>

namespace {
    bool isDarkTheme() {
        const QColor bg = QApplication::palette().color(QPalette::Window);
        return bg.lightness() < 128;
    }
}

MainWindow::MainWindow(SrunClient *client, QWidget *parent)
    : QMainWindow(parent)
      , m_client(client) {
    m_config = Config::load();

    loadIcons();
    m_lastDarkTheme = isDarkTheme();
    buildUi();
    loadConfigToUi();
    m_postLoginTimer = new QTimer(this);
    m_postLoginTimer->setSingleShot(true);
    connectSignals();
    applyLogExpanded(m_config.logExpanded);
    logWelcome();
    m_client->checkInternet();

    setWindowTitle(QString("%1 v.%2 by %3").arg(AppInfo::Description, AppInfo::Version, AppInfo::Author));
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_forceQuit || !QSystemTrayIcon::isSystemTrayAvailable()) {
        event->accept();
        return;
    }
    event->ignore();
    hide();
}

void MainWindow::changeEvent(QEvent *event) {
    if (event->type() == QEvent::PaletteChange) {
        const bool nowDark = isDarkTheme();
        if (nowDark != m_lastDarkTheme) {
            m_lastDarkTheme = nowDark;
            // 重新加载图标
            loadIcons();
            // 把新图标应用到所有用到的地方
            m_toggleLogBtn->setIcon(m_config.logExpanded
                                        ? m_iconChevronUp
                                        : m_iconChevronDown);
            m_clearLogBtn->setIcon(m_iconTrash);
            m_openLogDirBtn->setIcon(m_iconFolder);
        }
    }
    QMainWindow::changeEvent(event);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::ToolTip) {
        if (auto *widget = qobject_cast<QWidget *>(obj)) {
            // 优先用 customTooltip property（动态内容）
            QString text = widget->property("customTooltip").toString();

            // 没有 property 就回退到 Qt 内置 toolTip（静态内容）
            if (text.isEmpty()) {
                text = widget->toolTip();
            }

            if (!text.isEmpty()) {
                const QPoint pos = widget->mapToGlobal(
                    QPoint(0, widget->height() + 4));
                QToolTip::showText(pos, text, widget);
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::setForceQuit(bool force) {
    m_forceQuit = force;
}

void MainWindow::logWelcome() {
    appendLog("SUCCESS", QString("启动成功！欢迎来到%1(%2) by %3")
              .arg(AppInfo::Description, AppInfo::Name, AppInfo::Author));
    // appendLog("INFO", "{彩蛋TIP的占位符}");  // TODO
    appendLog("INFO", QString("当前版本: v%1").arg(AppInfo::Version));
    // TODO 检查版本
    appendLog("INFO", QString("项目地址: %1").arg(AppInfo::RepoUrl));
    appendLog("INFO", QString("本项目以%1开源，完全免费！如果你认为该软件帮助到了你，"
                  "麻烦在GitHub点个Star，或将软件分享给朋友，你的支持是我继续创作的动力~").arg(AppInfo::License));
}

void MainWindow::writeLogToFile(const QString &level, const QString &msg) {
    ensureLogFileForToday();
    if (!m_logFile.isOpen()) return;

    const QString ts = QDateTime::currentDateTime()
            .toString("yyyy-MM-dd HH:mm:ss.zzz");
    const QString line = QString("[%1] [%2] %3\n").arg(ts, level, msg);
    m_logFile.write(line.toUtf8());
    m_logFile.flush(); // 关键：防止崩溃/异常退出丢日志
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

QString MainWindow::logDirPath() const {
    return Config::appDataDir() + "/logs";
}

void MainWindow::loadIcons() {
    const QString dir = isDarkTheme()
                            ? ":/icons/dark"
                            : ":/icons/light";
    m_iconChevronDown = QIcon(dir + "/chevron-down.svg");
    m_iconChevronUp = QIcon(dir + "/chevron-up.svg");
    m_iconTrash = QIcon(dir + "/trash-2.svg");
    m_iconFolder = QIcon(dir + "/folder.svg");
}

void MainWindow::buildUiAuthForm(QVBoxLayout *mainLayout) {
    auto *authGroup = new QGroupBox("账户信息");
    auto *form = new QGridLayout(authGroup);

    auto *usernameLabel = new QLabel("用户名");
    usernameLabel->setToolTip("即学号，不用加@stu等后缀");
    usernameLabel->installEventFilter(this);
    m_usernameEdit = new QLineEdit;

    auto *passwordLabel = new QLabel("密码");
    m_passwordEdit = new QLineEdit;
    m_passwordEdit->setEchoMode(QLineEdit::Password);

    auto *acIdLabel = new QLabel("AC ID");
    acIdLabel->setToolTip("如果你不知道这个是做什么的，保留默认“1”即可！（其实我也不知道）");
    acIdLabel->installEventFilter(this);
    m_acIdEdit = new QLineEdit;

    auto *domainLabel = new QLabel("域");
    domainLabel->setToolTip("学生账号请保留为默认值“@stu”");
    domainLabel->installEventFilter(this);
    m_domainEdit = new QLineEdit;

    usernameLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    passwordLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    acIdLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    domainLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    form->addWidget(usernameLabel, 0, 0);
    form->addWidget(m_usernameEdit, 0, 1, 1, 3);
    form->addWidget(passwordLabel, 1, 0);
    form->addWidget(m_passwordEdit, 1, 1, 1, 3);
    form->addWidget(acIdLabel, 2, 0);
    form->addWidget(m_acIdEdit, 2, 1);
    form->addWidget(domainLabel, 2, 2);
    form->addWidget(m_domainEdit, 2, 3);

    form->setColumnStretch(1, 1);
    form->setColumnStretch(3, 1);

    mainLayout->addWidget(authGroup);
}

void MainWindow::buildUiAutoLogin(QVBoxLayout *mainLayout) {
    auto *autoGroup = new QGroupBox("自动登录");
    auto *autoLayout = new QHBoxLayout(autoGroup);

    m_autoStartCheck = new QCheckBox("开机自启");
    m_autoLoginCheck = new QCheckBox("自动登录");
    m_autoLoginCheck->setToolTip("检测到校园网需要认证时自动登录");
    m_autoLoginCheck->installEventFilter(this);

    autoLayout->addWidget(m_autoStartCheck);
    autoLayout->addWidget(m_autoLoginCheck);

    mainLayout->addWidget(autoGroup);
}

void MainWindow::buildUiOpBtnRow(QVBoxLayout *mainLayout) {
    auto *buttonRow = new QHBoxLayout;
    m_authBtn = new QPushButton("登录"); // 或注销，合并为一个按钮
    buttonRow->addWidget(m_authBtn, 1);
    m_pingBtn = new QPushButton("测试连接");
    buttonRow->addWidget(m_pingBtn, 1);
    m_saveBtn = new QPushButton("保存账户信息");
    m_saveBtn->setToolTip("登录成功时自动保存配置");
    buttonRow->addWidget(m_saveBtn, 1);

    mainLayout->addLayout(buttonRow);
}

void MainWindow::buildUiInfoArea(QVBoxLayout *mainLayout) {
    auto *statusRow = new QHBoxLayout;
    m_statusLabel = new QLabel("空闲");
    statusRow->addWidget(m_statusLabel);
    statusRow->addStretch();

    m_toggleLogBtn = new QToolButton;
    m_toggleLogBtn->setAutoRaise(true);
    m_toggleLogBtn->installEventFilter(this);
    statusRow->addWidget(m_toggleLogBtn);

    mainLayout->addLayout(statusRow);

    m_detailTab = new QTabWidget;

    auto *logPage = new QWidget;
    buildUiLogPanel(logPage);
    m_detailTab->addTab(logPage, "日志");

    auto *infoPage = new QWidget;
    buildUiInfoPanel(infoPage);
    m_detailTab->addTab(infoPage, "在线信息");

    mainLayout->addWidget(m_detailTab, 1);
}

void MainWindow::buildUiLogPanel(QWidget *parent) {
    auto *layout = new QVBoxLayout(parent);
    layout->setContentsMargins(0, 4, 0, 0);

    // 顶部工具行
    auto *toolRow = new QHBoxLayout;
    m_filterHttpCheck = new QCheckBox("过滤 HTTP 日志");
    toolRow->addWidget(m_filterHttpCheck);
    toolRow->addStretch();

    m_clearLogBtn = new QToolButton;
    m_clearLogBtn->setAutoRaise(true);
    m_clearLogBtn->setToolTip("清空日志");
    m_clearLogBtn->setIcon(m_iconTrash);
    m_clearLogBtn->installEventFilter(this);
    toolRow->addWidget(m_clearLogBtn);

    m_openLogDirBtn = new QToolButton;
    m_openLogDirBtn->setAutoRaise(true);
    m_openLogDirBtn->setToolTip("打开日志目录");
    m_openLogDirBtn->setIcon(m_iconFolder); // 需要新图标
    m_clearLogBtn->installEventFilter(this);
    toolRow->addWidget(m_openLogDirBtn);

    layout->addLayout(toolRow);

    m_logTextEdit = new QPlainTextEdit;
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setStyleSheet(
        "QPlainTextEdit {"
        "  background-color: #1E1E1E;"
        "  color: #D4D4D4;"
        "  border: 1px solid #3C3C3C;"
        "  font-family: Consolas, 'Courier New', monospace;"
        "  font-size: 12px;"
        "}"
    );
    layout->addWidget(m_logTextEdit, 1);
}

void MainWindow::buildUiInfoPanel(QWidget *parent) {
    auto *form = new QFormLayout(parent);
    form->setContentsMargins(40, 8, 40, 8);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(6);

    auto makeValueLabel = []() {
        auto *l = new QLabel("-");
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
        return l;
    };

    m_infoAccountLabel = makeValueLabel();
    m_infoIpLabel = makeValueLabel();
    m_infoMacLabel = makeValueLabel();
    m_infoProductLabel = makeValueLabel();
    m_infoBillingLabel = makeValueLabel();
    m_infoUsedFlowLabel = makeValueLabel();
    m_infoUsedTimeLabel = makeValueLabel();
    m_infoBalanceLabel = makeValueLabel();
    m_infoDeviceTotalLabel = makeValueLabel();

    m_infoUsedFlowLabel->installEventFilter(this);
    m_infoUsedTimeLabel->installEventFilter(this);
    m_infoDeviceTotalLabel->installEventFilter(this);

    m_infoDeviceTotalLabel->setCursor(Qt::WhatsThisCursor);

    form->addRow("账号", m_infoAccountLabel);
    form->addRow("IP", m_infoIpLabel);
    form->addRow("MAC", m_infoMacLabel);
    form->addRow("产品", m_infoProductLabel);
    form->addRow("计费策略", m_infoBillingLabel);
    form->addRow("已用流量", m_infoUsedFlowLabel);
    form->addRow("已用时长", m_infoUsedTimeLabel);
    form->addRow("余额", m_infoBalanceLabel);
    form->addRow("在线设备", m_infoDeviceTotalLabel);
}

void MainWindow::buildUi() {
    auto *central = new QWidget;
    auto *mainLayout = new QVBoxLayout(central);

    buildUiAuthForm(mainLayout);
    buildUiAutoLogin(mainLayout);
    buildUiOpBtnRow(mainLayout);
    buildUiInfoArea(mainLayout);

    setCentralWidget(central);
}

void MainWindow::onToggleLogClicked() {
    m_config.logExpanded = !m_config.logExpanded;
    applyLogExpanded(m_config.logExpanded);
}

void MainWindow::onAuthClicked() {
    m_config.username = m_usernameEdit->text();
    m_config.password = m_passwordEdit->text();
    m_config.acId = m_acIdEdit->text();
    m_config.domain = m_domainEdit->text();

    if (m_onlineState == OnlineState::Offline || m_onlineState == OnlineState::Unknown) {
        appendLog("STEP", "用户点击登录");
        setOnlineState(OnlineState::Busy);
        m_client->login(m_config.username, m_config.password, m_config.acId, m_config.domain);
    } else if (m_onlineState == OnlineState::Online) {
        appendLog("STEP", "用户点击注销");
        if (m_postLoginTimer->isActive()) {
            m_postLoginTimer->stop();
            appendLog("WARN", "取消了计划的外网连通性检查");
        }
        setOnlineState(OnlineState::Busy);
        m_client->logout(m_config.username, m_config.acId, m_config.domain);
    } else {
        // Busy
        appendLog("WARN", "当前后台正在进行其它请求，请稍后再试！");
    }
}

void MainWindow::onPingClicked() {
    m_client->checkInternet();
}

void MainWindow::onSaveConfigClicked() {
    if (saveConfigSilently()) {
        appendLog("INFO", "配置已保存到 " + Config::filePath());
    } else {
        appendLog("ERROR", "配置保存失败");
    }
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
        // 降级：复制路径到剪贴板
        QGuiApplication::clipboard()->setText(QDir::toNativeSeparators(dir));
        appendLog("WARN", "无法自动打开，路径已复制到剪贴板：" + dir);
    }
}

void MainWindow::onStageChanged(const QString &stage) {
    m_statusLabel->setText(stage);
}

void MainWindow::onLogMessage(const QString &level, const QString &msg) {
    appendLog(level, msg);
}

void MainWindow::onLoginSucceeded(const QJsonObject &info) {
    m_statusLabel->setText("已在线");
    appendLog("SUCCESS", "登录成功");
    applyOnlineInfo(info);
    setOnlineState(OnlineState::Online);
    saveConfigSilently();

    appendLog("STEP", "3 秒后检查外网连通性...");
    m_postLoginTimer->start(3000);
}

void MainWindow::onLoginFailed(const QString &msg, const QJsonObject &) {
    m_statusLabel->setText("登录失败");
    appendLog("ERROR", "登录失败: " + msg);
    if (msg.contains("Network unreachable")) {
        appendLog("ERROR", "你似乎没有连接到校园网，请检查你的网络配置");
    }
    if (msg.contains("E2901")) {
        appendLog("ERROR", "用户名或密码错误");
    }
    if (m_pendingAutoLogin) {
        m_pendingAutoLogin = false;
        m_autoLoginCheck->setChecked(false);
        appendLog("ERROR", "自动登录失败，已取消勾选：" + msg);
    }
    setOnlineState(OnlineState::Offline);
}

void MainWindow::onLogoutSucceeded(const QJsonObject &) {
    m_statusLabel->setText("已离线");
    setOnlineState(OnlineState::Offline);
    clearOnlineInfoPanel();
    m_pendingAutoLogin = false;

    if (m_autoLoginCheck->isChecked()) {
        m_autoLoginCheck->setChecked(false);
        appendLog("INFO", "已取消自动登录（用户主动注销）");
    }

    appendLog("SUCCESS", "注销成功");
}

void MainWindow::onLogoutFailed(const QString &msg, const QJsonObject &) {
    m_statusLabel->setText("注销失败");
    appendLog("ERROR", "注销失败: " + msg);
    if (msg.contains("Network unreachable")) {
        appendLog("ERROR", "你似乎没有连接到校园网，请检查你的网络配置");
    }
}

void MainWindow::onInternetReachable() {
    appendLog("SUCCESS", "外网连通 ✓");
}

void MainWindow::onInternetUnreachable(const QString &reason) {
    appendLog("WARN", "外网不通: " + reason);
}

void MainWindow::onOnlineStatusRefreshed(bool online, const QJsonObject &info) {
    if (online) {
        applyOnlineInfo(info);
        setOnlineState(OnlineState::Online);
    } else {
        clearOnlineInfoPanel();
        setOnlineState(OnlineState::Offline);
    }
}

void MainWindow::onAutoLoginToggled(bool on) {
    saveConfigSilently();
    if (!on) return;
    if (m_onlineState == OnlineState::Offline ||
        m_onlineState == OnlineState::Unknown) {
        appendLog("STEP", "自动登录已启用，尝试登录...");
        m_pendingAutoLogin = true;

        // 直接从输入框读值，不走 onAuthClicked（避免合并按钮的状态分派）
        m_config.username = m_usernameEdit->text();
        m_config.password = m_passwordEdit->text();
        m_config.acId = m_acIdEdit->text();
        m_config.domain = m_domainEdit->text();

        setOnlineState(OnlineState::Busy);
        m_client->login(m_config.username, m_config.password,
                        m_config.acId, m_config.domain);
    } else if (m_onlineState == OnlineState::Online) {
        appendLog("INFO", "已在线，无需自动登录");
    }
    // Busy 时复选框被禁用，不会触发到这里
}

void MainWindow::onAutoStartToggled(bool on) {
    const bool ok = AutoStart::setEnabled(on);
    saveConfigSilently(); // 无论成败都同步 Config（保持缓存一致）
    if (ok) {
        appendLog("INFO", on ? "已启用开机自启" : "已取消开机自启");
    } else {
        appendLog("ERROR", "开机自启设置失败，可能被安全软件阻止");
    }
}

void MainWindow::applyLogExpanded(bool expanded) {
    m_detailTab->setVisible(expanded);
    m_toggleLogBtn->setIcon(expanded ? m_iconChevronUp : m_iconChevronDown);
    m_toggleLogBtn->setToolTip(expanded ? "收起日志区" : "展开日志区");
    setFixedSize(kWindowWidth,
                 expanded ? kWindowHeightExpanded : kWindowHeightCollapsed);
}

void MainWindow::appendLog(const QString &level, const QString &msg) {
    writeLogToFile(level, msg);

    if (m_config.filterHttpLog && level == "HTTP") {
        return; // 直接丢弃，不写入
    }

    QColor color;
    if (level == "ERROR") {
        color = QColor(0xF4, 0x47, 0x47);
    } else if (level == "WARN") {
        color = QColor(0xDC, 0xDC, 0xAA);
    } else if (level == "SUCCESS") {
        color = QColor(0x4E, 0xC9, 0x6B);
    } else if (level == "STEP") {
        color = QColor(0x56, 0x9C, 0xD6);
    } else if (level == "HTTP") {
        color = QColor(0x80, 0x80, 0x80);
    } else {
        color = QColor(0xD4, 0xD4, 0xD4);
    }

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

void MainWindow::loadConfigToUi() {
    m_usernameEdit->setText(m_config.username);
    m_passwordEdit->setText(m_config.password);
    m_acIdEdit->setText(m_config.acId);
    m_domainEdit->setText(m_config.domain);

    m_autoStartCheck->setChecked(AutoStart::isEnabled());
    m_autoLoginCheck->setChecked(m_config.autoLogin);

    m_filterHttpCheck->setChecked(m_config.filterHttpLog);
    m_detailTab->setCurrentIndex(m_config.currentTab);
}

bool MainWindow::saveConfigSilently() {
    m_config.username = m_usernameEdit->text();
    m_config.password = m_passwordEdit->text();
    m_config.acId = m_acIdEdit->text();
    m_config.domain = m_domainEdit->text();
    m_config.autoStart = m_autoStartCheck->isChecked();
    m_config.autoLogin = m_autoLoginCheck->isChecked();

    m_config.filterHttpLog = m_filterHttpCheck->isChecked();
    m_config.currentTab = m_detailTab->currentIndex();

    return m_config.save();
}

void MainWindow::applyOnlineInfo(const QJsonObject &info) {
    const QString username = info.value("user_name").toString();
    const QString domain = info.value("domain").toString();
    const QString domainWithAt = domain.isEmpty() ? QString() : ("@" + domain);

    if (!username.isEmpty()) {
        m_usernameEdit->setText(username);
    }
    m_domainEdit->setText(domainWithAt);

    // acId / password 服务器不返回，不动

    // 填 Tab 2
    updateOnlineInfoPanel(info);
}

void MainWindow::setOnlineState(OnlineState s) {
    m_onlineState = s;
    updateUiForState();
    emit onlineStateChanged(s);
}

void MainWindow::updateUiForState() {
    const bool editable = m_onlineState != OnlineState::Online && m_onlineState != OnlineState::Busy;
    m_usernameEdit->setEnabled(editable);
    m_passwordEdit->setEnabled(editable);
    m_acIdEdit->setEnabled(editable);
    m_domainEdit->setEnabled(editable);

    const bool editable2 = m_onlineState != OnlineState::Busy;
    m_authBtn->setEnabled(editable2);
    m_saveBtn->setEnabled(editable2);
    m_autoLoginCheck->setEnabled(editable2);

    // 按状态切换按钮文案和配色
    QPalette pal = m_authBtn->palette();
    if (m_onlineState == OnlineState::Online) {
        m_authBtn->setText("注销");
        pal.setColor(QPalette::ButtonText, QColor(0xF4, 0x47, 0x47)); // 红
        pal.setColor(QPalette::Text, QColor(0xF4, 0x47, 0x47)); // fallback
    } else {
        m_authBtn->setText("登录");
        pal.setColor(QPalette::ButtonText, QColor(0x56, 0x9C, 0xD6)); // 蓝
        pal.setColor(QPalette::Text, QColor(0x56, 0x9C, 0xD6)); // fallback
    }
    m_authBtn->setPalette(pal);
}

void MainWindow::updateOnlineInfoPanel(const QJsonObject &info) {
    // 账号
    const QString user = info.value("user_name").toString();
    const QString domain = info.value("domain").toString();
    const QString account = user.isEmpty()
                                ? "-"
                                : (domain.isEmpty() ? user : user + "@" + domain);
    m_infoAccountLabel->setText(account);

    // 直接读的字段
    auto textOrDash = [](const QJsonValue &v) {
        const QString s = v.toString();
        return s.isEmpty() ? QString("-") : s;
    };

    m_infoIpLabel->setText(textOrDash(info.value("online_ip")));
    m_infoMacLabel->setText(textOrDash(info.value("user_mac")));
    m_infoProductLabel->setText(textOrDash(info.value("products_name")));
    m_infoBillingLabel->setText(textOrDash(info.value("billing_name")));

    // 数值字段：统一走 toVariant()，兼容字符串和数字两种类型
    const qint64 sumBytes = info.value("sum_bytes").toVariant().toLongLong();
    const qint64 sumSeconds = info.value("sum_seconds").toVariant().toLongLong();
    const double balance = info.value("user_balance").toVariant().toDouble();
    const int deviceTotal = info.value("online_device_total").toVariant().toInt();

    m_infoUsedFlowLabel->setText(formatBytes(sumBytes));
    m_infoUsedFlowLabel->setProperty("customTooltip",
                                     QString("%1 Byte(s)").arg(sumBytes));

    m_infoUsedTimeLabel->setText(formatDuration(sumSeconds));
    m_infoUsedTimeLabel->setProperty("customTooltip",
                                     QString("%1 秒").arg(sumSeconds));

    m_infoBalanceLabel->setText(QString::number(balance, 'f', 2) + " 元");
    m_infoDeviceTotalLabel->setText(QString::number(deviceTotal) + " 台");

    // tooltip
    const QString detailStr = info.value("online_device_detail").toString();
    QString tooltip;
    if (!detailStr.isEmpty()) {
        const QJsonDocument detailDoc = QJsonDocument::fromJson(detailStr.toUtf8());
        const QJsonObject devices = detailDoc.object();
        if (!devices.isEmpty()) {
            QStringList lines;
            lines << QString("共 %1 台设备:").arg(devices.size());
            int idx = 1;
            for (auto it = devices.constBegin(); it != devices.constEnd(); ++it, ++idx) {
                const QJsonObject dev = it.value().toObject();
                const QString os = dev.value("os_name").toString();
                const QString cls = dev.value("class_name").toString();
                const QString ip = dev.value("ip").toString();
                lines << QString("%1. %2 (%3)").arg(idx).arg(os, cls);
                lines << QString("    IP: %1").arg(ip);
            }
            tooltip = lines.join('\n');
        }
    }
    m_infoDeviceTotalLabel->setProperty("customTooltip", tooltip);
}

void MainWindow::clearOnlineInfoPanel() {
    m_infoAccountLabel->setText("-");
    m_infoIpLabel->setText("-");
    m_infoMacLabel->setText("-");
    m_infoProductLabel->setText("-");
    m_infoBillingLabel->setText("-");
    m_infoUsedFlowLabel->setText("-");
    m_infoUsedTimeLabel->setText("-");
    m_infoBalanceLabel->setText("-");
    m_infoDeviceTotalLabel->setText("-");
    m_infoDeviceTotalLabel->setToolTip(QString());
}

QString MainWindow::formatBytes(qint64 bytes) {
    if (bytes <= 0) return "0 B";
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double v = static_cast<double>(bytes);
    int i = 0;
    while (v >= 1024.0 && i < 4) {
        v /= 1024.0;
        ++i;
    }
    return QString::number(v, 'f', 2) + " " + units[i];
}

QString MainWindow::formatDuration(qint64 seconds) {
    if (seconds <= 0) return "0 秒";
    const qint64 d = seconds / 86400;
    const qint64 h = (seconds % 86400) / 3600;
    const qint64 m = (seconds % 3600) / 60;
    const qint64 s = seconds % 60;

    QStringList parts;
    if (d > 0) parts << QString::number(d) + " 天";
    if (h > 0 || d > 0) parts << QString::number(h) + " 小时";
    if (m > 0 || h > 0 || d > 0) parts << QString::number(m) + " 分";
    // 只有总时长小于 1 分钟才显示秒，否则秒没有意义
    if (s > 0 && parts.isEmpty()) parts << QString::number(s) + " 秒";
    if (parts.isEmpty()) parts << "0 秒";
    return parts.join(" ");
}

void MainWindow::connectSignals() {
    connect(m_postLoginTimer, &QTimer::timeout, this, [this] {
        m_client->checkInternet();
    });

    // UI 按钮 → 自己的槽
    connect(m_toggleLogBtn, &QToolButton::clicked,
            this, &MainWindow::onToggleLogClicked);
    connect(m_authBtn, &QPushButton::clicked,
            this, &MainWindow::onAuthClicked);
    connect(m_pingBtn, &QPushButton::clicked,
            this, &MainWindow::onPingClicked);
    connect(m_saveBtn, &QPushButton::clicked,
            this, &MainWindow::onSaveConfigClicked);
    connect(m_clearLogBtn, &QToolButton::clicked,
            this, &MainWindow::onClearLogClicked);
    connect(m_filterHttpCheck, &QCheckBox::toggled,
            this, [this] { saveConfigSilently(); });
    connect(m_detailTab, &QTabWidget::currentChanged,
            this, [this] { saveConfigSilently(); });
    connect(m_openLogDirBtn, &QToolButton::clicked,
            this, &MainWindow::onOpenLogDirClicked);
    connect(m_autoStartCheck, &QCheckBox::toggled,
            this, &MainWindow::onAutoStartToggled);
    connect(m_autoLoginCheck, &QCheckBox::toggled,
            this, &MainWindow::onAutoLoginToggled);

    // SrunClient 信号 → 自己的槽
    connect(m_client, &SrunClient::logMessage,
            this, &MainWindow::onLogMessage);
    connect(m_client, &SrunClient::stageChanged,
            this, &MainWindow::onStageChanged);
    connect(m_client, &SrunClient::loginSucceeded,
            this, &MainWindow::onLoginSucceeded);
    connect(m_client, &SrunClient::loginFailed,
            this, &MainWindow::onLoginFailed);
    connect(m_client, &SrunClient::logoutSucceeded,
            this, &MainWindow::onLogoutSucceeded);
    connect(m_client, &SrunClient::logoutFailed,
            this, &MainWindow::onLogoutFailed);
    connect(m_client, &SrunClient::onlineStatusRefreshed,
            this, &MainWindow::onOnlineStatusRefreshed);
    connect(m_client, &SrunClient::internetReachable,
            this, &MainWindow::onInternetReachable);
    connect(m_client, &SrunClient::internetUnreachable,
            this, &MainWindow::onInternetUnreachable);
}
