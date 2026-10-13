#include "net/SrunClient.h"
#include "net/SrunCrypto.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

// ============================================================
// 构造
// ============================================================

SrunClient::SrunClient(QObject *parent)
    : QObject(parent)
      , m_nam(new QNetworkAccessManager(this)) {
}

// ============================================================
// 日志 & 阶段
// ============================================================

void SrunClient::log(const QString &level, const QString &msg) {
    emit logMessage(level, msg);
    qDebug().noquote() << QString("[%1] %2").arg(level, msg);
}

void SrunClient::setStage(const QString &stage) {
    emit stageChanged(stage);
}

// ============================================================
// 辅助
// ============================================================

QJsonObject SrunClient::parseJsonOrJsonp(const QByteArray &data, bool *ok) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error == QJsonParseError::NoError) {
        *ok = true;
        return doc.object();
    }

    const QString text = QString::fromUtf8(data);
    static const QRegularExpression re(R"(\((.*)\))",
                                       QRegularExpression::DotMatchesEverythingOption);
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

QString SrunClient::getUAHeader() {
    return "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:157.0) Gecko/20100101 Firefox/157.0";
}

void SrunClient::applyCommonHeaders(QNetworkRequest &req) {
    req.setHeader(QNetworkRequest::UserAgentHeader, getUAHeader());
    req.setRawHeader("Accept", "application/json, text/javascript, */*; q=0.01");
    req.setRawHeader("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
    req.setRawHeader("X-Requested-With", "XMLHttpRequest");
    req.setRawHeader("Referer", (kBaseUrl + QString("/srun_portal_pc")).toUtf8());
    req.setRawHeader("Origin", kBaseUrl);
}

QString SrunClient::previewBody(const QByteArray &data, int maxLen) {
    QString text = QString::fromUtf8(data);
    if (text.size() <= maxLen) {
        return text;
    }
    return text.left(maxLen) + "...";
}

QString SrunClient::buildQuery(const QList<QPair<QString, QString> > &params) {
    QStringList parts;
    parts.reserve(params.size());
    for (const auto &kv: params) {
        const QString ek = QString::fromUtf8(QUrl::toPercentEncoding(kv.first));
        const QString ev = QString::fromUtf8(QUrl::toPercentEncoding(kv.second));
        parts.append(ek + '=' + ev);
    }
    return parts.join('&');
}

// ============================================================
// 公开入口：login
// ============================================================

void SrunClient::login(const QString &username, const QString &password,
                       const QString &acId, const QString &domain) {
    m_username = username;
    m_password = password;
    m_acId = acId;
    m_domain = domain;
    m_ip.clear();
    m_token.clear();
    m_ipResolved = false;

    log("STEP", "========== 开始登录流程 ==========");
    log("INFO", QString("user=%1%2  ac_id=%3")
            .arg(m_username, m_domain, m_acId));

    requestChallenge();
}

// ============================================================
// 公开入口：logout
// ============================================================

void SrunClient::logout(const QString &username, const QString &acId,
                        const QString &domain) {
    m_username = username;
    m_acId = acId;
    m_domain = domain;

    log("STEP", "========== 开始注销流程 ==========");
    log("INFO", QString("user=%1%2  ip=%3  ac_id=%4")
            .arg(m_username, m_domain, m_ip, m_acId));

    requestLogout();
}

// ============================================================
// 公开入口：queryOnline
// ============================================================

void SrunClient::queryOnline() {
    log("STEP", "→ 查询在线状态");

    const QString fullUrl = QString("%1/cgi-bin/rad_user_info").arg(kBaseUrl);
    log("HTTP", "GET " + fullUrl);

    QNetworkRequest req{QUrl::fromEncoded(fullUrl.toUtf8())};
    applyCommonHeaders(req);
    QNetworkReply *reply = m_nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        onQueryOnlineFinished(reply);
    });
}

