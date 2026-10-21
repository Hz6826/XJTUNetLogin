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
    if (!ok) return {};

    QByteArray payload = data;
    if (payload.startsWith("\xEF\xBB\xBF")) {
        payload.remove(0, 3);
    }
    payload = payload.trimmed();
    if (payload.isEmpty()) {
        *ok = false;
        return {};
    }

    // 1) 普通 JSON：只接受对象。null/数组/字符串都视为失败，
    //    否则调用方会把空对象当成“请求成功但字段缺失”，错误信息全丢。
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error == QJsonParseError::NoError) {
        if (!doc.isObject()) {
            *ok = false;
            return {};
        }
        *ok = true;
        return doc.object();
    }

    // 2) JSONP：整串必须是 callback({...})[;]，避免贪婪匹配到错误页里的括号。
    const QString text = QString::fromUtf8(payload);
    static const QRegularExpression re(
        R"(^\s*[A-Za-z_$][\w$]*(?:\.[A-Za-z_$][\w$]*)*\s*\(\s*(\{.*\})\s*\)\s*;?\s*$)",
        QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch match = re.match(text);
    if (match.hasMatch()) {
        const QString inner = match.captured(1);
        QJsonParseError err2;
        const QJsonDocument doc2 = QJsonDocument::fromJson(inner.toUtf8(), &err2);
        if (err2.error == QJsonParseError::NoError && doc2.isObject()) {
            *ok = true;
            return doc2.object();
        }
    }

    *ok = false;
    return {};
}

QString extractServerError(const QByteArray &data) {
    bool ok = false;
    const QJsonObject obj = parseJsonOrJsonp(data, &ok);
    if (!ok) return {};

    QString msg = obj.value("error_msg").toString().trimmed();
    if (msg.isEmpty()) msg = obj.value("ploy_msg").toString().trimmed();
    if (msg.isEmpty()) msg = obj.value("msg").toString().trimmed();
    if (msg.isEmpty()) {
        const QString err = obj.value("error").toString();
        if (!err.isEmpty() && err != "ok" && err != "not_online_error")
            msg = err;
    }
    return msg;
}

QString getUAHeader() {
    return "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:157.0) Gecko/20100101 Firefox/157.0";
}

QString previewBody(const QByteArray &data, int maxLen) {
    const QString text = QString::fromUtf8(data);
    return text.size() <= maxLen ? text : text.left(maxLen) + "...";
}

QString redactSecrets(const QString &text) {
    QString out = text;

    static const QRegularExpression queryRe(
        R"(\b((?:password|info|chksum))=[^&\s]*)",
        QRegularExpression::CaseInsensitiveOption);
    out.replace(queryRe, R"(\1=***)");

    static const QRegularExpression labeledRe(
        R"(\b(token|hmd5|chksum)\s*=\s*\S+)",
        QRegularExpression::CaseInsensitiveOption);
    out.replace(labeledRe, R"(\1=***)");

    static const QRegularExpression challengeRe(
        R"("challenge"\s*:\s*"[^"]*"?)");
    out.replace(challengeRe, R"("challenge":"***")");

    static const QRegularExpression srbxRe(R"(\{SRBX1\}[A-Za-z0-9+/=]+)");
    out.replace(srbxRe, "{SRBX1}***");

    static const QRegularExpression srbxEncodedRe(
        R"(%7[Bb]SRBX1%7[Dd][^&\s]*)");
    out.replace(srbxEncodedRe, "%7BSRBX1%7D***");

    return out;
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
