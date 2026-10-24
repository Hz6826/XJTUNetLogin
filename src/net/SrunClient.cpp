#include "net/SrunClient.h"
#include "net/SrunCrypto.h"
#include "net/SrunHttpUtils.h"

#include <QDebug>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QNetworkProxy>
#include <QPointer>

namespace {
// 响应体日志预览：截断并去掉可能还原密码的材料（challenge / SRBX1 等）
QString previewForLog(const QByteArray &data, int maxLen = 300) {
    return SrunHttpUtils::redactSecrets(
        SrunHttpUtils::previewBody(data, maxLen));
}
} // namespace

// ============================================================
// 构造
// ============================================================

SrunClient::SrunClient(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this)) {
    m_nam->setProxy(QNetworkProxy::NoProxy);
    m_nam->setTransferTimeout(kTransferTimeoutMs);   // 统一 12s
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

void SrunClient::applyCommonHeaders(QNetworkRequest &req) const {
    SrunHttpUtils::applyCommonHeaders(req, baseUrl());
}

void SrunClient::emitLoginSucceeded(const QJsonObject &info) {
    if (m_cancelled) return;
    disarmWatchdog();
    m_operation = OpNone;
    m_busy = false;
    emit loginSucceeded(info);
}

void SrunClient::emitLoginFailed(const QString &msg, const QJsonObject &raw) {
    if (m_cancelled) return;
    disarmWatchdog();
    m_operation = OpNone;
    m_busy = false;
    emit loginFailed(msg, raw);
}

void SrunClient::emitLogoutSucceeded(const QJsonObject &info) {
    if (m_cancelled) return;
    disarmWatchdog();
    m_operation = OpNone;
    m_busy = false;
    emit logoutSucceeded(info);
}

void SrunClient::emitLogoutFailed(const QString &msg, const QJsonObject &raw) {
    if (m_cancelled) return;
    disarmWatchdog();
    m_operation = OpNone;
    m_busy = false;
    emit logoutFailed(msg, raw);
}

void SrunClient::finishPoll(bool success, const QJsonObject &obj) {
    // 先取走再调用：回调里如果同步开启新流程，不会覆盖正在执行的闭包
    PollCallback cb = std::move(m_pollCallback);
    m_pollCallback = nullptr;
    if (cb) cb(success, obj);
}

void SrunClient::updateIpFromOnlineInfo(const QJsonObject &obj) {
    if (!m_ip.isEmpty()) return;

    QString ip = obj.value("client_ip").toString();
    if (ip.isEmpty()) ip = obj.value("online_ip").toString();
    if (ip.isEmpty()) return;

    m_ip = ip;
    log("INFO", "已获取本机 IP: " + m_ip);
}

QString SrunClient::maskIfNeeded(const QString &text) const {
    if (text.isEmpty() || m_logSensitive) return text;

    if (text.startsWith(QLatin1String("http://")) ||
        text.startsWith(QLatin1String("https://"))) {
        // 保留 path/参数名，只打码 password/info/chksum 等敏感值
        return SrunHttpUtils::redactSecrets(text);
    }
    return QStringLiteral("****");
}

// ============================================================
// 看门狗 & 取消
// ============================================================

void SrunClient::attachOpReply(QNetworkReply *reply) {
    m_opReplies.append(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        m_opReplies.removeAll(QPointer<QNetworkReply>(reply));
    });
}

void SrunClient::abortOpReplies() {
    // abort() 会同步 emit finished，所以先拷贝快照
    const auto snapshot = m_opReplies;
    m_opReplies.clear();
    for (const auto &r : snapshot) {
        if (r) r->abort();
    }
}

void SrunClient::armWatchdog(int timeoutMs) {
    if (!m_watchdog) {
        m_watchdog = new QTimer(this);
        m_watchdog->setSingleShot(true);
        connect(m_watchdog, &QTimer::timeout,
                this, &SrunClient::onWatchdogTimeout);
    }
    m_watchdog->start(timeoutMs);
}

void SrunClient::disarmWatchdog() const {
    if (m_watchdog) m_watchdog->stop();
}

