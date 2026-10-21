//
// Created by Hz6826 on 2026/10/15.
// MainWindow 的登录/注销、自动登录、网络事件相关槽函数。
//

#include "MainWindow.h"

#include "net/SrunClient.h"
#include "platform/AutoStart.h"

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>

void MainWindow::onAuthClicked() {
    syncAccountFieldsToConfig();

    if (m_onlineState == OnlineState::Offline ||
        m_onlineState == OnlineState::Unknown) {
        appendLog("STEP", "用户点击登录");
        setOnlineState(OnlineState::Busy);
        m_client->login(m_config.username, m_config.password,
                        m_config.acId, m_config.domain);
    } else if (m_onlineState == OnlineState::Online) {
        appendLog("STEP", "用户点击注销");
        if (m_postLoginTimer->isActive()) {
            m_postLoginTimer->stop();
            appendLog("WARN", "取消了计划的外网连通性检查");
        }
        setOnlineState(OnlineState::Busy);
        m_client->logout(m_config.username, m_config.acId, m_config.domain);
    } else {
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

void MainWindow::triggerAutoLogin() {
    syncAccountFieldsToConfig();

    if (m_config.username.trimmed().isEmpty() || m_config.password.isEmpty()) {
        if (!m_autoLoginMissingCredsNoted) {
            appendLog("WARN", "未填写账号或密码，已跳过自动登录");
            m_autoLoginMissingCredsNoted = true;
        }
        return;
    }
    m_autoLoginMissingCredsNoted = false;

    if (m_autoLoginFailedTrials >= kMaxAutoLoginFailNotifyCount)
        return;

    appendLog("STEP", "自动登录触发，尝试登录...");
    m_pendingAutoLogin = true;

    setOnlineState(OnlineState::Busy);
    m_client->login(m_config.username, m_config.password,
                    m_config.acId, m_config.domain);
}

bool MainWindow::shouldAttemptAutoLogin() {
    if (!m_autoLoginCheck->isChecked())
        return false;
    if (m_onlineState == OnlineState::Online || m_onlineState == OnlineState::Busy)
        return false;
    if (m_autoLoginFailedTrials >= kMaxAutoLoginFailNotifyCount)
        return false;
    if (m_usernameEdit->text().trimmed().isEmpty() || m_passwordEdit->text().isEmpty()) {
        if (!m_autoLoginMissingCredsNoted) {
            appendLog("WARN", "未填写账号或密码，已跳过自动登录");
            m_autoLoginMissingCredsNoted = true;
        }
        return false;
    }
    m_autoLoginMissingCredsNoted = false;
    return true;
}

void MainWindow::onStageChanged(const QString &stage) {
    m_statusLabel->setText(stage);
}

void MainWindow::onLogMessage(const QString &level, const QString &msg) {
    appendLog(level, msg);
}

void MainWindow::onLoginSucceeded(const QJsonObject &info) {
    m_pendingAutoLogin = false;
    m_autoLoginFailedTrials = 0;
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

    const bool autoAttempt = m_pendingAutoLogin;
    m_pendingAutoLogin = false;
    if (autoAttempt) {
        if (msg.contains(QStringLiteral("E2901"))) {
            m_autoLoginFailedTrials = kMaxAutoLoginFailNotifyCount;
            appendLog("WARN", "账号或密码错误，已暂停自动登录。关闭再打开该选项后可重试");
        } else {
            m_autoLoginFailedTrials += 1;
            if (m_autoLoginFailedTrials >= kMaxAutoLoginFailNotifyCount) {
                appendLog("WARN", "自动登录已连续失败，已暂停。关闭再打开该选项后可重试");
            }
        }
        appendLog("ERROR", "自动登录失败：" + msg);
        if (m_autoLoginFailedTrials < kMaxAutoLoginFailNotifyCount)
            emit loginFailedNotify(msg);
    } else {
        emit loginFailedNotify(msg);
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

    setOnlineState(OnlineState::Offline);
    m_client->queryOnline(true);
}

void MainWindow::onInternetReachable() {
    appendLog("SUCCESS", "外网连通 ✓");
}

void MainWindow::onInternetUnreachable(const QString &reason) {
    appendLog("WARN", "外网不通: " + reason);
}

void MainWindow::onOnlineStatusRefreshed(bool online, const QJsonObject &info) {
    if (m_onlineState == OnlineState::Busy) {
        return;
    }

    if (online) {
        applyOnlineInfo(info);
        setOnlineState(OnlineState::Online);
    } else {
        clearOnlineInfoPanel();
        setOnlineState(OnlineState::Offline);

        if (shouldAttemptAutoLogin()) {
            appendLog("STEP", "检测到离线，探测校园网认证需求...");
            m_client->checkInternet();
        }
    }
}

void MainWindow::onAutoLoginToggled(bool on) {
    m_autoLoginFailedTrials = 0;
    m_autoLoginMissingCredsNoted = false;
    if (!on && m_onlineState != OnlineState::Busy)
        m_pendingAutoLogin = false;
    saveConfigSilently();
    if (!on) return;
    if (m_onlineState == OnlineState::Offline ||
        m_onlineState == OnlineState::Unknown) {
        triggerAutoLogin();
    } else if (m_onlineState == OnlineState::Online) {
        appendLog("INFO", "已在线，无需自动登录");
    }
}

void MainWindow::onAutoStartToggled(bool on) {
    const bool ok = AutoStart::setEnabled(on);
    saveConfigSilently();
    if (ok) {
        appendLog("INFO", on ? "已启用开机自启" : "已取消开机自启");
    } else {
        appendLog("ERROR", "开机自启设置失败，可能被安全软件阻止");
    }
}

void MainWindow::onCaptivePortalDetected(const QString &newBaseUrl) {
    appendLog("INFO", "检测到 captive portal：" + newBaseUrl);

    if (!shouldAttemptAutoLogin())
        return;

    triggerAutoLogin();
}

void MainWindow::onPollTimerApplyBtnClicked() {
    bool ok = false;
    int interval = m_pollTimerIntervalEdit->text().toInt(&ok);
    if (!ok) {
        QMessageBox::critical(this, "错误", "数字解析失败！");
        m_pollTimerIntervalEdit->setFocus();
        m_pollTimerIntervalEdit->selectAll();
        return;
    }

    if (interval < Config::kMinPollInterval || interval > Config::kMaxPollInterval) {
        const QString detail = QString("请输入%1 (%2) 到%3 (%4) 间的整数值！")
            .arg(Config::kMinPollInterval)
            .arg(formatDuration(static_cast<qint64>(Config::kMinPollInterval) / 1000))
            .arg(Config::kMaxPollInterval)
            .arg(formatDuration(static_cast<qint64>(Config::kMaxPollInterval) / 1000));
        QMessageBox::critical(this, "错误", detail);
        m_pollTimerIntervalEdit->setFocus();
        m_pollTimerIntervalEdit->selectAll();
        return;
    }

    m_client->applyPollingIntervalAndRestart(interval);
    if (!savePollTimerInterval(interval)) {
        appendLog("WARN", "轮询间隔已生效，但保存到配置文件失败");
    }
    QString successTip = QString("成功设置间隔时间为 %1ms (%2) 并重启轮询计时器！")
        .arg(interval)
        .arg(formatDuration(static_cast<qint64>(interval) / 1000));
    appendLog("SUCCESS", successTip);
    QMessageBox::information(this, "成功", successTip);
    m_pollTimerIntervalEdit->setPlaceholderText(QString("当前设置: %1").arg(QString::number(interval)));
    m_pollTimerIntervalEdit->setText(QString::number(interval));
}
