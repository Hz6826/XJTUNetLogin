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

QString Config::filePath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return dir + "/config.json";
}

Config Config::load() {
    const QString path = filePath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "config.json 解析失败:" << err.errorString()
                   << "@ offset" << err.offset;
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
    c.ssid = autoCfg.value("ssid").toString("XJTU_STU");

    const QJsonObject uiCfg = root.value("ui").toObject();

    c.logExpanded = uiCfg.value("logExpanded").toBool(true);
    c.logHttp = uiCfg.value("logHttp").toBool(false);

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
    autoCfg["ssid"] = ssid;

    QJsonObject uiCfg;
    uiCfg["logExpanded"] = logExpanded;
    uiCfg["logHttp"] = logHttp;

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
        qWarning() << "无法写入文件:" << path;
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
