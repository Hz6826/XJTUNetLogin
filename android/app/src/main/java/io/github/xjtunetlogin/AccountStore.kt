package io.github.xjtunetlogin

import android.content.Context
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.Base64
import org.json.JSONObject
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

/** 账户整体加密，密钥由 Android Keystore 持有；清除记住账户时同步移除密文。 */
class AccountStore(context: Context) {
    private val prefs = context.getSharedPreferences("account", Context.MODE_PRIVATE)
    private val alias = "xjtunetlogin.account.v1"

    private fun key(): SecretKey {
        val store = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
        (store.getKey(alias, null) as? SecretKey)?.let { return it }
        return KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore").run {
            init(KeyGenParameterSpec.Builder(alias, KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE).build())
            generateKey()
        }
    }

    fun load(): Account {
        val encrypted = prefs.getString("ciphertext", null) ?: return Account(
            domain = prefs.getString("domain", "@stu") ?: "@stu",
            acId = prefs.getString("acId", "1") ?: "1",
            baseUrl = prefs.getString("baseUrl", "http://10.6.21.2") ?: "http://10.6.21.2")
        val parts = encrypted.split(":")
        require(parts.size == 2) { "账户存储格式异常" }
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.DECRYPT_MODE, key(), GCMParameterSpec(128, Base64.decode(parts[0], Base64.NO_WRAP)))
        val json = JSONObject(String(cipher.doFinal(Base64.decode(parts[1], Base64.NO_WRAP)), Charsets.UTF_8))
        return Account(json.getString("username"), json.getString("password"), json.getString("domain"),
            json.getString("acId"), json.getString("baseUrl"), true, json.optBoolean("autoLogin"))
    }

    fun save(account: Account) {
        if (!account.remember) {
            check(prefs.edit().clear().putString("domain", account.domain).putString("acId", account.acId)
                .putString("baseUrl", account.baseUrl).commit()) { "清除账户失败，请重试" }
            return
        }
        val json = JSONObject().put("username", account.username).put("password", account.password)
            .put("domain", account.domain).put("acId", account.acId).put("baseUrl", account.baseUrl)
            .put("autoLogin", account.autoLogin)
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.ENCRYPT_MODE, key())
        val encrypted = cipher.doFinal(json.toString().toByteArray(Charsets.UTF_8))
        val payload = Base64.encodeToString(cipher.iv, Base64.NO_WRAP) + ":" + Base64.encodeToString(encrypted, Base64.NO_WRAP)
        check(prefs.edit().putString("ciphertext", payload).commit()) { "保存账户失败，请重试" }
    }
}
