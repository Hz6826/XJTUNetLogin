#ifndef SRUNCLIENT_H
#define SRUNCLIENT_H

#include <QObject>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QString>
#include <QPointer>
#include <QList>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class SrunClient : public QObject {
    Q_OBJECT

public:
    explicit SrunClient(QObject *parent = nullptr);

    // ---- 对外调用（全部异步，立即返回）----
    void login(const QString &username,
               const QString &password,
               const QString &acId,
               const QString &domain);

    void logout(const QString &username,
                const QString &acId,
                const QString &domain);

    void queryOnline(bool silent = false);
    void checkInternet();

    void startPolling(int intervalMs = 30000);
    void stopPolling();

    void applyPollingIntervalAndRestart();

    void applyPollingIntervalAndRestart(int intervalMs);

    void setLogSensitive(bool logSensitive);

    void setBaseUrl(const QString &baseUrl);

    QString baseUrl() const;

    bool isBaseUrlOverridden() const;

    inline static const QString kDefaultBaseUrl = "http://10.6.21.2";

public slots:
    // 取消正在进行的 login/logout（含其内部 challenge/auth/verify 轮询），
    // 以及 abort 在飞的周期轮询请求。不改变账号/IP 上下文。
    void cancel();

    // 复位：cancel() + 停轮询 + 清 token/IP + 回空闲态。
    void reset();

signals:
    // level: "STEP" / "INFO" / "HTTP" / "WARN" / "ERROR" / "SUCCESS"
    void logMessage(const QString &level, const QString &message);
    void stageChanged(const QString &stage);

    void loginSucceeded(const QJsonObject &onlineInfo);
    void loginFailed(const QString &errorMessage, const QJsonObject &rawResponse);

    void logoutSucceeded(const QJsonObject &rawResponse);
    void logoutFailed(const QString &errorMessage, const QJsonObject &rawResponse);

    void onlineInfoReceived(const QJsonObject &onlineInfo);
    void onlineQueryFailed(const QString &errorMessage);

    void internetReachable();
    void internetUnreachable(const QString &reason);

    void onlineStatusRefreshed(bool online, const QJsonObject &info);
    void captivePortalDetected(const QString &newBaseUrl);

    // 由 cancel() / reset() 触发（login/logout 被主动取消时发出）
    void operationCancelled();

private:
    // ---------- 操作状态 ----------
    enum Operation { OpNone, OpLogin, OpLogout };

    // ---------- 登录/登出流程 ----------
    void requestChallenge();
    void onChallengeFinished(QNetworkReply *reply);
    void requestAuth();
    void onAuthFinished(QNetworkReply *reply);
    void requestLogout();
    void onLogoutFinished(QNetworkReply *reply);

    // ---------- 查询/轮询 ----------
    using PollCallback = std::function<void(bool success, const QJsonObject &obj)>;
    void pollOnlineStatus(int maxChecks, const QString &successError, PollCallback callback);
    void doPollStep();
    void onPollStepFinished(QNetworkReply *reply);

    void onQueryOnlineFinished(QNetworkReply *reply, bool silent);
    void onCheckInternetFinished(QNetworkReply *reply);
    void doPeriodicPoll();
    void onPeriodicPollFinished(QNetworkReply *reply);

    // ---------- 看门狗 / 取消 ----------
    void armWatchdog(int timeoutMs);
    void disarmWatchdog() const;
    void onWatchdogTimeout();

    void attachOpReply(QNetworkReply *reply);
    void abortOpReplies();

    // ---------- 辅助 ----------
    void log(const QString &level, const QString &msg);
    void setStage(const QString &stage);
    void applyCommonHeaders(QNetworkRequest &req) const; // 封装 utils

    void emitLoginSucceeded(const QJsonObject &info);
    void emitLoginFailed(const QString &msg, const QJsonObject &raw);
    void emitLogoutSucceeded(const QJsonObject &info);
    void emitLogoutFailed(const QString &msg, const QJsonObject &raw);

    // 取走当前 poll 回调并清空，避免回调内重入时旧闭包被覆盖
    void finishPoll(bool success, const QJsonObject &obj);
    // 在线信息里回写本机 IP（仅当本地还没有时）
    void updateIpFromOnlineInfo(const QJsonObject &obj);

    QString maskIfNeeded(const QString &text) const;

    // ---------- 状态 ----------
    QNetworkAccessManager *m_nam = nullptr;

    // 当前操作的上下文（异步需要把参数存成成员）
    QString m_username;
    QString m_password;
    QString m_ip;
    QString m_acId;
    QString m_domain;
    QString m_token;
    bool m_ipResolved = false;

    int m_pollCheckCount = 0;
    int m_pollMaxChecks = 3;
    QString m_pollSuccessError;
    PollCallback m_pollCallback;

    QTimer *m_pollTimer = nullptr;
    bool m_busy = false;

    // 当前正在进行的 login/logout 操作（OpNone 表示空闲）
    Operation m_operation = OpNone;

    // 整体看门狗（覆盖 login / logout 全流程）
    QTimer *m_watchdog = nullptr;

    // 取消窗口：为 true 时业务回调直接丢弃，避免误发 success/failed
    bool m_cancelled = false;

    // 追踪 login/logout 流程内所有在飞请求，cancel/watchdog 统一 abort
    QList<QPointer<QNetworkReply>> m_opReplies;

    // 周期轮询单飞保护
    bool m_periodicPollInFlight = false;
    QPointer<QNetworkReply> m_periodicReply;
    // 连续失败计数：网络抖动/解析失败时不立刻把 UI 判成离线
    int m_periodicFailureCount = 0;

    bool m_logSensitive = false;

    // ---------- 常量 ----------
    static constexpr int kTransferTimeoutMs = 12000;   // QNAM 统一超时（8~15s）
    static constexpr int kLoginWatchdogMs    = 60000;  // 登录整体看门狗
    static constexpr int kLogoutWatchdogMs   = 30000;  // 注销整体看门狗
    static constexpr int kVerifyPollIntervalMs = 1000; // 登录/注销后确认在线间隔
    static constexpr int kPeriodicFailureThreshold = 3; // 连续失败几次才判离线

    QString m_baseUrl;
    bool m_baseUrlOverridden = false;   // 用户显式覆盖时，302 探测不得改写
};

#endif // SRUNCLIENT_H