void SrunClient::onQueryOnlineFinished(QNetworkReply *reply) {
    const QByteArray data = reply->readAll();
    const auto netErr = reply->error();
    const QString netErrStr = reply->errorString();
    reply->deleteLater();

    if (netErr != QNetworkReply::NoError) {
        log("ERROR", "查询在线 网络错误: " + netErrStr);
        emit onlineQueryFailed("网络错误: " + netErrStr);
        return;
    }

    log("HTTP", "← 在线信息响应: " + previewBody(data, 300));

    bool ok = false;
    const QJsonObject obj = parseJsonOrJsonp(data, &ok);
    if (!ok) {
        log("ERROR", "在线信息响应解析失败");
        emit onlineQueryFailed("在线信息解析失败");
        return;
    }

    // 若本地还没有 IP，用服务器看到的 client_ip 更新
    if (m_ip.isEmpty()) {
        const QString clientIp = obj.value("client_ip").toString();
        if (!clientIp.isEmpty()) {
            m_ip = clientIp;
            log("INFO", "已获取本机 IP: " + m_ip);
        }
    }

    const QString err = obj.value("error").toString();

    if (err == "ok") {
        log("INFO", QString("当前在线: user=%1, ip=%2")
                .arg(obj.value("user_name").toString(),
                     obj.value("online_ip").toString()));
        emit onlineInfoReceived(obj);
        return;
    }

    if (err == "not_online_error") {
        log("INFO", "当前不在线");
        emit onlineQueryFailed("not_online_error");
        return;
    }

    log("ERROR", "查询在线失败: " + err);
    emit onlineQueryFailed(err);
}

// ============================================================
// 登录流程：步骤 1 - challenge
// ============================================================

void SrunClient::requestChallenge() {
    setStage("获取令牌");
    log("STEP", "→ 请求 challenge");

    const QList<QPair<QString, QString> > params = {
        {"username", m_username + m_domain},
        {"ip", m_ip},
    };

    const QString fullUrl = QString("%1/cgi-bin/get_challenge?%2")
            .arg(kBaseUrl, buildQuery(params));

    log("HTTP", "GET " + fullUrl);

    QNetworkRequest req{QUrl::fromEncoded(fullUrl.toUtf8())};
    applyCommonHeaders(req);
    QNetworkReply *reply = m_nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        onChallengeFinished(reply);
    });
}

void SrunClient::onChallengeFinished(QNetworkReply *reply) {
    const QByteArray data = reply->readAll();
    const auto netErr = reply->error();
    const QString netErrStr = reply->errorString();
    reply->deleteLater();

    if (netErr != QNetworkReply::NoError) {
        log("ERROR", "challenge 网络错误: " + netErrStr);
        emit loginFailed("网络错误: " + netErrStr, {});
        return;
    }

    log("HTTP", "← challenge 响应: " + previewBody(data));

    bool ok = false;
    const QJsonObject obj = parseJsonOrJsonp(data, &ok);
    if (!ok) {
        log("ERROR", "challenge 响应解析失败");
        emit loginFailed("challenge 响应解析失败", {});
        return;
    }

    const QString token = obj.value("challenge").toString();
    if (token.isEmpty()) {
        log("ERROR", "challenge 字段为空");
        emit loginFailed("未获取到 challenge", obj);
        return;
    }

    // 服务器视角的 IP 和本地不一致时，用服务器为准，重新请求一次 challenge
    const QString clientIp = obj.value("client_ip").toString();
    if (!m_ipResolved && !clientIp.isEmpty() && clientIp != m_ip) {
        log("INFO", QString("本地 IP=%1，服务器看到 IP=%2，以服务器为准并重新获取 token")
                .arg(m_ip.isEmpty() ? "(空)" : m_ip, clientIp));
        m_ip = clientIp;
        m_ipResolved = true;
        requestChallenge();
        return;
    }

    m_token = token;
    log("INFO", "token = " + m_token);

    requestAuth();
}

// ============================================================
// 登录流程：步骤 2 - auth
// ============================================================

