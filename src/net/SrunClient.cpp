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
    req.setRawHeader("Referer", (m_baseUrl + QString("/srun_portal_pc")).toUtf8());
    req.setRawHeader("Origin", m_baseUrl.toUtf8());
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

void SrunClient::emitLoginSucceeded(const QJsonObject &info) {
    m_busy = false;
    emit loginSucceeded(info);
}

void SrunClient::emitLoginFailed(const QString &msg, const QJsonObject &raw) {
    m_busy = false;
    emit loginFailed(msg, raw);
}

void SrunClient::emitLogoutSucceeded(const QJsonObject &info) {
    m_busy = false;
    emit logoutSucceeded(info);
}

void SrunClient::emitLogoutFailed(const QString &msg, const QJsonObject &raw) {
    m_busy = false;
    emit logoutFailed(msg, raw);
}

void SrunClient::doPeriodicPoll()
{
    if (m_busy) return;

    const QString fullUrl = QString("%1/cgi-bin/rad_user_info").arg(m_baseUrl);
    QNetworkRequest req{QUrl::fromEncoded(fullUrl.toUtf8())};
    applyCommonHeaders(req);
    QNetworkReply *reply = m_nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        onPeriodicPollFinished(reply);
    });
}

void SrunClient::onPeriodicPollFinished(QNetworkReply *reply)
{
    const QByteArray data = reply->readAll();
    const auto netErr = reply->error();
    reply->deleteLater();

    if (m_busy) return;

    if (netErr != QNetworkReply::NoError) {
        return;
    }

    bool ok = false;
    const QJsonObject obj = parseJsonOrJsonp(data, &ok);
    if (!ok) {
        return;
    }

    const QString err = obj.value("error").toString();
    if (err == "ok") {
        emit onlineStatusRefreshed(true, obj);
    } else if (err == "not_online_error") {
        emit onlineStatusRefreshed(false, {});
    }
    // 其他错误：静默丢弃，等下一轮
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
    m_busy = true;

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
    m_busy = true;

    log("STEP", "========== 开始注销流程 ==========");
    log("INFO", QString("user=%1%2  ip=%3  ac_id=%4")
            .arg(m_username, m_domain, m_ip, m_acId));

    requestLogout();
}

// ============================================================
// 公开入口：queryOnline
// ============================================================

void SrunClient::queryOnline(bool silent) {
    if (!silent) log("STEP", "→ 查询在线状态");

    const QString fullUrl = QString("%1/cgi-bin/rad_user_info").arg(m_baseUrl);
    if (!silent) log("HTTP", "GET " + fullUrl);

    QNetworkRequest req{QUrl::fromEncoded(fullUrl.toUtf8())};
    applyCommonHeaders(req);
    QNetworkReply *reply = m_nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, silent] {
        onQueryOnlineFinished(reply, silent);
    });
}

void SrunClient::onQueryOnlineFinished(QNetworkReply *reply, bool silent) {
    const QByteArray data = reply->readAll();
    const auto netErr = reply->error();
    const QString netErrStr = reply->errorString();
    reply->deleteLater();

    if (netErr != QNetworkReply::NoError) {
        if (!silent) {
            log("ERROR", "查询在线 网络错误: " + netErrStr);
            emit onlineQueryFailed("网络错误: " + netErrStr);
        }
        return;
    }

    if (!silent) log("HTTP", "← 在线信息响应: " + previewBody(data, 300));

    bool ok = false;
    const QJsonObject obj = parseJsonOrJsonp(data, &ok);
    if (!ok) {
        if (!silent) {
            log("ERROR", "在线信息响应解析失败");
            emit onlineQueryFailed("在线信息解析失败");
        }
        return;
    }

    // 若本地还没有 IP，用服务器看到的 client_ip 更新
    if (m_ip.isEmpty()) {
        const QString clientIp = obj.value("client_ip").toString();
        if (!clientIp.isEmpty()) {
            m_ip = clientIp;
            if (!silent) log("INFO", "已获取本机 IP: " + m_ip);
        }
    }

    const QString err = obj.value("error").toString();

    if (err == "ok") {
        if (!silent) {
            log("INFO", QString("当前在线: user=%1, ip=%2")
                .arg(obj.value("user_name").toString(),
                    obj.value("online_ip").toString()));
            emit onlineInfoReceived(obj);
        }
        emit onlineStatusRefreshed(true, obj);
        return;
    }

    if (err == "not_online_error") {
        if (!silent) {
            log("INFO", "当前不在线");
            emit onlineQueryFailed("not_online_error");
        }
        emit onlineStatusRefreshed(false, {});
        return;
    }

    if (!silent) {
        log("ERROR", "查询在线失败: " + err);
        emit onlineQueryFailed(err);
    }
}

