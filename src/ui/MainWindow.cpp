//
// Created by Hz6826 on 2026/10/12.
//

// ReSharper disable CppDFAMemoryLeak
// ReSharper disable CppMemberFunctionMayBeConst
#include "MainWindow.h"
#include "net/SrunClient.h"
#include "AppInfo.h"

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
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
// ReSharper disable once CppUnusedIncludeDirective
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(SrunClient *client, QWidget *parent)
    : QMainWindow(parent)
    , m_client(client) {
    m_config = Config::load();

    buildUi();
    loadConfigToUi();
    connectSignals();
    applyLogExpanded(m_config.logExpanded);
    logWelcome();

    setWindowTitle(QString("%1 v.%2 by %3").arg(AppInfo::Description, AppInfo::Version, AppInfo::Author));
}

void MainWindow::closeEvent(QCloseEvent *event) {
    event->ignore();
    hide();
}

void MainWindow::logWelcome() {
    appendLog("SUCCESS", QString("启动成功！欢迎来到%1(%2) by %3")
        .arg(AppInfo::Description, AppInfo::Name, AppInfo::Author));
    // appendLog("INFO", "{彩蛋TIP的占位符}");  // TODO
    appendLog("INFO", QString("当前版本: v%1").arg(AppInfo::Version));
    // TODO 检查版本
    appendLog("INFO", QString("项目地址：%1").arg(AppInfo::RepoUrl));
    appendLog("INFO", QString("本项目以 %1 开源，完全免费！如果你认为该软件帮助到了你，"
                              "麻烦在GitHub点个Star，或将软件分享给朋友，你的支持是我继续创作的动力~").arg(AppInfo::License));
}

void MainWindow::buildUi() {
    auto *central = new QWidget;
    auto *mainLayout = new QVBoxLayout(central);

    auto *authGroup = new QGroupBox("账户信息");
    auto *form = new QFormLayout(authGroup);
    m_usernameEdit = new QLineEdit;
    m_passwordEdit = new QLineEdit;
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_acIdEdit = new QLineEdit;
    m_domainEdit = new QLineEdit;
    form->addRow("用户名", m_usernameEdit);
    form->addRow("密码", m_passwordEdit);

    auto *acIdDomainRow = new QHBoxLayout;
    auto acIdLabel = new QLabel("AC ID");
    acIdLabel->setToolTip("如果你不知道这个是做什么的，保留默认“1”即可！（其实我也不知道）");
    acIdDomainRow->addWidget(acIdLabel);
    acIdDomainRow->addWidget(m_acIdEdit);
    acIdDomainRow->addSpacing(16);
    auto domainLabel = new QLabel("域");
    domainLabel->setToolTip("学生账号请保留为默认值“@stu”");
    acIdDomainRow->addWidget(domainLabel);
    acIdDomainRow->addWidget(m_domainEdit);
    form->addRow(acIdDomainRow);

    mainLayout->addWidget(authGroup);

    auto *autoGroup = new QGroupBox("自动登录");
    auto *autoLayout = new QVBoxLayout(autoGroup);

    auto *autoLayoutR1 = new QHBoxLayout;
    m_autoStartCheck = new QCheckBox("开机自启");
    autoLayoutR1->addWidget(m_autoStartCheck);
    m_autoLoginCheck = new QCheckBox("自动登录");
    autoLayoutR1->addWidget(m_autoLoginCheck);
    autoLayout->addLayout(autoLayoutR1);

    auto *autoLayoutR2 = new QHBoxLayout;
    auto *ssidLabel = new QLabel("SSID");
    autoLayoutR2->addWidget(ssidLabel);
    m_ssidEdit = new QLineEdit;
    autoLayoutR2->addWidget(m_ssidEdit);
    autoLayout->addLayout(autoLayoutR2);

    mainLayout->addWidget(autoGroup);

    auto *buttonRow = new QHBoxLayout;
    m_loginBtn = new QPushButton("登录");
    buttonRow->addWidget(m_loginBtn, 1);
    m_logoutBtn = new QPushButton("注销");
    buttonRow->addWidget(m_logoutBtn, 1);
    m_saveBtn = new QPushButton("保存配置");
    m_saveBtn->setToolTip("登录成功时自动保存配置");
    buttonRow->addWidget(m_saveBtn, 1);

    mainLayout->addLayout(buttonRow);

    m_statusLabel = new QLabel("空闲");
    mainLayout->addWidget(m_statusLabel);

    auto *logHeaderRow = new QHBoxLayout;
    auto *logLabel = new QLabel("日志");
    logHeaderRow->addWidget(logLabel);
    logHeaderRow->addStretch();
    m_clearLogBtn = new QPushButton("清空日志");
    logHeaderRow->addWidget(m_clearLogBtn);
    m_toggleLogBtn = new QPushButton;
    logHeaderRow->addWidget(m_toggleLogBtn);

    mainLayout->addLayout(logHeaderRow);

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
    mainLayout->addWidget(m_logTextEdit, 1);

    setCentralWidget(central);
}