void SrunClient::requestAuth() {
    setStage("认证中");
    log("STEP", "→ 请求认证");

    const QString fullUser = m_username + m_domain;

    const QString hmd5 = SrunCrypto::hmacMd5(m_password, m_token);
    const QString info = SrunCrypto::encodeUserInfo(fullUser, m_password, m_ip, m_acId, m_token);
    constexpr int n = 200;
    constexpr int type = 1;
    constexpr int doubleStack = 0;
    const QString chksum = SrunCrypto::buildChksum(
        m_token, fullUser, hmd5, m_acId, m_ip, n, type, info);

    log("INFO", "hmd5 = " + hmd5);
    log("INFO", "chksum = " + chksum);

    const QList<QPair<QString, QString> > params = {
        {"action", "login"},
        {"username", fullUser},
        {"password", "{MD5}" + hmd5},
        {"os", "Windows 10"},
        {"name", "Windows"},
        {"double_stack", QString::number(doubleStack)},
        {"chksum", chksum},
        {"info", info},
        {"ac_id", m_acId},
        {"ip", m_ip},
        {"n", QString::number(n)},
        {"type", QString::number(type)},
    };

    const QString fullUrl = QString("%1/cgi-bin/srun_portal?%2")
            .arg(kBaseUrl, buildQuery(params));

    log("HTTP", "GET " + fullUrl);

    QNetworkRequest req{QUrl::fromEncoded(fullUrl.toUtf8())};
    applyCommonHeaders(req);
    QNetworkReply *reply = m_nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        onAuthFinished(reply);
    });
}

void SrunClient::onAuthFinished(QNetworkReply *reply) {
    const QByteArray data = reply->readAll();
    const auto netErr = reply->error();
    const QString netErrStr = reply->errorString();
    reply->deleteLater();

    if (netErr != QNetworkReply::NoError) {
        log("ERROR", "auth 网络错误: " + netErrStr);
        emit loginFailed("网络错误: " + netErrStr, {});
        return;
    }

    log("HTTP", "← auth 响应: " + previewBody(data, 300));

    bool ok = false;
    const QJsonObject obj = parseJsonOrJsonp(data, &ok);
    if (!ok) {
        log("ERROR", "auth 响应解析失败");
        emit loginFailed("auth 响应解析失败", {});
        return;
    }

    const QString err = obj.value("error").toString();
    const QString sucMsg = obj.value("suc_msg").toString();
    const bool success = (err == "ok" || sucMsg == "login_ok");

    if (!success) {
        QString errMsg = obj.value("error_msg").toString();
        if (errMsg.isEmpty()) {
            errMsg = obj.value("ploy_msg").toString();
        }
        if (errMsg.isEmpty()) {
            errMsg = err;
        }
        log("ERROR", "认证失败: " + errMsg);
        emit loginFailed(errMsg, obj);
        return;
    }

    log("INFO", "认证接口返回成功，开始轮询在线状态");
    setStage("确认在线状态");

    pollOnlineStatus(3, "ok", [this](bool success, const QJsonObject &obj) {
        if (success) {
            log("INFO", QString("登录成功: user=%1, ip=%2, 产品=%3, 已用流量=%4 bytes")
                    .arg(obj.value("user_name").toString(),
                         obj.value("online_ip").toString(),
                         obj.value("products_name").toString(),
                         QString::number(obj.value("sum_bytes").toVariant().toLongLong())));
            setStage("已在线");
            emit loginSucceeded(obj);
        } else {
            log("ERROR", "登录后未上线");
            emit loginFailed("登录后未上线", obj);
        }
    });
}

// ============================================================
// 注销流程
// ============================================================

void SrunClient::requestLogout() {
    setStage("注销中");
    log("STEP", "→ 请求注销");

    const QList<QPair<QString, QString> > params = {
        {"action", "logout"},
        {"username", m_username + m_domain},
        {"ip", m_ip},
        {"ac_id", m_acId},
    };

    const QString fullUrl = QString("%1/cgi-bin/srun_portal?%2")
            .arg(kBaseUrl, buildQuery(params));

    log("HTTP", "GET " + fullUrl);

    QNetworkRequest req{QUrl::fromEncoded(fullUrl.toUtf8())};
    applyCommonHeaders(req);
    QNetworkReply *reply = m_nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        onLogoutFinished(reply);
    });
}

