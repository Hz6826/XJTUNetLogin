//
// Created by Hz6826 on 2026/10/12.
//

#ifndef CONFIG_H
#define CONFIG_H

#include <QJsonObject>
#include <QString>

enum class CloseAction : int {
    Ask            = 0,
    MinimizeToTray = 1,
    Quit           = 2,
};

struct Config {
    // ---------- auth 分组 ----------
    QString username;
    QString password;
    QString acId   = "1";
    QString domain = "@stu";

    // ---------- auto 分组 ----------
    bool autoStart = false;
    bool autoLogin = true;
    int pollTimerIntervalMs = 30000;

    // ----------  ui 分组 ----------
    bool logExpanded = true;
    bool filterHttpLog = true;
    int currentTab = 0;
    // 关闭行为
    CloseAction closeAction = CloseAction::Ask;
    QByteArray windowGeometry;
    int expandedWindowHeight = -1;

    // ---------- debug 分组 ---------
    // 不在UI显示，需手动配置json以防误配置
    bool logSensitive = false;   // 是否在日志中包含敏感信息（token 等）

    static QString appDataDir();

    // ---------- 静态入口 ----------
    // 从磁盘加载；文件不存在或损坏时返回默认值
    static Config load();

    // 保存到磁盘；成功返回 true
    [[nodiscard]] bool save() const;

    static QString filePath();

    static constexpr int kMinPollInterval = 10000;
    static constexpr int kMaxPollInterval = 600000;

private:
    // JSON 序列化辅助
    static Config fromJson(const QJsonObject &obj);
    [[nodiscard]] QJsonObject toJson() const;
};

#endif // CONFIG_H
