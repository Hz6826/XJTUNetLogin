#ifndef SRUNCRYPTO_H
#define SRUNCRYPTO_H

#include <QByteArray>
#include <QString>

namespace SrunCrypto {

    // 自定义字母表 Base64 编码
    QString customBase64Encode(const QByteArray &data);

    // XXTEA 加密（Srun 变体），输入输出都用字节串
    QByteArray xxteaEncrypt(const QByteArray &data, const QByteArray &key);

    // HMAC-MD5，返回 32 位小写 hex 字符串
    QString hmacMd5(const QString &password, const QString &token);

    // SHA1，返回 40 位小写 hex 字符串
    QString sha1(const QString &text);

    // 加密用户信息，返回 "{SRBX1}..." 形式
    QString encodeUserInfo(const QString &username,
                           const QString &password,
                           const QString &ip,
                           const QString &acid,
                           const QString &token);

    // 拼接 chksum 并计算 SHA1
    QString buildChksum(const QString &token,
                        const QString &username,
                        const QString &hmd5,
                        const QString &acId,
                        const QString &ip,
                        int n,
                        int type,
                        const QString &info);

} // namespace SrunCrypto

#endif // SRUNCRYPTO_H
