//
// Created by Hz6826 on 2026/10/12.
//

#ifndef CONFIG_H
#define CONFIG_H

#include <QJsonObject>
#include <QString>

struct Config {
    // ---------- auth 分组 ----------
    QString username;
    QString password;
    QString acId   = "1";
    QString domain = "@stu";

    // ---------- auto 分组 ----------
    bool autoStart = false;
    bool autoLogin = true;

    // ----------  ui 分组 ----------
    bool logExpanded = true;
    bool filterHttpLog = true;
    int currentTab = 0;

    static QString appDataDir();

    // ---------- 静态入口 ----------
    // 从磁盘加载；文件不存在或损坏时返回默认值
    static Config load();

    // 保存到磁盘；成功返回 true
    [[nodiscard]] bool save() const;

    // 配置文件路径（调试用）
    static QString filePath();

private:
    // JSON 序列化辅助
    static Config fromJson(const QJsonObject &obj);
    [[nodiscard]] QJsonObject toJson() const;
};

#endif // CONFIG_H