void SrunClient::onWatchdogTimeout() {
    const Operation op = m_operation;
    m_operation = OpNone;

    log("ERROR", "操作超时（看门狗触发），中止本次请求");

    m_cancelled = true;
    abortOpReplies();
    m_cancelled = false;
    m_pollCallback = nullptr;
    disarmWatchdog();

    switch (op) {
    case OpLogin:
        emitLoginFailed("登录超时（看门狗）", {});
        break;
    case OpLogout:
        emitLogoutFailed("注销超时（看门狗）", {});
        break;
    default:
        m_busy = false;
        emit operationCancelled();
        break;
    }
}

void SrunClient::cancel() {
    if (m_operation == OpNone) return;

    log("INFO", "用户取消当前操作");

    m_operation = OpNone;
    m_cancelled = true;
    abortOpReplies();          // abort 触发 finished，回调里 m_canceled=true 直接 return
    disarmWatchdog();
    m_cancelled = false;
    m_busy = false;
    m_pollCallback = nullptr;

    emit operationCancelled();
}

void SrunClient::reset() {
    // 静默中止：不 emit operationCancelled，避免 reset 时 UI 误弹“操作已取消”
    if (m_operation != OpNone) {
        log("INFO", "reset: 中止当前操作");
        m_operation = OpNone;
        m_cancelled = true;
        abortOpReplies();
        disarmWatchdog();
        m_cancelled = false;
    }

    if (m_pollTimer) m_pollTimer->stop();
    m_periodicPollInFlight = false;
    m_periodicFailureCount = 0;
    m_pollCallback = nullptr;
    if (m_periodicReply) {
        m_periodicReply->abort();
        m_periodicReply = nullptr;
    }

    m_ip.clear();
    m_token.clear();
    m_ipResolved = false;
    m_busy = false;
    m_operation = OpNone;
    setStage("空闲");
}

// ============================================================
// 公开入口：login
// ============================================================

void SrunClient::login(const QString &username, const QString &password,
                       const QString &acId, const QString &domain) {
    if (m_busy) {
        log("WARN", "上一次操作尚未完成，先取消");
        cancel();
    }

    m_username = username;
    m_password = password;
    m_acId = acId;
    m_domain = domain;
    m_ip.clear();
    m_token.clear();
    m_ipResolved = false;
    m_busy = true;
    m_cancelled = false;
    m_operation = OpLogin;

    armWatchdog(kLoginWatchdogMs);

    log("STEP", "========== 开始登录流程 ==========");
    log("INFO", QString("user=%1%2  ac_id=%3")
            .arg(maskIfNeeded(m_username), m_domain, m_acId));

    requestChallenge();
}

// ============================================================
// 公开入口：logout
// ============================================================

void SrunClient::logout(const QString &username, const QString &acId,
                        const QString &domain) {
    if (m_busy) {
        log("WARN", "上一次操作尚未完成，先取消");
        cancel();
    }

    m_username = username;
    m_acId = acId;
    m_domain = domain;
    m_busy = true;
    m_cancelled = false;
    m_operation = OpLogout;

    armWatchdog(kLogoutWatchdogMs);

    log("STEP", "========== 开始注销流程 ==========");
    log("INFO", QString("user=%1%2")
            .arg(maskIfNeeded(m_username), m_domain));

    requestLogout();
}

// ============================================================
// 公开入口：queryOnline
// ============================================================

