#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QFile>
#include <QMainWindow>

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

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;

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

private:
    void loadIcons();

    void buildUiAuthForm(QVBoxLayout *mainLayout);
    void buildUiAutoLogin(QVBoxLayout *mainLayout);
    void buildUiOpBtnRow(QVBoxLayout *mainLayout);
    void buildUiInfoArea(QVBoxLayout *mainLayout);

    void buildUiLogPanel(QWidget *parent);
    void buildUiInfoPanel(QWidget *parent);

    void buildUi();

    void loadConfigToUi();
    void connectSignals();

    void applyLogExpanded(bool expanded);
    void appendLog(const QString &level, const QString &msg);

    bool saveConfigSilently();

    void applyOnlineInfo(const QJsonObject &info);
    void setOnlineState(OnlineState s);
    void updateUiForState();

    void updateOnlineInfoPanel(const QJsonObject &info);
    void clearOnlineInfoPanel();
    static QString formatBytes(qint64 bytes);
    static QString formatDuration(qint64 seconds);

    void logWelcome();

    void writeLogToFile(const QString &level, const QString &msg);
    void ensureLogFileForToday();
    QString logDirPath() const;

    // 成员
    SrunClient *m_client = nullptr;
    Config m_config;
    QTimer *m_postLoginTimer = nullptr;
    QFile m_logFile;
    QDate m_logFileDate;
    bool m_lastDarkTheme = true;
    OnlineState m_onlineState = OnlineState::Unknown;
    bool m_forceQuit = false;

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
    QLineEdit *m_ssidEdit = nullptr;

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

    // 在线信息面板的字段 Label（每个字段一个 QLabel）
    QLabel *m_infoAccountLabel    = nullptr;
    QLabel *m_infoIpLabel         = nullptr;
    QLabel *m_infoMacLabel        = nullptr;
    QLabel *m_infoProductLabel    = nullptr;
    QLabel *m_infoBillingLabel    = nullptr;
    QLabel *m_infoUsedFlowLabel   = nullptr;
    QLabel *m_infoUsedTimeLabel   = nullptr;
    QLabel *m_infoBalanceLabel    = nullptr;
    QLabel *m_infoDeviceTotalLabel= nullptr;

    // 固定窗口尺寸
    static constexpr int kWindowWidth           = 380;
    static constexpr int kWindowHeightExpanded  = 520;
    static constexpr int kWindowHeightCollapsed = 272;

signals:
    void onlineStateChanged(OnlineState state);
};

#endif // MAINWINDOW_H