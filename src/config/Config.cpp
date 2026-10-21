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
#include <QSaveFile>

#include <algorithm>   // std::clamp

namespace {

    // exe 同目录的 config.json —— 便携模式的标记文件
    QString portableConfigPath() {
        return QCoreApplication::applicationDirPath() + "/config.json";
    }

    // 独立的便携标记文件：避免D"配置文件被备份走"后便携模式丢失
    QString portableFlagPath() {
        return QCoreApplication::applicationDirPath() + "/portable.flag";
    }

    bool isPortableMode() {
        // 新标记优先；config.json 存在视为兼容旧版便携部署
        return QFile::exists(portableFlagPath())
            || QFile::exists(portableConfigPath());
    }

    // 损坏的配置文件改名备份，绝不静默覆盖
    void backupCorrupted(const QString &path) {
        const QString bak = path + ".bak";
        QFile::remove(bak);   // 只保留最近一份备份
        if (!QFile::rename(path, bak)) {
            qWarning() << "无法备份损坏的配置文件:" << path
                       << "->" << bak;
            return;
        }
        qWarning() << "损坏的配置文件已备份为:" << bak;

        // 便携版补一个 portable.flag，保证 portable 状态不丢
        if (path == portableConfigPath()
            && !QFile::exists(portableFlagPath())) {
            QFile flag(portableFlagPath());
            if (flag.open(QIODevice::WriteOnly)) {
                flag.close();
            } else {
                qWarning() << "无法创建便携标记:" << portableFlagPath();
            }
        }
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
        // 文件存在但打不开也要提示，纯路径不存在则安静返回默认值
        if (QFile::exists(path)) {
            qWarning() << "无法读取配置文件:" << path
                       << "错误:" << file.errorString();
        }
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
        backupCorrupted(path);   // 先备份，再回默认，绝不静默覆盖
        return {};
    }

    if (!doc.isObject()) {
        qWarning() << "config.json 根节点不是 JSON 对象";
        backupCorrupted(path);   // 同上
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
    c.baseUrlOverride = authCfg.value("baseUrlOverride").toString("");

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

    const QJsonObject debugCfg = root.value("debug").toObject();

    c.logSensitive = debugCfg.value("logSensitive").toBool(false);

    return c;
}

QJsonObject Config::toJson() const {
    QJsonObject authCfg;
    authCfg["username"] = username;
    authCfg["password"] = password;
    authCfg["acId"] = acId;
    authCfg["domain"] = domain;
    authCfg["baseUrlOverride"] = baseUrlOverride;

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

    QJsonObject debugCfg;
    debugCfg["logSensitive"] = logSensitive;

    QJsonObject root;
    root["auth"] = authCfg;
    root["auto"] = autoCfg;
    root["ui"] = uiCfg;
    root["debug"] = debugCfg;
    return root;
}

bool Config::save() const {
    const QString path = filePath();
    const QString dir = QFileInfo(path).absolutePath();

    if (!QDir().mkpath(dir)) {
        qWarning() << "无法创建目录:" << dir;
        return false;
    }

    // QSaveFile：先写临时文件，commit() 时原子 rename 覆盖；
    // 磁盘满 / 崩溃 / 断电时原文件保持完好。
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "无法写入文件:" << path
                   << "错误:" << file.errorString();
        return false;
    }

    const QByteArray bytes =
        QJsonDocument(toJson()).toJson(QJsonDocument::Indented);

    // 按字节数校验，write() 返回 -1 或短写都要判失败
    if (const qint64 written = file.write(bytes); written != bytes.size()) {
        qWarning() << "写入文件失败:" << path
                   << "错误:" << file.errorString();
        file.cancelWriting();   // 丢弃临时文件，原文件不受影响
        return false;
    }

    if (!file.commit()) {
        qWarning() << "提交文件失败:" << path
                   << "错误:" << file.errorString();
        return false;
    }

#ifdef Q_OS_UNIX
    QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
#endif

    return true;
}