void SrunClient::queryOnline(bool silent) {
    if (!silent) log("STEP", "→ 查询在线状态");

    const QString fullUrl = QString("%1/cgi-bin/rad_user_info").arg(baseUrl());
    if (!silent) log("HTTP", "GET " + maskIfNeeded(fullUrl));

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
            // HTTP 4xx/5xx 时 Qt 直接报网络错误，但门户的错误体里通常有 error_msg
            const QString serverMsg = SrunHttpUtils::extractServerError(data);
            const QString detail = serverMsg.isEmpty() ? netErrStr : serverMsg;
            log("ERROR", "查询在线 网络错误: " + detail);
            emit onlineQueryFailed("网络错误: " + detail);
        }
        return;
    }

    if (!silent) log("HTTP", "← 在线信息响应: " + previewForLog(data, 300));

    bool ok = false;
    const QJsonObject obj = SrunHttpUtils::parseJsonOrJsonp(data, &ok);
    if (!ok) {
        if (!silent) {
            log("ERROR", "在线信息响应解析失败");
            emit onlineQueryFailed("在线信息解析失败");
        }
        return;
    }

    updateIpFromOnlineInfo(obj);

    const QString err = obj.value("error").toString();

    if (err == "ok") {
        if (!silent) {
            log("INFO", QString("当前在线: user=%1, ip=%2")
                .arg(maskIfNeeded(obj.value("user_name").toString()),
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

// ============================================================
// 外网连通性检测
// ============================================================

void SrunClient::checkInternet() {
    log("STEP", "→ 检查外网连通性");

    const QString testUrl("http://firefox-portal-detection.com/generate_204");
    QNetworkRequest req{QUrl(testUrl)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::ManualRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader, SrunHttpUtils::getUAHeader());
    // 不再单独覆盖 transferTimeout，统一由 QNAM 控制
    QNetworkReply *reply = m_nam->get(req);

    log("HTTP", "GET " + testUrl);

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        onCheckInternetFinished(reply);
    });
}

void SrunClient::onCheckInternetFinished(QNetworkReply *reply) {
    reply->readAll();
    const auto netErr = reply->error();
    const QString netErrStr = reply->errorString();
    const int statusCode = reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    // 登录或注销还没结束时，探测结果不能改认证地址，也不能再触发一次登录。
    if (m_busy) {
        log("INFO", "认证流程进行中，忽略本次外网探测");
        return;
    }

    if (netErr != QNetworkReply::NoError) {
        emit internetUnreachable(netErrStr);
        return;
    }

    if (statusCode == 204) {
        emit internetReachable();
        return;
    }

    const QByteArray locationRaw = reply->rawHeader("Location");
    if (!locationRaw.isEmpty()) {
        const QUrl locUrl = QUrl::fromEncoded(locationRaw);
        if (locUrl.isValid() && !locUrl.host().isEmpty()) {
            QString newBase = locUrl.scheme() + "://" + locUrl.host();
            if (locUrl.port() != -1 && locUrl.port() != 80) {
                newBase += ":" + QString::number(locUrl.port());
            }

            if (newBase != baseUrl()) {
                if (m_baseUrlOverridden) {
                    // 用户在设置里显式覆盖了服务器地址，探测结果只作提示
                    log("INFO", "已手动设置服务器 URL，忽略探测到的地址：" + newBase);
                } else {
                    m_baseUrl = newBase;
                    log("INFO", "检测到 captive portal，服务器地址更新为：" + baseUrl());
                }
            }
            emit captivePortalDetected(baseUrl());
            return;
        }
    }

    const QString reason = QString("HTTP %1（可能被 captive portal 劫持）").arg(statusCode);
    emit internetUnreachable(reason);
}

// ============================================================
// 周期性轮询
// ============================================================

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
    if (m_periodicReply) {
        m_periodicReply->abort();
        m_periodicReply = nullptr;
    }
    m_periodicPollInFlight = false;
}

void SrunClient::applyPollingIntervalAndRestart() {
    // 无参数 overload，用于保留原interval并重启
    if (!m_pollTimer) {
        startPolling();
        return;
    }
    applyPollingIntervalAndRestart(m_pollTimer->interval());
}

void SrunClient::applyPollingIntervalAndRestart(int intervalMs) {
    // timer 还没创建时直接走 startPolling，否则这里静默什么都不做
    if (!m_pollTimer) {
        startPolling(intervalMs);
        return;
    }
    m_pollTimer->setInterval(intervalMs);
    m_pollTimer->start();
    m_periodicFailureCount = 0;  // 重置计数器
    doPeriodicPoll();  // 点击按钮后立即执行一次
}

void SrunClient::setLogSensitive(const bool logSensitive) {
    m_logSensitive = logSensitive;
}

// ============================================================
// 服务器地址
// ============================================================

void SrunClient::setBaseUrl(const QString &baseUrl) {
    const QString trimmed = baseUrl.trimmed();

    if (trimmed.isEmpty()) {
        m_baseUrl.clear();
        m_baseUrlOverridden = false;
        log("INFO", "服务器地址恢复默认：" + this->baseUrl());
        return;
    }

    const QUrl url(trimmed);
    if (!url.isValid() || url.host().isEmpty()
        || (url.scheme() != QLatin1String("http")
            && url.scheme() != QLatin1String("https"))) {
        // 配置可能被手动改坏；保留当前可用地址，不让整体认证失效
        log("WARN", "忽略无效的服务器地址：" + trimmed);
        return;
    }

    QString normalized = trimmed;
    while (normalized.endsWith(QLatin1Char('/'))) {
        normalized.chop(1);
    }

    m_baseUrl = normalized;
    m_baseUrlOverridden = true;
    log("INFO", "服务器地址设置为：" + this->baseUrl());
}

QString SrunClient::baseUrl() const {
    return m_baseUrl.isEmpty() ? kDefaultBaseUrl : m_baseUrl;
}

bool SrunClient::isBaseUrlOverridden() const {
    return m_baseUrlOverridden;
}

void SrunClient::doPeriodicPoll() {
    if (m_busy) return;

    // 关键：单飞保护，避免请求在无响应时堆积
    if (m_periodicPollInFlight) {
        log("WARN", "上一次周期轮询尚未结束，跳过本次");
        return;
    }
    m_periodicPollInFlight = true;

    const QString fullUrl = QString("%1/cgi-bin/rad_user_info").arg(baseUrl());
    QNetworkRequest req{QUrl::fromEncoded(fullUrl.toUtf8())};
    applyCommonHeaders(req);
    QNetworkReply *reply = m_nam->get(req);
    m_periodicReply = reply;

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        m_periodicPollInFlight = false;
        if (m_periodicReply == reply) m_periodicReply = nullptr;
        onPeriodicPollFinished(reply);
    });
}

