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

class TrayController : public QObject {
    Q_OBJECT

public:
    explicit TrayController(SrunClient *client, MainWindow *window,
                            QObject *parent = nullptr);

private slots:
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onShowWindow();
    void onLoginRequested();
    void onLogoutRequested();
    void onQuit();
    void onOnlineStateChanged(int state);

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
    QAction *m_loginAction = nullptr;
    QAction *m_logoutAction = nullptr;
    QAction *m_quitAction = nullptr;

    // 0=Unknown, 1=Offline, 2=Online, 3=Busy
    int m_onlineState = 0;
};

#endif // TRAYCONTROLLER_H
