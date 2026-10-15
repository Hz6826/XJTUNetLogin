#ifndef SRUNCLIENT_H
#define SRUNCLIENT_H

#include <QObject>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class SrunClient : public QObject {
    Q_OBJECT

public:
    explicit SrunClient(QObject *parent = nullptr);

    // 对外调用（全部异步，立即返回）
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

signals:
    // ---- 通用日志（未来 textarea 直接连这个）----
    // level: "STEP" / "INFO" / "HTTP" / "WARN" / "ERROR"
    void logMessage(const QString &level, const QString &message);

    // ---- 阶段变化（状态栏/标题栏用）----
    void stageChanged(const QString &stage);

    // ---- 业务信号 ----
    void loginSucceeded(const QJsonObject &onlineInfo);
    void loginFailed(const QString &errorMessage,
                     const QJsonObject &rawResponse);

    void logoutSucceeded(const QJsonObject &rawResponse);
    void logoutFailed(const QString &errorMessage,
                      const QJsonObject &rawResponse);

    void onlineInfoReceived(const QJsonObject &onlineInfo);
    void onlineQueryFailed(const QString &errorMessage);

    void internetReachable();
    void internetUnreachable(const QString &reason);

    void onlineStatusRefreshed(bool online, const QJsonObject &info);

private:
    // ---------- 内部异步步骤 ----------
    void requestChallenge();
    void onChallengeFinished(QNetworkReply *reply);

    void requestAuth();
    void onAuthFinished(QNetworkReply *reply);

    void requestLogout();
    void onLogoutFinished(QNetworkReply *reply);

    using PollCallback = std::function<void(bool success, const QJsonObject &obj)>;

    void pollOnlineStatus(int maxChecks, const QString &successError, PollCallback callback);
    void doPollStep();
    void onPollStepFinished(QNetworkReply *reply);

    void onQueryOnlineFinished(QNetworkReply *reply, bool silent);

    void onCheckInternetFinished(QNetworkReply *reply);

    void onPeriodicPollFinished(QNetworkReply *reply);

    // ---------- 辅助 ----------
    void log(const QString &level, const QString &msg);
    void setStage(const QString &stage);

    static QJsonObject parseJsonOrJsonp(const QByteArray &data, bool *ok);
    static void applyCommonHeaders(QNetworkRequest &req);
    static QString getUAHeader();
    static QString previewBody(const QByteArray &data, int maxLen = 200);
    static QString buildQuery(const QList<QPair<QString, QString>> &params);

    void emitLoginSucceeded(const QJsonObject &info);
    void emitLoginFailed(const QString &msg, const QJsonObject &raw);
    void emitLogoutSucceeded(const QJsonObject &info);
    void emitLogoutFailed(const QString &msg, const QJsonObject &raw);

    void doPeriodicPoll();

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

    static constexpr int kPollIntervalMs = 1000;
    static constexpr auto kBaseUrl = "http://10.6.21.2";
};

#endif // SRUNCLIENT_H
