//
// Created by Hz6826 on 2026/10/15.
// MainWindow 的“在线信息”Tab 相关实现 + 两个格式化工具。
//

#include "MainWindow.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QStringList>

void MainWindow::applyOnlineInfo(const QJsonObject &info) {
    const QString username = info.value("user_name").toString();
    const QString domain   = info.value("domain").toString().trimmed();

    if (!username.isEmpty()) {
        m_usernameEdit->setText(username);
    }
    // 域是本地登录参数。在线信息里的 domain 经常为空，空值不能把 @stu 清掉再写进配置。
    if (!domain.isEmpty()) {
        m_domainEdit->setText(domain.startsWith(QLatin1Char('@'))
                                  ? domain
                                  : QLatin1Char('@') + domain);
    }

    // acId / password 服务器不返回，不动

    updateOnlineInfoPanel(info);
}

void MainWindow::updateOnlineInfoPanel(const QJsonObject &info) {
    const QString user   = info.value("user_name").toString();
    const QString domain = info.value("domain").toString();
    const QString account = user.isEmpty()
                                ? "-"
                                : (domain.isEmpty() ? user : user + "@" + domain);
    m_infoAccountLabel->setText(account);

    auto textOrDash = [](const QJsonValue &v) {
        const QString s = v.toString();
        return s.isEmpty() ? QString("-") : s;
    };

    m_infoIpLabel->setText(textOrDash(info.value("online_ip")));
    m_infoMacLabel->setText(textOrDash(info.value("user_mac")));
    m_infoProductLabel->setText(textOrDash(info.value("products_name")));
    m_infoBillingLabel->setText(textOrDash(info.value("billing_name")));

    const qint64 sumBytes    = info.value("sum_bytes").toVariant().toLongLong();
    const qint64 sumSeconds  = info.value("sum_seconds").toVariant().toLongLong();
    const double balance     = info.value("user_balance").toVariant().toDouble();
    const int    deviceTotal = info.value("online_device_total").toVariant().toInt();

    m_infoUsedFlowLabel->setText(formatBytes(sumBytes));
    m_infoUsedFlowLabel->setProperty("customTooltip",
                                     QString("%1 Byte(s)").arg(sumBytes));

    m_infoUsedTimeLabel->setText(formatDuration(sumSeconds));
    m_infoUsedTimeLabel->setProperty("customTooltip",
                                     QString("%1 秒").arg(sumSeconds));

    m_infoBalanceLabel->setText(QString::number(balance, 'f', 2) + " 元");
    m_infoDeviceTotalLabel->setText(QString::number(deviceTotal) + " 台");

    const QString detailStr = info.value("online_device_detail").toString();
    QString tooltip;
    if (!detailStr.isEmpty()) {
        const QJsonDocument detailDoc = QJsonDocument::fromJson(detailStr.toUtf8());
        const QJsonObject devices = detailDoc.object();
        if (!devices.isEmpty()) {
            QStringList lines;
            lines << QString("共 %1 台设备:").arg(devices.size());
            int idx = 1;
            for (auto it = devices.constBegin(); it != devices.constEnd(); ++it, ++idx) {
                const QJsonObject dev = it.value().toObject();
                const QString os  = dev.value("os_name").toString();
                const QString cls = dev.value("class_name").toString();
                const QString ip  = dev.value("ip").toString();
                lines << QString("%1. %2 (%3)").arg(idx).arg(os, cls);
                lines << QString("    IP: %1").arg(ip);
            }
            tooltip = lines.join('\n');
        }
    }
    m_infoDeviceTotalLabel->setProperty("customTooltip", tooltip);
}

void MainWindow::clearOnlineInfoPanel() {
    m_infoAccountLabel->setText("-");
    m_infoIpLabel->setText("-");
    m_infoMacLabel->setText("-");
    m_infoProductLabel->setText("-");
    m_infoBillingLabel->setText("-");
    m_infoUsedFlowLabel->setText("-");
    m_infoUsedTimeLabel->setText("-");
    m_infoBalanceLabel->setText("-");
    m_infoDeviceTotalLabel->setText("-");
    m_infoDeviceTotalLabel->setToolTip(QString());
}

QString MainWindow::formatBytes(qint64 bytes) {
    if (bytes <= 0) return "0 B";
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    auto v = static_cast<double>(bytes);
    int i = 0;
    while (v >= 1024.0 && i < 4) {
        v /= 1024.0;
        ++i;
    }
    return QString::number(v, 'f', 2) + " " + units[i];
}

QString MainWindow::formatDuration(qint64 seconds) {
    if (seconds <= 0) return "0 秒";
    const qint64 d = seconds / 86400;
    const qint64 h = (seconds % 86400) / 3600;
    const qint64 m = (seconds % 3600) / 60;
    const qint64 s = seconds % 60;

    QStringList parts;
    if (d > 0) parts << QString::number(d) + " 天";
    if (h > 0 || d > 0) parts << QString::number(h) + " 小时";
    if (m > 0 || h > 0 || d > 0) parts << QString::number(m) + " 分";
    if (s > 0 && parts.isEmpty()) parts << QString::number(s) + " 秒";
    if (parts.isEmpty()) parts << "0 秒";
    return parts.join(" ");
}