void SrunClient::onPeriodicPollFinished(QNetworkReply *reply) {
    const QByteArray data = reply->readAll();
    const auto netErr = reply->error();
    const QString netErrStr = reply->errorString();
    reply->deleteLater();

    if (m_busy) return;

    const auto noteFailure = [this](const QString &reason) {
        ++m_periodicFailureCount;
        log("WARN", QString("在线轮询失败(%1/%2): %3")
                .arg(m_periodicFailureCount)
                .arg(kPeriodicFailureThreshold)
                .arg(reason));
        // 单次 Wi-Fi 抖动不要立刻把界面判成离线；连续失败才认为掉线
        if (m_periodicFailureCount >= kPeriodicFailureThreshold)
            emit onlineStatusRefreshed(false, {});
    };

    if (netErr != QNetworkReply::NoError) {
        // 网络层错误一般意味着本机确实断网，直接按离线处理；
        // 服务器异常/解析失败可能是门户抖动，交给 noteFailure 连续计数。
        log("WARN", QString("在线轮询失败: %1").arg(netErrStr));
        emit onlineStatusRefreshed(false, {});
        return;
    }

    bool ok = false;
    const QJsonObject obj = SrunHttpUtils::parseJsonOrJsonp(data, &ok);
    if (!ok) {
        noteFailure("响应解析失败");
        return;
    }

    const QString err = obj.value("error").toString();
    if (err == "ok") {
        m_periodicFailureCount = 0;
        updateIpFromOnlineInfo(obj);
        emit onlineStatusRefreshed(true, obj);
    } else if (err == "not_online_error") {
        m_periodicFailureCount = 0;
        emit onlineStatusRefreshed(false, {});
    } else {
        noteFailure("服务器返回：" + (err.isEmpty() ? QStringLiteral("未知错误") : err));
    }
}

// ============================================================
// 登录流程：步骤 1 - challenge
// ============================================================

void SrunClient::requestChallenge() {
    if (m_operation == OpNone) return;

    setStage("获取令牌");
    log("STEP", "→ 请求 challenge");

    const QList<QPair<QString, QString>> params = {
        {"username", m_username + m_domain},
        {"ip", m_ip},
    };

    const QString fullUrl = QString("%1/cgi-bin/get_challenge?%2")
            .arg(baseUrl(), SrunHttpUtils::buildQuery(params));

    log("HTTP", "GET " + maskIfNeeded(fullUrl));

    QNetworkRequest req{QUrl::fromEncoded(fullUrl.toUtf8())};
    applyCommonHeaders(req);
    QNetworkReply *reply = m_nam->get(req);
    attachOpReply(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        onChallengeFinished(reply);
    });
}

