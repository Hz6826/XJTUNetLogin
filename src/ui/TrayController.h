//
// Created by Hz6826 on 2026/10/14.
//

#ifndef TRAYCONTROLLER_H
#define TRAYCONTROLLER_H

#include <QObject>
#include <QSystemTrayIcon>

class SrunClient;
class MainWindow;
class QMenu;
class QAction;

// 前向声明：不 include MainWindow.h，避免循环依赖
enum class OnlineState : int;

class TrayController : public QObject {
    Q_OBJECT

public:
    explicit TrayController(SrunClient *client, MainWindow *window,
                            QObject *parent = nullptr);

private slots:
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onShowWindow();
    void onAuthRequested();      // 合并"立即登录/注销"两个菜单项
    void onQuit();
    void onOnlineStateChanged(OnlineState state);

private:
    void createTray();
    void createMenu();
    void updateTrayIcon();
    void updateMenuActions();
    void showNotification(const QString &title, const QString &msg,
                          bool success);

    SrunClient *m_client = nullptr;
    MainWindow *m_window = nullptr;

    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_showAction = nullptr;
    QAction *m_authAction = nullptr;    // ← 合并成一个动作
    QAction *m_quitAction = nullptr;

    OnlineState m_onlineState = static_cast<OnlineState>(0);  // Unknown
};

#endif // TRAYCONTROLLER_H
