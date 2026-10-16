//
// Created by Hz6826 on 2026/10/16.
//

#ifndef SRUN_HTTP_UTILS_H
#define SRUN_HTTP_UTILS_H

#include <QByteArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QString>
#include <QList>
#include <QPair>

namespace SrunHttpUtils {

    // 解析 JSON 或 JSONP（形如 cb({...})）
    QJsonObject parseJsonOrJsonp(const QByteArray &data, bool *ok);

    // 统一 User-Agent
    QString getUAHeader();

    // 截断响应体用于日志预览
    QString previewBody(const QByteArray &data, int maxLen = 200);

    // 拼接 query string（key/value 会做 URL 编码）
    QString buildQuery(const QList<QPair<QString, QString>> &params);

    // 为 srun 门户接口设置通用请求头
    void applyCommonHeaders(QNetworkRequest &req, const QString &baseUrl);

} // namespace SrunHttpUtils

#endif // SRUN_HTTP_UTILS_H