void SrunClient::onChallengeFinished(QNetworkReply *reply) {
    const QByteArray data = reply->readAll();
    const auto netErr = reply->error();
    const QString netErrStr = reply->errorString();
    reply->deleteLater();

    if (m_cancelled) return;

    if (netErr != QNetworkReply::NoError) {
        const QString serverMsg = SrunHttpUtils::extractServerError(data);
        const QString detail = serverMsg.isEmpty() ? netErrStr : serverMsg;
        log("ERROR", "challenge 网络错误: " + detail);
        emitLoginFailed("网络错误: " + detail, {});
        return;
    }

    log("HTTP", "← challenge 响应: " + previewForLog(data));

    bool ok = false;
    const QJsonObject obj = SrunHttpUtils::parseJsonOrJsonp(data, &ok);
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

    // 服务器视角的 IP 和本地不一致时，用服务器为准，重新请求一次 challenge。
    // 有的部署把 IP 放在 online_ip 里，两个字段都尝试。
    QString clientIp = obj.value("client_ip").toString();
    if (clientIp.isEmpty()) clientIp = obj.value("online_ip").toString();

    if (!m_ipResolved) {
        if (clientIp.isEmpty()) {
            log("ERROR", "challenge 响应里没有服务器分配的 IP");
            emitLoginFailed("未获取到服务器分配的 IP，请检查网络后重试", obj);
            return;
        }
        if (clientIp != m_ip) {
            log("INFO", QString("本地 IP=%1，服务器看到 IP=%2，以服务器为准并重新获取 token")
                    .arg(m_ip.isEmpty() ? "(空)" : m_ip, clientIp));
            m_ip = clientIp;
            m_ipResolved = true;
            requestChallenge();
            return;
        }
    }

    m_token = token;
    log("INFO", "token = " + maskIfNeeded(m_token));

    requestAuth();
}

// ============================================================
// 登录流程：步骤 2 - auth
// ============================================================

void SrunClient::requestAuth() {
    if (m_operation == OpNone) return;

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

    log("INFO", "hmd5 = " + maskIfNeeded(hmd5));
    log("INFO", "chksum = " + maskIfNeeded(chksum));

    const QList<QPair<QString, QString>> params = {
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
            .arg(baseUrl(), SrunHttpUtils::buildQuery(params));

    log("HTTP", "GET " + maskIfNeeded(fullUrl));

    QNetworkRequest req{QUrl::fromEncoded(fullUrl.toUtf8())};
    applyCommonHeaders(req);
    QNetworkReply *reply = m_nam->get(req);
    attachOpReply(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        onAuthFinished(reply);
    });
}

