//
// Created by Hz6826 on 2026/10/15.
// MainWindow 的 UI 构建部分。
//

// ReSharper disable CppDFAMemoryLeak

#include "MainWindow.h"

#include "ThemeUtils.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QToolButton>
#include <QIntValidator>
#include <QVBoxLayout>

void MainWindow::loadIcons() {
    const QString dir = Theme::isDark() ? ":/icons/dark" : ":/icons/light";
    m_iconChevronDown = QIcon(dir + "/chevron-down.svg");
    m_iconChevronUp   = QIcon(dir + "/chevron-up.svg");
    m_iconTrash       = QIcon(dir + "/trash-2.svg");
    m_iconFolder      = QIcon(dir + "/folder.svg");
}

void MainWindow::buildUi() {
    auto *central = new QWidget;
    auto *mainLayout = new QVBoxLayout(central);

    buildUiAuthForm(mainLayout);
    buildUiOpBtnRow(mainLayout);
    buildUiInfoArea(mainLayout);

    setCentralWidget(central);
    setMinimumWidth(kWindowMinWidth);
    setMinimumHeight(kWindowMinHeight);
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

    form->addWidget(usernameLabel,   0, 0);
    form->addWidget(m_usernameEdit,  0, 1, 1, 3);
    form->addWidget(passwordLabel,   1, 0);
    form->addWidget(m_passwordEdit,  1, 1, 1, 3);
    form->addWidget(acIdLabel,       2, 0);
    form->addWidget(m_acIdEdit,      2, 1);
    form->addWidget(domainLabel,     2, 2);
    form->addWidget(m_domainEdit,    2, 3);

    form->setColumnStretch(1, 1);
    form->setColumnStretch(3, 1);

    authGroup->setFixedHeight(kAuthFormHeight);
    mainLayout->addWidget(authGroup);
}

void MainWindow::buildUiOpBtnRow(QVBoxLayout *mainLayout) {
    auto *buttonRowWrapper = new QWidget;
    auto *buttonRow = new QHBoxLayout(buttonRowWrapper);
    buttonRow->setContentsMargins(0, 0, 0, 0);

    m_authBtn = new QPushButton("登录");   // 或注销，合并为一个按钮
    buttonRow->addWidget(m_authBtn, 1);

    m_pingBtn = new QPushButton("测试连接");
    buttonRow->addWidget(m_pingBtn, 1);

    m_saveBtn = new QPushButton("保存账户信息");
    m_saveBtn->setToolTip("登录成功时自动保存配置");
    buttonRow->addWidget(m_saveBtn, 1);

    buttonRowWrapper->setFixedHeight(kOpBtnRowHeight);
    mainLayout->addWidget(buttonRowWrapper);
}

void MainWindow::buildUiInfoArea(QVBoxLayout *mainLayout) {
    auto *statusRowWrapper = new QWidget;
    auto *statusRow = new QHBoxLayout(statusRowWrapper);
    statusRow->setContentsMargins(0, 0, 0, 0);

    m_statusLabel = new QLabel("空闲");
    m_statusLabel->setAlignment(Qt::AlignVCenter);
    statusRow->addWidget(m_statusLabel);
    statusRow->addStretch();

    m_toggleLogBtn = new QToolButton;
    m_toggleLogBtn->setAutoRaise(true);
    m_toggleLogBtn->installEventFilter(this);
    statusRow->addWidget(m_toggleLogBtn);

    statusRowWrapper->setFixedHeight(kStatusRowHeight);
    mainLayout->addWidget(statusRowWrapper);

    m_detailTab = new QTabWidget;

    auto *logPage = new QWidget;
    buildUiLogPanel(logPage);
    m_detailTab->addTab(logPage, "日志");

    auto *infoPage = new QWidget;
    buildUiInfoPanel(infoPage);
    m_detailTab->addTab(infoPage, "在线信息");

    auto *settingsPage = new QWidget;
    buildUiSettingsPanel(settingsPage);
    m_detailTab->addTab(settingsPage, "设置");

    auto *aboutPage = new QWidget;
    buildUiAboutPanel(aboutPage);
    m_detailTab->addTab(aboutPage, "关于");

    mainLayout->addWidget(m_detailTab, 1);
}