void SrunClient::onLogoutFinished(QNetworkReply *reply) {
    const QByteArray data = reply->readAll();
    const auto netErr = reply->error();
    const QString netErrStr = reply->errorString();
    reply->deleteLater();

    if (netErr != QNetworkReply::NoError) {
        log("ERROR", "logout 网络错误: " + netErrStr);
        emit logoutFailed("网络错误: " + netErrStr, {});
        return;
    }

    log("HTTP", "← logout 响应: " + previewBody(data, 300));

    bool ok = false;
    const QJsonObject obj = parseJsonOrJsonp(data, &ok);
    if (!ok) {
        log("ERROR", "logout 响应解析失败");
        emit logoutFailed("logout 响应解析失败", {});
        return;
    }

    const QString err = obj.value("error").toString();
    if (err != "ok") {
        QString errMsg = obj.value("error_msg").toString();
        if (errMsg.isEmpty()) {
            errMsg = obj.value("ploy_msg").toString();
        }
        if (errMsg.isEmpty()) {
            errMsg = err;
        }
        log("ERROR", "注销请求被拒绝: " + errMsg);
        emit logoutFailed(errMsg, obj);
        return;
    }

    log("INFO", "注销请求已接受，开始轮询在线状态");
    setStage("确认下线状态");

    pollOnlineStatus(3, "not_online_error", [this](bool success, const QJsonObject &obj) {
        if (success) {
            log("INFO", "已确认下线");
            setStage("已离线");
            emit logoutSucceeded(obj);
        } else {
            log("ERROR", "注销超时");
            emit logoutFailed("注销超时", obj);
        }
    });
}

// ============================================================
// 通用：在线状态轮询
// ============================================================

void SrunClient::pollOnlineStatus(int maxChecks, const QString &successError,
                                  PollCallback callback) {
    m_pollCheckCount = 0;
    m_pollMaxChecks = maxChecks;
    m_pollSuccessError = successError;
    m_pollCallback = std::move(callback);

    // 首次延迟，等服务器把状态写完
    QTimer::singleShot(kPollIntervalMs, this, [this] {
        doPollStep();
    });
}

void SrunClient::doPollStep() {
    m_pollCheckCount++;
    log("STEP", QString("→ 第 %1/%2 次查询在线状态")
            .arg(m_pollCheckCount).arg(m_pollMaxChecks));

    const QString fullUrl = QString("%1/cgi-bin/rad_user_info").arg(kBaseUrl);
    log("HTTP", "GET " + fullUrl);

    QNetworkRequest req{QUrl::fromEncoded(fullUrl.toUtf8())};
    applyCommonHeaders(req);
    QNetworkReply *reply = m_nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        onPollStepFinished(reply);
    });
}

void SrunClient::onPollStepFinished(QNetworkReply *reply) {
    const QByteArray data = reply->readAll();
    const auto netErr = reply->error();
    const QString netErrStr = reply->errorString();
    reply->deleteLater();

    if (netErr != QNetworkReply::NoError) {
        log("WARN", "轮询网络错误: " + netErrStr);
        // 网络错误不算成功也不算失败，继续重试
        if (m_pollCheckCount < m_pollMaxChecks) {
            QTimer::singleShot(kPollIntervalMs, this, [this] { doPollStep(); });
        } else {
            m_pollCallback(false, {});
        }
        return;
    }

    log("HTTP", "← 在线信息响应: " + previewBody(data, 200));

    bool ok = false;
    const QJsonObject obj = parseJsonOrJsonp(data, &ok);
    if (ok) {
        const QString err = obj.value("error").toString();
        if (err == m_pollSuccessError) {
            m_pollCallback(true, obj);
            return;
        }
    }

    if (m_pollCheckCount < m_pollMaxChecks) {
        QTimer::singleShot(kPollIntervalMs, this, [this] { doPollStep(); });
    } else {
        m_pollCallback(false, obj);
    }
}
