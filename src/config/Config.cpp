//
// Created by Hz6826 on 2026/10/12.
//

#include "Config.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>

namespace {

    // exe 同目录的 config.json —— 便携模式的标记文件
    QString portableConfigPath() {
        return QCoreApplication::applicationDirPath() + "/config.json";
    }

    bool isPortableMode() {
        return QFile::exists(portableConfigPath());
    }

} // namespace

QString Config::appDataDir() {
    if (isPortableMode()) {
        return QCoreApplication::applicationDirPath();
    }
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString Config::filePath() {
    if (isPortableMode()) {
        return portableConfigPath();
    }
    const QString dir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(dir)) {
        qWarning() << "无法创建目录:" << dir;
    }
    return dir + "/config.json";
}

Config Config::load() {
    const QString path = filePath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QByteArray data = file.readAll();
    file.close();

    // 跳过 UTF-8 BOM（PowerShell 5.1 的 Out-File 会加）
    if (data.startsWith("\xEF\xBB\xBF")) {
        data.remove(0, 3);
    }

    // 空文件（便携版首次运行）→ 直接用默认配置
    if (data.trimmed().isEmpty()) {
        return {};
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "config.json 解析失败:" << err.errorString()
                   << "@ offset" << err.offset;
        return {};
    }

    if (!doc.isObject()) {
        qWarning() << "config.json 根节点不是 JSON 对象";
        return {};
    }

    const QJsonObject root = doc.object();
    return fromJson(root);
}

Config Config::fromJson(const QJsonObject &root) {
    Config c;

    const QJsonObject authCfg = root.value("auth").toObject();

    c.username = authCfg.value("username").toString();
    c.password = authCfg.value("password").toString();
    c.acId = authCfg.value("acId").toString("1");
    c.domain = authCfg.value("domain").toString("@stu");

    const QJsonObject autoCfg = root.value("auto").toObject();

    c.autoStart = autoCfg.value("autoStart").toBool(false);
    c.autoLogin = autoCfg.value("autoLogin").toBool(true);
    int rawInterval = autoCfg.value("pollTimerIntervalMs").toInt(30000);
    c.pollTimerIntervalMs = std::clamp(rawInterval, kMinPollInterval, kMaxPollInterval);

    const QJsonObject uiCfg = root.value("ui").toObject();

    c.logExpanded = uiCfg.value("logExpanded").toBool(true);
    c.filterHttpLog = uiCfg.value("filterHttpLog").toBool(true);
    c.currentTab = uiCfg.value("currentTab").toInt(0);
    int closeActionRaw = uiCfg.value("closeAction").toInt(0);
    c.closeAction = static_cast<CloseAction>(closeActionRaw);
    c.windowGeometry = QByteArray::fromBase64(
        uiCfg.value("windowGeometry").toString().toLatin1());
    c.expandedWindowHeight = uiCfg.value("expandedWindowHeight").toInt(-1);

    return c;
}

QJsonObject Config::toJson() const {
    QJsonObject authCfg;
    authCfg["username"] = username;
    authCfg["password"] = password;
    authCfg["acId"] = acId;
    authCfg["domain"] = domain;

    QJsonObject autoCfg;
    autoCfg["autoStart"] = autoStart;
    autoCfg["autoLogin"] = autoLogin;
    autoCfg["pollTimerIntervalMs"] = pollTimerIntervalMs;

    QJsonObject uiCfg;
    uiCfg["logExpanded"] = logExpanded;
    uiCfg["filterHttpLog"] = filterHttpLog;
    uiCfg["currentTab"] = currentTab;
    uiCfg["closeAction"] = static_cast<int>(closeAction);
    uiCfg["windowGeometry"] = QString::fromLatin1(
        windowGeometry.toBase64());
    uiCfg["expandedWindowHeight"] = expandedWindowHeight;

    QJsonObject root;
    root["auth"] = authCfg;
    root["auto"] = autoCfg;
    root["ui"] = uiCfg;
    return root;
}

bool Config::save() const {
    const QString path = filePath();
    const QString dir = QFileInfo(path).absolutePath();

    if (!QDir().mkpath(dir)) {
        qWarning() << "无法创建目录:" << dir;
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "无法写入文件:" << path
        << "错误:" << file.errorString();
        return false;
    }

    const QByteArray bytes = QJsonDocument(toJson()).toJson(QJsonDocument::Indented);

    if (file.write(bytes) == -1) {
        qWarning() << "写入文件失败:" << path;
        return false;
    }
    file.close();

#ifdef Q_OS_UNIX
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
#endif

    return true;
}