void SrunClient::checkInternet() {
    log("STEP", "→ 检查外网连通性");

    const QString testUrl("http://firefox-portal-detection.com/generate_204");
    QNetworkRequest req{QUrl(testUrl)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    req.setTransferTimeout(5000);  // 5s
    req.setHeader(QNetworkRequest::UserAgentHeader, getUAHeader());
    QNetworkReply *reply = m_nam->get(req);

    log("HTTP", "GET " + testUrl);

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        onCheckInternetFinished(reply);
    });
}

void SrunClient::startPolling(int intervalMs) {
    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        connect(m_pollTimer, &QTimer::timeout,
            this, &SrunClient::doPeriodicPoll);
    }
    m_pollTimer->start(intervalMs);
    log("INFO", QString("在线轮询已启动，间隔 %1 ms").arg(intervalMs));

    doPeriodicPoll();
}

void SrunClient::stopPolling() {
    if (m_pollTimer) {
        m_pollTimer->stop();
    }
}

void SrunClient::onCheckInternetFinished(QNetworkReply *reply)
{
    const QByteArray data = reply->readAll();
    const auto netErr = reply->error();
    const QString netErrStr = reply->errorString();
    const int statusCode = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    // 网络层错误（DNS 失败、超时、连接拒绝等）
    if (netErr != QNetworkReply::NoError) {
        emit internetUnreachable(netErrStr);
        return;
    }

    // 204 = 真通畅
    if (statusCode == 204) {
        emit internetReachable();
        return;
    }

    // 302 或 200：可能是 captive portal
    const QByteArray locationRaw = reply->rawHeader("Location");
    if (!locationRaw.isEmpty()) {
        const QUrl locUrl = QUrl::fromEncoded(locationRaw);
        if (locUrl.isValid() && !locUrl.host().isEmpty()) {
            // 提取 scheme://host[:port]
            QString newBase = locUrl.scheme() + "://" + locUrl.host();
            if (locUrl.port() != -1 && locUrl.port() != 80) {
                newBase += ":" + QString::number(locUrl.port());
            }

            if (newBase != m_baseUrl) {
                m_baseUrl = newBase;
                log("INFO", "检测到 captive portal，服务器地址更新为：" + m_baseUrl);
            }
            emit captivePortalDetected(m_baseUrl);
            return;
        }
    }

    // 其他非 204 且无可用 Location
    const QString reason = QString("HTTP %1（可能被 captive portal 劫持）").arg(statusCode);
    emit internetUnreachable(reason);
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
            .arg(m_baseUrl, buildQuery(params));

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
        emitLoginFailed("网络错误: " + netErrStr, {});
        return;
    }

    log("HTTP", "← challenge 响应: " + previewBody(data));

    bool ok = false;
    const QJsonObject obj = parseJsonOrJsonp(data, &ok);
    if (!ok) {
        log("ERROR", "challenge 响应解析失败");
        emitLoginFailed("challenge 响应解析失败", {});
        return;
    }

    const QString token = obj.value("challenge").toString();
    if (token.isEmpty()) {
        log("ERROR", "challenge 字段为空");
        emitLoginFailed("未获取到 challenge", obj);
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
            .arg(m_baseUrl, buildQuery(params));

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
        emitLoginFailed("网络错误: " + netErrStr, {});
        return;
    }

    log("HTTP", "← auth 响应: " + previewBody(data, 300));

    bool ok = false;
    const QJsonObject obj = parseJsonOrJsonp(data, &ok);
    if (!ok) {
        log("ERROR", "auth 响应解析失败");
        emitLoginFailed("auth 响应解析失败", {});
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
        emitLoginFailed(errMsg, obj);
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
            emitLoginSucceeded(obj);
        } else {
            log("ERROR", "登录后未上线");
            emitLoginFailed("登录后未上线", obj);
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
            .arg(m_baseUrl, buildQuery(params));

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
        emitLogoutFailed("网络错误: " + netErrStr, {});
        return;
    }

    log("HTTP", "← logout 响应: " + previewBody(data, 300));

    bool ok = false;
    const QJsonObject obj = parseJsonOrJsonp(data, &ok);
    if (!ok) {
        log("ERROR", "logout 响应解析失败");
        emitLogoutFailed("logout 响应解析失败", {});
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
        emitLogoutFailed(errMsg, obj);
        return;
    }

    log("INFO", "注销请求已接受，开始轮询在线状态");
    setStage("确认下线状态");

    pollOnlineStatus(3, "not_online_error", [this](bool success, const QJsonObject &obj) {
        if (success) {
            log("INFO", "已确认下线");
            setStage("已离线");
            emitLogoutSucceeded(obj);
        } else {
            log("ERROR", "注销超时");
            emitLogoutFailed("注销超时", obj);
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

    const QString fullUrl = QString("%1/cgi-bin/rad_user_info").arg(m_baseUrl);
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
