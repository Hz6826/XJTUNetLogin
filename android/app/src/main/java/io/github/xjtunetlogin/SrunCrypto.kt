package io.github.xjtunetlogin

import org.json.JSONObject
import java.security.MessageDigest
import java.util.Base64
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec

/** Srun 门户的 xencode 变体，运算分组与仓库 C++ 实现一致。 */
object SrunCrypto {
    private const val STANDARD = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
    private const val CUSTOM = "LVoJPiCN2R8G90yg+hmFHuacZ1OWMnrsSTXkYpUq/3dlbfKwv6xztjI7DeBE45QA"

    private fun words(data: ByteArray, length: Boolean): IntArray {
        val result = IntArray((data.size + 3) / 4 + if (length) 1 else 0)
        data.forEachIndexed { i, byte ->
            result[i / 4] = result[i / 4] or ((byte.toInt() and 255) shl ((i and 3) * 8))
        }
        if (length) result[result.lastIndex] = data.size
        return result
    }

    fun xencode(data: ByteArray, key: ByteArray): ByteArray {
        if (data.isEmpty()) return byteArrayOf()
        val v = words(data, true)
        val k = words(key, false).copyOf(maxOf(4, (key.size + 3) / 4))
        val n = v.lastIndex
        var z = v[n]
        var sum = 0
        repeat(6 + 52 / v.size) {
            sum += 0x9E3779B9.toInt()
            val e = (sum ushr 2) and 3
            for (p in 0..n) {
                val y = v[if (p == n) 0 else p + 1]
                val mix = ((z ushr 5) xor (y shl 2)) +
                    (((y ushr 3) xor (z shl 4)) xor (sum xor y)) +
                    (k[(p and 3) xor e] xor z)
                v[p] += mix
                z = v[p]
            }
        }
        return ByteArray(v.size * 4) { i -> (v[i / 4] ushr ((i and 3) * 8)).toByte() }
    }

    fun customBase64(data: ByteArray): String = Base64.getEncoder().encodeToString(data).map {
        val index = STANDARD.indexOf(it)
        if (index < 0) it else CUSTOM[index]
    }.joinToString("")

    private fun ByteArray.hex() = joinToString("") { "%02x".format(it.toInt() and 255) }

    fun hmacMd5(password: String, token: String): String = Mac.getInstance("HmacMD5").run {
        init(SecretKeySpec(token.toByteArray(Charsets.UTF_8), "HmacMD5"))
        doFinal(password.toByteArray(Charsets.UTF_8)).hex()
    }

    fun info(username: String, password: String, ip: String, acId: String, token: String): String {
        // 固定字段顺序，使用 JSON 转义以支持引号、反斜线和 Unicode 密码。
        val json = "{\"username\":${JSONObject.quote(username)},\"password\":${JSONObject.quote(password)}," +
            "\"ip\":${JSONObject.quote(ip)},\"acid\":${JSONObject.quote(acId)},\"enc_ver\":\"srun_bx1\"}"
        return "{SRBX1}" + customBase64(xencode(json.toByteArray(Charsets.UTF_8), token.toByteArray(Charsets.UTF_8)))
    }

    fun checksum(token: String, username: String, hmd5: String, acId: String, ip: String, info: String): String {
        val text = listOf(username, hmd5, acId, ip, "200", "1", info).joinToString("") { token + it }
        return MessageDigest.getInstance("SHA-1").digest(text.toByteArray(Charsets.UTF_8)).hex()
    }
}
