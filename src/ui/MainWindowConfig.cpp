//
// Created by Hz6826 on 2026/10/15.
// MainWindow 的配置读写、窗口几何、日志区展开/收起。
//

#include "MainWindow.h"
#include "platform/AutoStart.h"

#include <QCheckBox>
#include <QLayout>
#include <QLineEdit>
#include <QTabWidget>
#include <QToolButton>
#include <QWidget>

void MainWindow::loadConfigToUi() {
    m_usernameEdit->setText(m_config.username);
    m_passwordEdit->setText(m_config.password);
    m_acIdEdit->setText(m_config.acId);
    m_domainEdit->setText(m_config.domain);

    m_autoStartCheck->setChecked(AutoStart::isEnabled());
    m_autoLoginCheck->setChecked(m_config.autoLogin);

    m_filterHttpCheck->setChecked(m_config.filterHttpLog);
    m_detailTab->setCurrentIndex(m_config.currentTab);

    m_pollTimerIntervalEdit->setPlaceholderText(QString("当前设置: %1").arg(QString::number(m_config.pollTimerIntervalMs)));
}

void MainWindow::syncAccountFieldsToConfig() {
    m_config.username = m_usernameEdit->text();
    m_config.password = m_passwordEdit->text();
    m_config.acId     = m_acIdEdit->text();
    m_config.domain   = m_domainEdit->text();
}

bool MainWindow::saveConfigSilently() {
    syncAccountFieldsToConfig();

    m_config.autoStart = m_autoStartCheck->isChecked();
    m_config.autoLogin = m_autoLoginCheck->isChecked();

    m_config.filterHttpLog = m_filterHttpCheck->isChecked();
    m_config.currentTab    = m_detailTab->currentIndex();

    return m_config.save();
}

void MainWindow::saveWindowGeometryToConfig() {
    if (m_detailTab->isVisible()) {
        m_config.expandedWindowHeight = height();
    } else {
        setMinimumHeight(0);
        setMaximumHeight(QWIDGETSIZE_MAX);
        const int targetH = m_expandedWindowHeight > 0
                                ? m_expandedWindowHeight
                                : sizeHint().height();
        resize(width(), targetH);
    }

    m_config.windowGeometry = saveGeometry();
    saveConfigSilently();
}

void MainWindow::applyLogExpanded(bool expanded) {
    const bool wasExpanded = m_detailTab->isVisible();
    if (wasExpanded == expanded) return;

    m_detailTab->setVisible(expanded);
    if (expanded) {
        m_toggleLogBtn->setIcon(m_iconChevronUp);
        m_toggleLogBtn->setToolTip("简洁");
    } else {
        m_toggleLogBtn->setIcon(m_iconChevronDown);
        m_toggleLogBtn->setToolTip("更多");
    }

    resizeForLogState(expanded);
}

void MainWindow::resizeForLogState(bool expanded) {
    if (!isVisible() || isMaximized() || isFullScreen()) return;

    const int w = width();

    if (expanded) {
        setMinimumHeight(kWindowMinHeight);
        setMaximumHeight(kWindowMaxHeight);

        resize(w, m_expandedWindowHeight > 0
                      ? m_expandedWindowHeight
                      : sizeHint().height());
    } else {
        m_expandedWindowHeight = height();

        setMinimumHeight(0);
        setMaximumHeight(kWindowMaxHeight);

        if (auto *cw = centralWidget()) {
            cw->updateGeometry();
            if (cw->layout()) cw->layout()->activate();
        }
        updateGeometry();

        const int h = sizeHint().height();
        resize(w, h);
        setFixedHeight(h);
    }
}

bool MainWindow::savePollTimerInterval(int interval) {
    m_config.pollTimerIntervalMs = interval;
    return m_config.save();
}
