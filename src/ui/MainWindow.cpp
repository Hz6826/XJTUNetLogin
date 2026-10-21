//
// Created by Hz6826 on 2026/10/12.
//

// ReSharper disable CppDFAMemoryLeak
#include "MainWindow.h"

#include "AppInfo.h"
#include "ThemeUtils.h"
#include "net/SrunClient.h"

#include <QCheckBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QComboBox>
#include <QMouseEvent>
#include <QCursor>
#include <QCloseEvent>

MainWindow::MainWindow(SrunClient *client, QWidget *parent)
    : QMainWindow(parent)
    , m_client(client) {
    m_config = Config::load();
    m_client->setLogSensitive(m_config.logSensitive);

    loadIcons();
    m_lastDarkTheme = Theme::isDark();
    buildUi();
    loadConfigToUi();

    m_postLoginTimer = new QTimer(this);
    m_postLoginTimer->setSingleShot(true);

    connectSignals();
    applyLogExpanded(m_config.logExpanded);
    logWelcome();
    m_client->checkInternet();
    m_versionNam = new QNetworkAccessManager(this);
    checkVersion();

    setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);
    if (!m_config.windowGeometry.isEmpty()) {
        restoreGeometry(m_config.windowGeometry);
    }
    setWindowTitle(QString("%1 v%2 by %3")
                       .arg(AppInfo::Description, AppInfo::Version, AppInfo::Author));
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_forceQuit) {
        saveWindowGeometryToConfig();
        event->accept();
        return;
    }

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        saveWindowGeometryToConfig();
        event->accept();
        return;
    }

    switch (m_config.closeAction) {
        case CloseAction::MinimizeToTray:
            event->ignore();
            hide();
            return;
        case CloseAction::Quit:
            saveWindowGeometryToConfig();
            event->accept();
            return;
        default:
            break;
    }

    QMessageBox box(this);
    box.setWindowTitle("关闭");
    box.setIcon(QMessageBox::Question);
    box.setText("你想退出程序还是最小化到托盘？");

    const QPushButton *minBtn = box.addButton("最小化到托盘", QMessageBox::AcceptRole);
    box.addButton("退出程序", QMessageBox::DestructiveRole);  // const QPushButton *quitBtn
    const QPushButton *cancelBtn = box.addButton("取消", QMessageBox::RejectRole);
    auto *rememberBox = new QCheckBox("记住我的选择");
    box.setCheckBox(rememberBox);

    box.exec();

    QAbstractButton *clicked = box.clickedButton();
    if (clicked == cancelBtn || clicked == nullptr) {
        event->ignore();
        return;
    }

    const bool remember = rememberBox->isChecked();
    const CloseAction action = (clicked == minBtn) ? CloseAction::MinimizeToTray : CloseAction::Quit;

    if (remember) {
        if (const int idx = m_defaultCloseActionCombo->findData(static_cast<int>(action));
            idx >= 0)
            m_defaultCloseActionCombo->setCurrentIndex(idx);
        saveConfigSilently();
    }

    if (action == CloseAction::MinimizeToTray) {
        event->ignore();
        hide();
    } else {
        saveWindowGeometryToConfig();
        event->accept();
    }
}

