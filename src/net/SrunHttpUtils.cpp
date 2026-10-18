//
// Created by Hz6826 on 2026/10/16.
//

#include "net/SrunHttpUtils.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QUrl>

namespace SrunHttpUtils {

QJsonObject parseJsonOrJsonp(const QByteArray &data, bool *ok) {
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error == QJsonParseError::NoError) {
        *ok = true;
        return doc.object();
    }

    const QString text = QString::fromUtf8(data);
    static const QRegularExpression re(
        R"(\((.*)\))", QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch match = re.match(text);
    if (match.hasMatch()) {
        const QString inner = match.captured(1);
        QJsonParseError err2;
        const QJsonDocument doc2 = QJsonDocument::fromJson(inner.toUtf8(), &err2);
        if (err2.error == QJsonParseError::NoError) {
            *ok = true;
            return doc2.object();
        }
    }

    *ok = false;
    return {};
}

QString getUAHeader() {
    return "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:157.0) Gecko/20100101 Firefox/157.0";
}

QString previewBody(const QByteArray &data, int maxLen) {
    const QString text = QString::fromUtf8(data);
    return text.size() <= maxLen ? text : text.left(maxLen) + "...";
}

QString buildQuery(const QList<QPair<QString, QString>> &params) {
    QStringList parts;
    parts.reserve(params.size());
    for (const auto &kv : params) {
        const QString ek = QString::fromUtf8(QUrl::toPercentEncoding(kv.first));
        const QString ev = QString::fromUtf8(QUrl::toPercentEncoding(kv.second));
        parts.append(ek + '=' + ev);
    }
    return parts.join('&');
}

void applyCommonHeaders(QNetworkRequest &req, const QString &baseUrl) {
    // challenge、认证、注销、在线查询和轮询都走这里。挂起时要结束请求，m_busy 才能被失败回调清掉。
    req.setTransferTimeout(10000);
    req.setHeader(QNetworkRequest::UserAgentHeader, getUAHeader());
    req.setRawHeader("Accept", "application/json, text/javascript, */*; q=0.01");
    req.setRawHeader("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
    req.setRawHeader("X-Requested-With", "XMLHttpRequest");
    req.setRawHeader("Referer", (baseUrl + QString("/srun_portal_pc")).toUtf8());
    req.setRawHeader("Origin", baseUrl.toUtf8());
}

} // namespace SrunHttpUtils