void MainWindow::onToggleLogClicked() {
    m_config.logExpanded = !m_config.logExpanded;
    applyLogExpanded(m_config.logExpanded);
}

void MainWindow::onLoginClicked() {
    m_config.username = m_usernameEdit->text();
    m_config.password = m_passwordEdit->text();
    m_config.acId     = m_acIdEdit->text();
    m_config.domain   = m_domainEdit->text();

    // m_logTextEdit->clear();
    appendLog("STEP", "用户点击登录");
    m_client->login(m_config.username, m_config.password,
                    m_config.acId, m_config.domain);
}

void MainWindow::onLogoutClicked() {
    m_config.username = m_usernameEdit->text();
    m_config.acId     = m_acIdEdit->text();
    m_config.domain   = m_domainEdit->text();

    appendLog("STEP", "用户点击注销");
    m_client->logout(m_config.username, m_config.acId, m_config.domain);
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

void MainWindow::onStageChanged(const QString &stage) {
    m_statusLabel->setText(stage);
}

void MainWindow::onLogMessage(const QString &level, const QString &msg) {
    appendLog(level, msg);
}

void MainWindow::onLoginSucceeded(const QJsonObject &) {
    m_statusLabel->setText("已在线");
    appendLog("SUCCESS", "登录成功");
    saveConfigSilently();

    appendLog("STEP", "3 秒后检查外网连通性...");
    QTimer::singleShot(3000, this, [this] {
        m_client->checkInternet();
    });
}

void MainWindow::onLoginFailed(const QString &msg, const QJsonObject &) {
    m_statusLabel->setText("登录失败");
    appendLog("ERROR", "登录失败: " + msg);
    if (msg.contains("Network unreachable")) {
        appendLog("ERROR", "你似乎没有连接到校园网，请检查你的网络配置");
    }
}

void MainWindow::onLogoutSucceeded(const QJsonObject &) {
    m_statusLabel->setText("已离线");
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

void MainWindow::applyLogExpanded(bool expanded) {
    m_logTextEdit->setVisible(expanded);
    m_toggleLogBtn->setText(expanded ? "▼" : "▲");
    setFixedSize(kWindowWidth,
                 expanded ? kWindowHeightExpanded : kWindowHeightCollapsed);
}

void MainWindow::appendLog(const QString &level, const QString &msg) {
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

    m_autoStartCheck->setChecked(m_config.autoStart);
    m_autoLoginCheck->setChecked(m_config.autoLogin);
    m_ssidEdit->setText(m_config.ssid);
}

bool MainWindow::saveConfigSilently() {
    m_config.username  = m_usernameEdit->text();
    m_config.password  = m_passwordEdit->text();
    m_config.acId      = m_acIdEdit->text();
    m_config.domain    = m_domainEdit->text();
    m_config.autoStart = m_autoStartCheck->isChecked();
    m_config.autoLogin = m_autoLoginCheck->isChecked();
    m_config.ssid      = m_ssidEdit->text();
    return m_config.save();
}

void MainWindow::connectSignals() {
    // UI 按钮 → 自己的槽
    connect(m_toggleLogBtn, &QPushButton::clicked,
            this, &MainWindow::onToggleLogClicked);
    connect(m_loginBtn, &QPushButton::clicked,
            this, &MainWindow::onLoginClicked);
    connect(m_logoutBtn, &QPushButton::clicked,
            this, &MainWindow::onLogoutClicked);
    connect(m_saveBtn, &QPushButton::clicked,
            this, &MainWindow::onSaveConfigClicked);
    connect(m_clearLogBtn, &QPushButton::clicked,
            this, &MainWindow::onClearLogClicked);

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
}