void SrunClient::onAuthFinished(QNetworkReply *reply) {
    const QByteArray data = reply->readAll();
    const auto netErr = reply->error();
    const QString netErrStr = reply->errorString();
    reply->deleteLater();

    if (m_cancelled) return;

    if (netErr != QNetworkReply::NoError) {
        const QString serverMsg = SrunHttpUtils::extractServerError(data);
        const QString detail = serverMsg.isEmpty() ? netErrStr : serverMsg;
        log("ERROR", "auth 网络错误: " + detail);
        emitLoginFailed("网络错误: " + detail, {});
        return;
    }

    log("HTTP", "← auth 响应: " + previewForLog(data, 300));

    bool ok = false;
    const QJsonObject obj = SrunHttpUtils::parseJsonOrJsonp(data, &ok);
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
        if (errMsg.isEmpty()) errMsg = obj.value("ploy_msg").toString();
        if (errMsg.isEmpty()) errMsg = err;
        log("ERROR", "认证失败: " + errMsg);
        emitLoginFailed(errMsg, obj);
        return;
    }

    log("INFO", "认证接口返回成功，开始轮询在线状态");
    setStage("确认在线状态");

    pollOnlineStatus(3, "ok", [this](bool success, const QJsonObject &obj) {
        if (m_cancelled) return;
        if (success) {
            log("INFO", QString("登录成功: user=%1, ip=%2, 产品=%3, 已用流量=%4 bytes")
                    .arg(maskIfNeeded(obj.value("user_name").toString()),
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
    if (m_operation == OpNone) return;

    setStage("注销中");
    log("STEP", "→ 请求注销");

    const QList<QPair<QString, QString>> params = {
        {"action", "logout"},
        {"username", m_username + m_domain},
    };

    const QString fullUrl = QString("%1/cgi-bin/srun_portal?%2")
            .arg(baseUrl(), SrunHttpUtils::buildQuery(params));

    log("HTTP", "GET " + maskIfNeeded(fullUrl));

    QNetworkRequest req{QUrl::fromEncoded(fullUrl.toUtf8())};
    applyCommonHeaders(req);
    QNetworkReply *reply = m_nam->get(req);
    attachOpReply(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        onLogoutFinished(reply);
    });
}

void SrunClient::onLogoutFinished(QNetworkReply *reply) {
    const QByteArray data = reply->readAll();
    const auto netErr = reply->error();
    const QString netErrStr = reply->errorString();
    reply->deleteLater();

    if (m_cancelled) return;

    if (netErr != QNetworkReply::NoError) {
        const QString serverMsg = SrunHttpUtils::extractServerError(data);
        const QString detail = serverMsg.isEmpty() ? netErrStr : serverMsg;
        log("ERROR", "logout 网络错误: " + detail);
        emitLogoutFailed("网络错误: " + detail, {});
        return;
    }

    log("HTTP", "← logout 响应: " + previewForLog(data, 300));

    bool ok = false;
    const QJsonObject obj = SrunHttpUtils::parseJsonOrJsonp(data, &ok);
    if (!ok) {
        log("ERROR", "logout 响应解析失败");
        emitLogoutFailed("logout 响应解析失败", {});
        return;
    }

    const QString err = obj.value("error").toString();
    if (err != "ok") {
        QString errMsg = obj.value("error_msg").toString();
        if (errMsg.isEmpty()) errMsg = obj.value("ploy_msg").toString();
        if (errMsg.isEmpty()) errMsg = err;
        log("ERROR", "注销请求被拒绝: " + errMsg);
        emitLogoutFailed(errMsg, obj);
        return;
    }

    log("INFO", "注销请求已接受，开始轮询在线状态");
    setStage("确认下线状态");

    pollOnlineStatus(3, "not_online_error", [this](bool success, const QJsonObject &obj) {
        if (m_cancelled) return;
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

    QTimer::singleShot(kVerifyPollIntervalMs, this, [this] { doPollStep(); });
}

void SrunClient::doPollStep() {
    if (m_operation == OpNone) return;   // 已被 cancel / 看门狗中止

    m_pollCheckCount++;
    log("STEP", QString("→ 第 %1/%2 次查询在线状态")
            .arg(m_pollCheckCount).arg(m_pollMaxChecks));

    const QString fullUrl = QString("%1/cgi-bin/rad_user_info").arg(baseUrl());
    log("HTTP", "GET " + maskIfNeeded(fullUrl));

    QNetworkRequest req{QUrl::fromEncoded(fullUrl.toUtf8())};
    applyCommonHeaders(req);
    QNetworkReply *reply = m_nam->get(req);
    attachOpReply(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        onPollStepFinished(reply);
    });
}

void SrunClient::onPollStepFinished(QNetworkReply *reply) {
    const QByteArray data = reply->readAll();
    const auto netErr = reply->error();
    const QString netErrStr = reply->errorString();
    reply->deleteLater();

    if (m_cancelled || m_operation == OpNone) return;

    if (netErr != QNetworkReply::NoError) {
        log("WARN", "轮询网络错误: " + netErrStr);
        if (m_pollCheckCount < m_pollMaxChecks) {
            QTimer::singleShot(kVerifyPollIntervalMs, this, [this] { doPollStep(); });
        } else {
            finishPoll(false, {});
        }
        return;
    }

    log("HTTP", "← 在线信息响应: " + previewForLog(data, 200));

    bool ok = false;
    const QJsonObject obj = SrunHttpUtils::parseJsonOrJsonp(data, &ok);
    if (ok) {
        const QString err = obj.value("error").toString();
        if (err == m_pollSuccessError) {
            updateIpFromOnlineInfo(obj);
            finishPoll(true, obj);
            return;
        }
    }

    if (m_pollCheckCount < m_pollMaxChecks) {
        QTimer::singleShot(kVerifyPollIntervalMs, this, [this] { doPollStep(); });
    } else {
        finishPoll(false, obj);
    }
}
