//
// Created by Hz6826 on 2026/10/14.
//

#include "ui/TrayController.h"
#include "ui/MainWindow.h"
#include "net/SrunClient.h"

#include <QMenu>
#include <QAction>

TrayController::TrayController(SrunClient *client, MainWindow *window,
                               QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_window(window) {
}

void TrayController::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    Q_UNUSED(reason);
}

void TrayController::onShowWindow() {
}

void TrayController::onLoginRequested() {
}

void TrayController::onLogoutRequested() {
}

void TrayController::onQuit() {
}

void TrayController::onOnlineStateChanged(int state) {
    Q_UNUSED(state);
}

void TrayController::createTray() {
}

void TrayController::createMenu() {
}

void TrayController::updateTrayIcon() {
}

void TrayController::updateMenuActions() {
}

void TrayController::showNotification(const QString &title,
                                      const QString &msg,
                                      const bool success) {
    Q_UNUSED(title);
    Q_UNUSED(msg);
    Q_UNUSED(success);
}
