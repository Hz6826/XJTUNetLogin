#include "ui/TrayController.h"
#include "ui/MainWindow.h"
#include "net/SrunClient.h"
#include "AppInfo.h"

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QPainter>
#include <QTimer>

namespace {

    QIcon trayIconForState(OnlineState state) {
        const QString name = (state == OnlineState::Online) ? "online" : "offline";

        QIcon icon;
        icon.addFile(QString(":/icons/tray/tray-%1-16.png").arg(name));
        icon.addFile(QString(":/icons/tray/tray-%1-32.png").arg(name));
        return icon;
    }

} // namespace

TrayController::TrayController(SrunClient *client, MainWindow *window,
                               QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_window(window) {
    createMenu();
    createTray();

    m_blinkTimer = new QTimer(this);
    m_blinkTimer->setInterval(500);   // 每 500ms 切换一次
    connect(m_blinkTimer, &QTimer::timeout, this, [this] {
        m_blinkPhase = !m_blinkPhase;
        updateTrayIcon();
    });

    // 主窗口状态 → 更新托盘
    connect(m_window, &MainWindow::onlineStateChanged,
            this, &TrayController::onOnlineStateChanged);

    // 业务事件 → 弹通知
    connect(m_client, &SrunClient::loginSucceeded, this, [this] {
        showNotification("登录成功", "已连接到校园网", true);
    });
    connect(m_client, &SrunClient::loginFailed, this,
            [this](const QString &msg, const QJsonObject &) {
        showNotification("登录失败", msg, false);
    });
    connect(m_client, &SrunClient::logoutSucceeded, this, [this] {
        showNotification("已注销", "已断开校园网连接", true);
    });
    connect(m_client, &SrunClient::logoutFailed, this,
            [this](const QString &msg, const QJsonObject &) {
        showNotification("注销失败", msg, false);
    });

    updateTrayIcon();
    updateMenuActions();
}

// ============================================================
// 创建
// ============================================================

void TrayController::createTray() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning() << "系统托盘不可用，TrayController 已禁用";
        return;
    }

    m_tray = new QSystemTrayIcon(this);
    m_tray->setContextMenu(m_menu);
    m_tray->setToolTip(AppInfo::Description);
    m_tray->setIcon(trayIconForState(m_onlineState));

    m_tray->show();

    connect(m_tray, &QSystemTrayIcon::activated,
            this, &TrayController::onTrayActivated);
}

void TrayController::createMenu() {
    m_menu = new QMenu;

    m_showAction = m_menu->addAction("显示主窗口");
    connect(m_showAction, &QAction::triggered,
            this, &TrayController::onShowWindow);

    m_menu->addSeparator();

    m_authAction = m_menu->addAction("立即登录");
    connect(m_authAction, &QAction::triggered,
            this, &TrayController::onAuthRequested);

    m_menu->addSeparator();

    m_quitAction = m_menu->addAction("退出");
    connect(m_quitAction, &QAction::triggered,
            this, &TrayController::onQuit);
}

// ============================================================
// 刷新
// ============================================================

void TrayController::updateTrayIcon() {
    if (!m_tray) return;

    // Busy 状态时切换显示两种图标；否则固定显示对应状态
    OnlineState effective = m_onlineState;
    if (m_onlineState == OnlineState::Busy) {
        if (!m_blinkTimer->isActive()) {
            m_blinkTimer->start();
        }
        // 按相位选灰或红
        effective = m_blinkPhase ? OnlineState::Online
                                 : OnlineState::Offline;
    } else {
        if (m_blinkTimer->isActive()) {
            m_blinkTimer->stop();
            m_blinkPhase = false;   // 重置相位，下次进入 Busy 从灰开始
        }
    }

    m_tray->setIcon(trayIconForState(effective));
}

void TrayController::updateMenuActions() {
    if (!m_menu) return;

    const bool isOnline = (m_onlineState == OnlineState::Online);
    const bool isBusy   = (m_onlineState == OnlineState::Busy);

    // 认证动作：忙时禁用；在线时显示"立即注销"，否则"立即登录"
    m_authAction->setEnabled(!isBusy);
    m_authAction->setText(isOnline ? "立即注销" : "立即登录");
}

// ============================================================
// 通知
// ============================================================

void TrayController::showNotification(const QString &title,
                                      const QString &msg,
                                      bool success) {
    if (!m_tray) return;
    m_tray->showMessage(
        title,
        msg,
        success ? QSystemTrayIcon::Information : QSystemTrayIcon::Warning,
        3000);
}

// ============================================================
// 槽
// ============================================================

void TrayController::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
        // 左键单击：切换窗口显隐
        if (m_window->isVisible()) {
            m_window->hide();
        } else {
            m_window->show();
            m_window->raise();
            m_window->activateWindow();
        }
    }
}

void TrayController::onShowWindow() {
    m_window->show();
    m_window->raise();
    m_window->activateWindow();
}

void TrayController::onAuthRequested() {
    // 复用 MainWindow 的状态机逻辑
    m_window->onAuthClicked();
}

void TrayController::onQuit() {
    m_window->setForceQuit(true);   // 让 closeEvent 放行
    qApp->quit();
}

void TrayController::onOnlineStateChanged(OnlineState state) {
    m_onlineState = state;
    updateTrayIcon();
    updateMenuActions();
}