void MainWindow::changeEvent(QEvent *event) {
    if (event->type() == QEvent::PaletteChange) {
        const bool nowDark = Theme::isDark();
        if (nowDark != m_lastDarkTheme) {
            m_lastDarkTheme = nowDark;
            loadIcons();
            if (m_toggleLogBtn)
                m_toggleLogBtn->setIcon(m_config.logExpanded ? m_iconChevronUp : m_iconChevronDown);
            if (m_clearLogBtn)
                m_clearLogBtn->setIcon(m_iconTrash);
            if (m_openLogDirBtn)
                m_openLogDirBtn->setIcon(m_iconFolder);
        }
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);

    if (m_firstShowDone) return;
    m_firstShowDone = true;

    if (!m_config.logExpanded) {
        QTimer::singleShot(0, this, [this] { resizeForLogState(false); });
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::ToolTip) {
        if (auto *widget = qobject_cast<QWidget *>(obj)) {
            QString text = widget->property("customTooltip").toString();
            if (text.isEmpty()) text = widget->toolTip();

            if (!text.isEmpty()) {
                const QPoint pos = widget->mapToGlobal(QPoint(0, widget->height() + 4));
                QToolTip::showText(pos, text, widget);
                return true;
            }
        }
    }

    if (event->type() == QEvent::MouseButtonDblClick) {
        const auto *widget  = qobject_cast<QWidget *>(obj);
        if (const auto *mouseEv = static_cast<QMouseEvent *>(event);
            widget == m_fullIconLabel && mouseEv->button() == Qt::LeftButton) {
            toggleLogSensitive();
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        const auto *widget  = qobject_cast<QWidget *>(obj);
        // ReSharper disable once CppTooWideScopeInitStatement
        const auto *mouseEv = static_cast<QMouseEvent *>(event);

        if (widget && mouseEv->button() == Qt::LeftButton) {
            if (widget == m_versionCheckLabel) {
                // 链接态放行，让 QLabel 打开链接
                if (m_versionCheckLabel->text().contains(QLatin1String("<a "))) {
                    return false;
                }
                checkVersion();
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::setForceQuit(bool force) {
    m_forceQuit = force;
}

const Config &MainWindow::config() const {
    return m_config;
}

void MainWindow::setOnlineState(OnlineState s) {
    // 浏览器等方式上线不会走到 onLoginSucceeded。进入在线就结束这一轮失败计数。
    if (s == OnlineState::Online && m_onlineState != OnlineState::Online)
        m_autoLoginFailedTrials = 0;
    m_onlineState = s;
    updateUiForState();
    emit onlineStateChanged(s);
}

void MainWindow::toggleLogSensitive() {
    m_config.logSensitive = !m_config.logSensitive;
    m_client->setLogSensitive(m_config.logSensitive);
    saveConfigSilently();
    updateLogSensitiveUi();

    if (m_config.logSensitive) {
        onLogMessage("WARN",
            "日志脱敏已关闭：后续日志将包含 token / password / chksum 等敏感信息，请勿随意分享日志文件！");
    } else {
        onLogMessage("INFO",
            "日志脱敏已开启：敏感字段将被替换。");
    }
}

void MainWindow::updateLogSensitiveUi() const {
    if (!m_fullIconLabel) return;
    if (m_config.logSensitive) {
        // 关闭脱敏：加红色边框或换图标
        m_fullIconLabel->setStyleSheet("QLabel { border: 2px solid #f44747; border-radius: 4px; }");
    } else {
        m_fullIconLabel->setStyleSheet({});
    }
    const QString state = m_config.logSensitive
        ? "已关闭（明文显示敏感信息）"
        : "已开启（敏感信息打码）";
    m_fullIconLabel->setToolTip(
        QString("双击切换日志脱敏\n当前：%1").arg(state));
}

void MainWindow::updateUiForState() {
    const bool editable = m_onlineState != OnlineState::Online
                       && m_onlineState != OnlineState::Busy;
    m_usernameEdit->setEnabled(editable);
    m_passwordEdit->setEnabled(editable);
    m_acIdEdit->setEnabled(editable);
    m_domainEdit->setEnabled(editable);

    const bool editable2 = m_onlineState != OnlineState::Busy;
    m_authBtn->setEnabled(editable2);
    m_saveBtn->setEnabled(editable2);
    m_autoLoginCheck->setEnabled(editable2);

    QPalette pal = m_authBtn->palette();
    if (m_onlineState == OnlineState::Online) {
        m_authBtn->setText("注销");
        pal.setColor(QPalette::ButtonText, QColor(0xF4, 0x47, 0x47));
        pal.setColor(QPalette::Text,       QColor(0xF4, 0x47, 0x47));
    } else {
        m_authBtn->setText("登录");
        pal.setColor(QPalette::ButtonText, QColor(0x56, 0x9C, 0xD6));
        pal.setColor(QPalette::Text,       QColor(0x56, 0x9C, 0xD6));
    }
    m_authBtn->setPalette(pal);
}

void MainWindow::connectSignals() {
    connect(m_postLoginTimer, &QTimer::timeout, this, [this] {
        m_client->checkInternet();
    });

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
    connect(m_pollTimerApplyBtn, &QPushButton::clicked,
            this, &MainWindow::onPollTimerApplyBtnClicked);
    connect(m_defaultCloseActionCombo, QOverload<int>::of(&QComboBox::activated),
            this, &MainWindow::saveConfigSilently);
    connect(m_baseUrlOverrideSaveBtn, &QPushButton::clicked,
        this, &MainWindow::onBaseUrlOverrideSaveBtnClicked);

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
    connect(m_client, &SrunClient::captivePortalDetected,
            this, &MainWindow::onCaptivePortalDetected);
}
