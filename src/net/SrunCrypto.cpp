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

    // 门户脚本 s()：v[i>>2] |= charCodeAt(i) << ((i&3)*8)，长度是码元个数。
    // 第二个字符只左移 8 位，会和前一个码元重叠。这和 UTF-8 字节布局不同。
    QVector<quint32> charsToInts(const QString &text, bool includeLength) {
        const int len = text.size();
        const int intCount = (len + 3) / 4 + (includeLength ? 1 : 0);
        QVector<quint32> v(intCount, 0);
        const ushort *codes = text.utf16();

        for (int i = 0; i < len; ++i) {
            v[i >> 2] |= static_cast<quint32>(codes[i]) << ((i & 3) * 8);
        }
        if (includeLength)
            v[intCount - 1] = static_cast<quint32>(len);
        return v;
    }

    // 与 JSON.stringify 一致，供 encodeUserInfo 拼进固定字段顺序。
    QString jsonEscape(const QString &text) {
        QString out;
        out.reserve(text.size());
        for (const QChar ch : text) {
            switch (ch.unicode()) {
            case '"':  out += QStringLiteral("\\\""); break;
            case '\\': out += QStringLiteral("\\\\"); break;
            case '\b': out += QStringLiteral("\\b"); break;
            case '\f': out += QStringLiteral("\\f"); break;
            case '\n': out += QStringLiteral("\\n"); break;
            case '\r': out += QStringLiteral("\\r"); break;
            case '\t': out += QStringLiteral("\\t"); break;
            case 0x2028:
            case 0x2029:
                out += QStringLiteral("\\u%1").arg(ch.unicode(), 4, 16, QLatin1Char('0'));
                break;
            default:
                if (ch.unicode() < 0x20) {
                    out += QStringLiteral("\\u%1").arg(ch.unicode(), 4, 16, QLatin1Char('0'));
                } else {
                    out += ch;
                }
                break;
            }
        }
        return out;
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
    QByteArray xxteaEncrypt(const QString &data, const QString &key) {
        if (data.isEmpty()) return QByteArray();

        QVector<quint32> v = charsToInts(data, true);
        QVector<quint32> k = charsToInts(key, false);
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
        // 字段顺序固定。不用 arg()，避免密码里的 %1 被当成占位符。
        QString json = QStringLiteral("{\"username\":\"");
        json += jsonEscape(username);
        json += QStringLiteral("\",\"password\":\"");
        json += jsonEscape(password);
        json += QStringLiteral("\",\"ip\":\"");
        json += jsonEscape(ip);
        json += QStringLiteral("\",\"acid\":\"");
        json += jsonEscape(acid);
        json += QStringLiteral("\",\"enc_ver\":\"srun_bx1\"}");

        const QByteArray encrypted = xxteaEncrypt(json, token);
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
