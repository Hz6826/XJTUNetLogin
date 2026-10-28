//
// Created by Hz6826 on 2026/10/15.
// MainWindow 的配置读写、窗口几何、日志区展开/收起。
//

#include "MainWindow.h"
#include "platform/AutoStart.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QTabWidget>
#include <QToolButton>
#include <QWidget>

#include "net/SrunClient.h"

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
    m_baseUrlOverrideEdit->setText(m_config.baseUrlOverride);

    m_client->setBaseUrl(m_config.baseUrlOverride);

    const int idx = m_defaultCloseActionCombo->findData(
        static_cast<int>(m_config.closeAction));
    if (idx >= 0)
        m_defaultCloseActionCombo->setCurrentIndex(idx);
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
    if (!m_disableAutoLoginUntilRestart) {
        m_config.autoLogin = m_autoLoginCheck->isChecked();
    }

    m_config.filterHttpLog = m_filterHttpCheck->isChecked();
    m_config.currentTab    = m_detailTab->currentIndex();

    m_config.closeAction   = static_cast<CloseAction>(
        m_defaultCloseActionCombo->currentData().toInt());
    m_config.baseUrlOverride = m_baseUrlOverrideEdit->text();

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

void MainWindow::onBaseUrlOverrideSaveBtnClicked() {
    if (m_onlineState == OnlineState::Busy) {
        QMessageBox::warning(this, "请稍候", "登录/注销进行中，请结束后再修改服务器 URL");
        return;
    }
    QString text = m_baseUrlOverrideEdit->text().trimmed();
    if (!text.isEmpty()) {
        QUrl u(text);
        if (!u.isValid() || u.scheme().isEmpty() || u.host().isEmpty()) {
            QMessageBox::warning(this, "URL 无效", "请输入形如 http://host 的完整 URL");
            return;
        }
        // 去掉尾部斜杠，保证与 path 拼接一致
        while (text.endsWith('/')) text.chop(1);
        m_baseUrlOverrideEdit->setText(text);
    }
    if (m_baseUrlOverrideEdit->text().isEmpty()) {
        if (m_config.baseUrlOverride.isNull() || m_config.baseUrlOverride.isEmpty()) {
            QMessageBox::warning(this, "失败", QString("请先填入值再应用，当前已在使用默认URL: %1")
                .arg(SrunClient::kDefaultBaseUrl));
            return;
        }
        if (!saveConfigSilently()) {
            QMessageBox::critical(this, "保存失败", "配置写入失败，请检查文件权限");
            return;
        }
        m_client->setBaseUrl(m_config.baseUrlOverride);
        QMessageBox::information(this, "成功", QString("已恢复默认服务器URL为%1")
            .arg(SrunClient::kDefaultBaseUrl));
        appendLog("INFO", QString("已恢复默认服务器URL为%1")
            .arg(SrunClient::kDefaultBaseUrl));
        return;
    }
    if (!saveConfigSilently()) {
        QMessageBox::critical(this, "保存失败", "配置写入失败，请检查文件权限");
        return;
    }
    m_client->setBaseUrl(m_config.baseUrlOverride);
    QMessageBox::information(this, "成功", QString("已更新服务器URL为%1")
        .arg(m_baseUrlOverrideEdit->text()));
    appendLog("INFO", QString("已更新服务器URL为%1")
        .arg(m_baseUrlOverrideEdit->text()));
}
