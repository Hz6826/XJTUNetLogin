//
// Created by Hz6826 on 2026/10/16.
//
#include "MainWindow.h"
#include "AppInfo.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QVersionNumber>

void MainWindow::checkVersion() {
    appendLog("STEP", "→ 检查新版本");
    m_versionCheckLabel->setText(QStringLiteral("正在检查新版本…"));

    if (!m_versionNam) {
        m_versionNam = new QNetworkAccessManager(this);
    }

    QString repoPath = QString::fromUtf8(AppInfo::RepoUrl);
    repoPath.remove(QStringLiteral("https://github.com/"));
    while (repoPath.endsWith('/')) repoPath.chop(1);

    // 只查正式版
    const QString api =
        QStringLiteral("https://api.github.com/repos/%1/releases/latest")
            .arg(repoPath);

    QNetworkRequest req{QUrl(api)};
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("XJTUNetLogin/%1").arg(AppInfo::Version));
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setTransferTimeout(8000);

    appendLog("HTTP", "GET " + api);

    QNetworkReply *reply = m_versionNam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        onVersionCheckFinished(reply);
    });
}

void MainWindow::onVersionCheckFinished(QNetworkReply *reply) {
    reply->deleteLater();

    const int status = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // 404 = 仓库还没有正式 release（或全是 prerelease），不是错误
    if (status == 404) {
        appendLog("INFO", "暂无正式 release，跳过版本检查");
        m_versionCheckLabel->setText(QStringLiteral("暂无发布版本"));
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        const QString err = reply->errorString();
        appendLog("WARN", "版本检查失败: " + err);
        m_versionCheckLabel->setText(QStringLiteral("版本检查失败：%1").arg(err));
        return;
    }

    const QByteArray body = reply->readAll();
    appendLog("HTTP", QString("← 版本检查响应 (%1 bytes)").arg(body.size()));

    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
        appendLog("WARN", "版本检查响应格式错误");
        m_versionCheckLabel->setText(QStringLiteral("版本检查失败：响应格式错误"));
        return;
    }

    const QJsonObject obj = doc.object();
    const QString tag = obj.value("tag_name").toString();
    const QString url = obj.value("html_url").toString();

    if (tag.isEmpty()) {
        appendLog("WARN", "版本检查失败: 未找到 tag_name 字段");
        m_versionCheckLabel->setText(QStringLiteral("版本检查失败：未找到最新版本"));
        return;
    }

    QString latestStr = tag;
    if (latestStr.startsWith('v', Qt::CaseInsensitive)) {
        latestStr.remove(0, 1);
    }

    const QVersionNumber current = QVersionNumber::fromString(AppInfo::Version);
    const QVersionNumber latest  = QVersionNumber::fromString(latestStr);

    appendLog("INFO", QString("当前版本: %1, 最新正式版: %2")
                         .arg(AppInfo::Version, latestStr));

    if (latest > current) {
        m_hasNewVersion    = true;
        m_latestVersionUrl = url;

        appendLog("SUCCESS", "发现新版本: " + tag);

        m_versionCheckLabel->setTextFormat(Qt::RichText);
        m_versionCheckLabel->setText(QStringLiteral(
            "<a href=\"%1\">发现新版本 %2，点击前往下载</a>").arg(url, tag));
        m_versionCheckLabel->setOpenExternalLinks(true);
        m_versionCheckLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
        m_versionCheckLabel->setCursor(Qt::PointingHandCursor);
    } else {
        m_hasNewVersion = false;
        appendLog("INFO", "已是最新版本");

        m_versionCheckLabel->setText(QStringLiteral("已是最新版本 (%1)").arg(tag));
        m_versionCheckLabel->setTextFormat(Qt::PlainText);
        m_versionCheckLabel->setCursor(Qt::ArrowCursor);
    }
}