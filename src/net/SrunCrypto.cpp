//
// Created by Hz6826 on 2026/10/10.
//

#include "net/SrunCrypto.h"

#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QVector>

#include <cstring>

namespace {
    constexpr char CUSTOM_ALPHABET[] =
            "LVoJPiCN2R8G90yg+hmFHuacZ1OWMnrsSTXkYpUq/3dlbfKwv6xztjI7DeBE45QA";
    constexpr char STANDARD_ALPHABET[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    // 字节串 -> 32 位整数数组（小端）
    QVector<quint32> bytesToInts(const QByteArray &data, bool includeLength) {
        const int len = data.size();
        const int intCount = (len + 3) / 4 + (includeLength ? 1 : 0);
        QVector<quint32> v(intCount, 0);

        for (int i = 0; i < len; ++i) {
            v[i >> 2] |= static_cast<quint32>(static_cast<quint8>(data[i]))
                    << ((i & 3) * 8);
        }
        if (includeLength) {
            v[intCount - 1] = static_cast<quint32>(len);
        }
        return v;
    }

    // 32 位整数数组 -> 字节串
    QByteArray intsToBytes(const QVector<quint32> &v, bool includeLength) {
        const int d = v.size();
        int c = (d - 1) << 2;

        if (includeLength) {
            const quint32 m = v[d - 1];
            if (m < static_cast<quint32>(c - 3) || m > static_cast<quint32>(c)) {
                return QByteArray();
            }
            c = static_cast<int>(m);
        }

        QByteArray result;
        result.reserve(d * 4);
        for (quint32 val: v) {
            result.append(static_cast<char>(val & 0xff));
            result.append(static_cast<char>((val >> 8) & 0xff));
            result.append(static_cast<char>((val >> 16) & 0xff));
            result.append(static_cast<char>((val >> 24) & 0xff));
        }
        if (includeLength) {
            result.truncate(c);
        }
        return result;
    }
} // namespace

namespace SrunCrypto {
    QByteArray xxteaEncrypt(const QByteArray &data, const QByteArray &key) {
        if (data.isEmpty()) return QByteArray();

        QVector<quint32> v = bytesToInts(data, true);
        QVector<quint32> k = bytesToInts(key, false);
        while (k.size() < 4) k.append(0);

        const int n = v.size() - 1;
        if (n < 1) return QByteArray();

        quint32 z = v[n];
        quint32 y = v[0];
        quint32 d = 0;
        const quint32 c = 0x9E3779B9u;
        int q = 6 + 52 / (n + 1);

        while (q-- > 0) {
            d += c;
            const quint32 e = (d >> 2) & 3;
            for (int p = 0; p < n; ++p) {
                y = v[p + 1];
                const quint32 mx = ((z >> 5) ^ (y << 2)) +
                                   (((y >> 3) ^ (z << 4)) ^ (d ^ y)) +
                                   (k[(p & 3) ^ e] ^ z);
                z = v[p] = v[p] + mx;
            }
            y = v[0];
            const quint32 mx = ((z >> 5) ^ (y << 2)) +
                               (((y >> 3) ^ (z << 4)) ^ (d ^ y)) +
                               (k[(n & 3) ^ e] ^ z);
            z = v[n] = v[n] + mx;
        }

        return intsToBytes(v, false);
    }

    QString customBase64Encode(const QByteArray &data) {
        // Qt 的 toBase64() 默认不带换行，就是标准 Base64
        const QByteArray std = data.toBase64();
        QString result;
        result.reserve(std.size());

        for (char ch: std) {
            const char *pos = std::strchr(STANDARD_ALPHABET, ch);
            if (pos != nullptr) {
                result.append(QLatin1Char(CUSTOM_ALPHABET[pos - STANDARD_ALPHABET]));
            } else {
                // '=' 或其它字符原样保留
                result.append(QLatin1Char(ch));
            }
        }
        return result;
    }

    QString hmacMd5(const QString &password, const QString &token) {
        const QByteArray key = token.toUtf8();
        const QByteArray data = password.toUtf8();
        const QByteArray hash = QMessageAuthenticationCode::hash(
            data, key, QCryptographicHash::Md5);
        return QString::fromLatin1(hash.toHex());
    }

    QString sha1(const QString &text) {
        const QByteArray hash = QCryptographicHash::hash(
            text.toUtf8(), QCryptographicHash::Sha1);
        return QString::fromLatin1(hash.toHex());
    }

    QString encodeUserInfo(const QString &username,
                           const QString &password,
                           const QString &ip,
                           const QString &acid,
                           const QString &token) {
        // 手动拼 JSON，字段顺序和 Python 完全一致，方便日后校验
        const QString json = QString(
            R"({"username":"%1","password":"%2","ip":"%3","acid":"%4","enc_ver":"srun_bx1"})"
        ).arg(username, password, ip, acid);

        const QByteArray encrypted = xxteaEncrypt(json.toUtf8(), token.toUtf8());
        return QStringLiteral("{SRBX1}") + customBase64Encode(encrypted);
    }

    QString buildChksum(const QString &token,
                        const QString &username,
                        const QString &hmd5,
                        const QString &acId,
                        const QString &ip,
                        int n,
                        int type,
                        const QString &info) {
        QString s = token + username;
        s += token + hmd5;
        s += token + acId;
        s += token + ip;
        s += token + QString::number(n);
        s += token + QString::number(type);
        s += token + info;
        return sha1(s);
    }
} // namespace SrunCrypto
