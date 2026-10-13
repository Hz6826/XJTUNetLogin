#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "config/Config.h"

class SrunClient;
class QLineEdit;
class QCheckBox;
class QPushButton;
class QLabel;
class QPlainTextEdit;
class QCloseEvent;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(SrunClient *client, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

    void logWelcome();

private slots:
    void onToggleLogClicked();

    void onLoginClicked();
    void onLogoutClicked();
    void onSaveConfigClicked();
    void onClearLogClicked();

    void onStageChanged(const QString &stage);
    void onLogMessage(const QString &level, const QString &msg);
    void onLoginSucceeded(const QJsonObject &info);
    void onLoginFailed(const QString &msg, const QJsonObject &raw);
    void onLogoutSucceeded(const QJsonObject &info);
    void onLogoutFailed(const QString &msg, const QJsonObject &raw);
    void onInternetReachable();
    void onInternetUnreachable(const QString &reason);

private:
    void buildUi();
    void loadConfigToUi();
    void connectSignals();

    void applyLogExpanded(bool expanded);
    void appendLog(const QString &level, const QString &msg);

    bool saveConfigSilently();

    // 成员
    SrunClient *m_client = nullptr;
    Config m_config;

    // UI 元素
    QLineEdit *m_usernameEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QLineEdit *m_acIdEdit = nullptr;
    QLineEdit *m_domainEdit = nullptr;

    QCheckBox *m_autoStartCheck = nullptr;
    QCheckBox *m_autoLoginCheck = nullptr;
    QLineEdit *m_ssidEdit = nullptr;

    QPushButton *m_loginBtn = nullptr;
    QPushButton *m_logoutBtn = nullptr;
    QPushButton *m_saveBtn = nullptr;

    QLabel *m_statusLabel = nullptr;
    QPushButton *m_clearLogBtn = nullptr;
    QPushButton *m_toggleLogBtn = nullptr;
    QPlainTextEdit *m_logTextEdit = nullptr;

    // 固定窗口尺寸
    static constexpr int kWindowWidth           = 380;
    static constexpr int kWindowHeightExpanded  = 520;
    static constexpr int kWindowHeightCollapsed = 300;
};

#endif // MAINWINDOW_H