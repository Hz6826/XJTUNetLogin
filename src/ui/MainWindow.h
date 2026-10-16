#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QFile>
#include <QMainWindow>
#include <QLabel>

#include "config/Config.h"

class SrunClient;
class QLineEdit;
class QCheckBox;
class QPushButton;
class QToolButton;
class QLabel;
class QPlainTextEdit;
class QCloseEvent;
class QVBoxLayout;
class QTimer;
class QTabWidget;

enum class OnlineState : int { Unknown, Offline, Online, Busy };

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(SrunClient *client, QWidget *parent = nullptr);
    bool eventFilter(QObject *obj, QEvent *event) override;
    void setForceQuit(bool force);
    const Config &config() const;

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;

public slots:
    void onAuthClicked();

private slots:
    void onToggleLogClicked();
    void onPingClicked();
    void onSaveConfigClicked();
    void onClearLogClicked();
    void onOpenLogDirClicked();

    void onStageChanged(const QString &stage);
    void onLogMessage(const QString &level, const QString &msg);
    void onLoginSucceeded(const QJsonObject &info);
    void onLoginFailed(const QString &msg, const QJsonObject &raw);
    void onLogoutSucceeded(const QJsonObject &info);
    void onLogoutFailed(const QString &msg, const QJsonObject &raw);
    void onInternetReachable();
    void onInternetUnreachable(const QString &reason);
    void onOnlineStatusRefreshed(bool online, const QJsonObject &info);
    void onAutoLoginToggled(bool on);
    void onAutoStartToggled(bool on);
    void onCaptivePortalDetected(const QString &newBaseUrl);
    void onPollTimerApplyBtnClicked();

private:
    // ---------- UI 构建（MainWindowUi.cpp） ----------
    void loadIcons();
    void buildUi();
    void buildUiAuthForm(QVBoxLayout *mainLayout);
    void buildUiAutoLogin(QVBoxLayout *uiSettingsLayout);
    void buildUiPollingSetting(QVBoxLayout *uiSettingsLayout);
    void buildUiOpBtnRow(QVBoxLayout *mainLayout);
    void buildUiInfoArea(QVBoxLayout *mainLayout);
    void buildUiLogPanel(QWidget *parent);
    void buildUiInfoPanel(QWidget *parent);
    void buildUiSettingsPanel(QWidget *parent);
    void buildUiAboutPanel(QWidget *parent);

    static QPixmap renderSvg(const QString &path, int w, int h, qreal dpr);

    // ---------- 配置/窗口几何（MainWindowConfig.cpp） ----------
    void loadConfigToUi();
    void syncAccountFieldsToConfig();
    bool saveConfigSilently();
    void saveWindowGeometryToConfig();
    void applyLogExpanded(bool expanded);
    void resizeForLogState(bool expanded);
    bool savePollTimerInterval(int interval);

    // ---------- 日志（MainWindowLog.cpp） ----------
    void appendLog(const QString &level, const QString &msg);
    void logWelcome();
    void writeLogToFile(const QString &level, const QString &msg);
    void ensureLogFileForToday();
    static QString logDirPath();

    // ---------- 认证/自动登录（MainWindowAuth.cpp） ----------
    void connectSignals();
    void triggerAutoLogin();
    void setOnlineState(OnlineState s);
    void updateUiForState();

    // ---------- 在线信息面板（MainWindowInfo.cpp） ----------
    void applyOnlineInfo(const QJsonObject &info);
    void updateOnlineInfoPanel(const QJsonObject &info);
    void clearOnlineInfoPanel();
    static QString formatBytes(qint64 bytes);
    static QString formatDuration(qint64 seconds);

    void checkVersion();

    // 成员
    SrunClient *m_client = nullptr;
    Config m_config;
    QTimer *m_postLoginTimer = nullptr;
    QFile m_logFile;
    QDate m_logFileDate;
    bool m_lastDarkTheme = true;
    OnlineState m_onlineState = OnlineState::Unknown;
    bool m_forceQuit = false;
    bool m_pendingAutoLogin = false;
    int m_expandedWindowHeight = -1;
    bool m_firstShowDone = false;

    // 图标
    QIcon m_iconChevronDown;
    QIcon m_iconChevronUp;
    QIcon m_iconTrash;
    QIcon m_iconFolder;

    // UI 元素
    QLineEdit *m_usernameEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QLineEdit *m_acIdEdit = nullptr;
    QLineEdit *m_domainEdit = nullptr;

    QCheckBox *m_autoStartCheck = nullptr;
    QCheckBox *m_autoLoginCheck = nullptr;

    QPushButton *m_authBtn = nullptr;
    QPushButton *m_pingBtn = nullptr;
    QPushButton *m_saveBtn = nullptr;

    QLabel *m_statusLabel = nullptr;
    QCheckBox *m_filterHttpCheck = nullptr;
    QToolButton *m_clearLogBtn = nullptr;
    QToolButton *m_toggleLogBtn = nullptr;
    QPlainTextEdit *m_logTextEdit = nullptr;

    QTabWidget *m_detailTab = nullptr;

    QToolButton *m_openLogDirBtn = nullptr;

    QLabel *m_infoAccountLabel     = nullptr;
    QLabel *m_infoIpLabel          = nullptr;
    QLabel *m_infoMacLabel         = nullptr;
    QLabel *m_infoProductLabel     = nullptr;
    QLabel *m_infoBillingLabel     = nullptr;
    QLabel *m_infoUsedFlowLabel    = nullptr;
    QLabel *m_infoUsedTimeLabel    = nullptr;
    QLabel *m_infoBalanceLabel     = nullptr;
    QLabel *m_infoDeviceTotalLabel = nullptr;

    QLineEdit *m_pollTimerIntervalEdit = nullptr;
    QPushButton *m_pollTimerApplyBtn = nullptr;

    QLabel *m_versionCheckLabel = nullptr;

    static constexpr int kAuthFormHeight     = 120;
    static constexpr int kOpBtnRowHeight     = 45;
    static constexpr int kStatusRowHeight    = 30;
    static constexpr int kDetailTabMinHeight = 260;
    static constexpr int kWindowMinWidth     = 360;
    static constexpr int kWindowMinHeight    =
        kAuthFormHeight + kOpBtnRowHeight + kStatusRowHeight + kDetailTabMinHeight;
    static constexpr int kWindowMaxWidth     = 500;
    static constexpr int kWindowMaxHeight    = kWindowMinHeight + 200;

signals:
    void onlineStateChanged(OnlineState state);
};

#endif // MAINWINDOW_H