void MainWindow::buildUiLogPanel(QWidget *parent) {
    auto *layout = new QVBoxLayout(parent);
    layout->setContentsMargins(0, 4, 0, 0);

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
    m_openLogDirBtn->setIcon(m_iconFolder);
    m_openLogDirBtn->installEventFilter(this);
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

    m_infoAccountLabel     = makeValueLabel();
    m_infoIpLabel          = makeValueLabel();
    m_infoMacLabel         = makeValueLabel();
    m_infoProductLabel     = makeValueLabel();
    m_infoBillingLabel     = makeValueLabel();
    m_infoUsedFlowLabel    = makeValueLabel();
    m_infoUsedTimeLabel    = makeValueLabel();
    m_infoBalanceLabel     = makeValueLabel();
    m_infoDeviceTotalLabel = makeValueLabel();

    m_infoUsedFlowLabel->installEventFilter(this);
    m_infoUsedTimeLabel->installEventFilter(this);
    m_infoDeviceTotalLabel->installEventFilter(this);

    m_infoDeviceTotalLabel->setCursor(Qt::WhatsThisCursor);

    form->addRow("账号",     m_infoAccountLabel);
    form->addRow("IP",       m_infoIpLabel);
    form->addRow("MAC",      m_infoMacLabel);
    form->addRow("产品",     m_infoProductLabel);
    form->addRow("计费策略", m_infoBillingLabel);
    form->addRow("已用流量", m_infoUsedFlowLabel);
    form->addRow("已用时长", m_infoUsedTimeLabel);
    form->addRow("余额",     m_infoBalanceLabel);
    form->addRow("在线设备", m_infoDeviceTotalLabel);
}

void MainWindow::buildUiSettingsPanel(QWidget *parent) {
    auto *uiSettingsLayout = new QVBoxLayout(parent);
    buildUiAutoLogin(uiSettingsLayout);
    buildUiPollingSetting(uiSettingsLayout);

    uiSettingsLayout->addStretch();
}

void MainWindow::buildUiAutoLogin(QVBoxLayout *uiSettingsLayout) {
    auto *autoGroup = new QGroupBox("自动登录");
    auto *autoLayout = new QHBoxLayout(autoGroup);

    m_autoStartCheck = new QCheckBox("开机自启");
    m_autoLoginCheck = new QCheckBox("自动登录");
    m_autoLoginCheck->setToolTip("检测到校园网需要认证时自动登录");
    m_autoLoginCheck->installEventFilter(this);

    autoLayout->addWidget(m_autoStartCheck);
    autoLayout->addWidget(m_autoLoginCheck);

    uiSettingsLayout->addWidget(autoGroup);
}

void MainWindow::buildUiPollingSetting(QVBoxLayout *uiSettingsLayout) {
    auto *pollingGroup = new QGroupBox("轮询");
    auto *pollingLayout = new QHBoxLayout(pollingGroup);

    QLabel *pollTimerIntervalLabel = new QLabel("轮询间隔(ms)");
    QString toolTipText = QString("单位为毫秒，可以设置%1 (%2) 到%3 (%4) 间的任意整数值")
        .arg(Config::kMinPollInterval)
        .arg(formatDuration(static_cast<qint64>(Config::kMinPollInterval) / 1000))
        .arg(Config::kMaxPollInterval)
        .arg(formatDuration(static_cast<qint64>(Config::kMaxPollInterval) / 1000));
    pollTimerIntervalLabel->setToolTip(toolTipText);
    pollTimerIntervalLabel->installEventFilter(this);

    m_pollTimerIntervalEdit = new QLineEdit;
    QIntValidator *validator = new QIntValidator(Config::kMinPollInterval, Config::kMaxPollInterval, this);
    m_pollTimerIntervalEdit->setValidator(validator);
    m_pollTimerIntervalEdit->setToolTip(toolTipText);
    m_pollTimerIntervalEdit->installEventFilter(this);

    m_pollTimerApplyBtn = new QPushButton("重置并应用");
    m_pollTimerApplyBtn->setToolTip("会立即重置当前定时器，并应用新的时间（间隔未更改也会重置）");
    m_pollTimerApplyBtn->installEventFilter(this);

    pollingLayout->addWidget(pollTimerIntervalLabel);
    pollingLayout->addWidget(m_pollTimerIntervalEdit);
    pollingLayout->addWidget(m_pollTimerApplyBtn);

    uiSettingsLayout->addWidget(pollingGroup);
}

void MainWindow::buildUiAboutPanel(QWidget *parent) {
    // TODO
}
