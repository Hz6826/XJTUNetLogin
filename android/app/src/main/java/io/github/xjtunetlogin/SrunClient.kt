package io.github.xjtunetlogin

import org.json.JSONObject
import java.net.URI

data class Account(
    val username: String = "",
    val password: String = "",
    val domain: String = "@stu",
    val acId: String = "1",
    val baseUrl: String = "http://10.6.21.2",
    val remember: Boolean = false,
    val autoLogin: Boolean = false
) {
    val fullUsername: String get() = if (domain.isEmpty() || username.endsWith(domain)) username else username + domain

    fun validate(requirePassword: Boolean = true) {
        require(username.isNotBlank()) { "请输入校园网账号" }
        if (requirePassword) require(password.isNotEmpty()) { "请输入密码" }
        require(acId.matches(Regex("[0-9]+"))) { "AC ID 应为数字" }
        validatePortal(baseUrl)
    }

    companion object {
        fun validatePortal(baseUrl: String) {
            val uri = runCatching { URI(baseUrl) }.getOrNull()
            require(uri != null && uri.host != null && uri.rawUserInfo == null &&
                uri.rawQuery == null && uri.rawFragment == null && uri.path.orEmpty() in listOf("", "/") &&
                (uri.scheme == "https" || (uri.scheme == "http" && uri.host == "10.6.21.2"))) {
                "认证地址应为 http://10.6.21.2 或 HTTPS 网关根地址"
            }
        }
    }
}

fun interface PortalTransport {
    fun get(path: String, params: Map<String, String>): String
}

/** 同步协议层，调用方负责在工作线程执行。 */
class SrunClient(private val transport: PortalTransport, private val pause: () -> Unit = { Thread.sleep(1000) }) {
    private fun request(path: String, params: Map<String, String> = emptyMap()): JSONObject = parse(transport.get(path, params))

    fun query(): JSONObject {
        val result = request("/cgi-bin/rad_user_info")
        require(result.optString("error") in listOf("ok", "not_online_error")) { message(result) }
        return result
    }

    fun login(account: Account, stage: (String) -> Unit = {}): JSONObject {
        account.validate()
        stage("获取认证令牌…")
        val first = request("/cgi-bin/get_challenge", mapOf("username" to account.fullUsername, "ip" to ""))
        val ip = first.optString("client_ip")
        require(ip.isNotBlank()) { "认证服务器未返回设备 IP" }
        val second = request("/cgi-bin/get_challenge", mapOf("username" to account.fullUsername, "ip" to ip))
        val token = second.optString("challenge")
        require(token.isNotBlank()) { "未获取到认证令牌" }
        stage("正在认证…")
        val hmd5 = SrunCrypto.hmacMd5(account.password, token)
        val info = SrunCrypto.info(account.fullUsername, account.password, ip, account.acId, token)
        val response = request("/cgi-bin/srun_portal", linkedMapOf(
            "action" to "login", "username" to account.fullUsername, "password" to "{MD5}$hmd5",
            "os" to "Android", "name" to "Android", "double_stack" to "0",
            "chksum" to SrunCrypto.checksum(token, account.fullUsername, hmd5, account.acId, ip, info),
            "info" to info, "ac_id" to account.acId, "ip" to ip, "n" to "200", "type" to "1"
        ))
        require(response.optString("error") == "ok" || response.optString("suc_msg") == "login_ok") { message(response) }
        stage("确认在线状态…")
        return verify("ok", "认证已提交，暂未确认上线，请刷新状态")
    }

    fun logout(account: Account): JSONObject {
        Account.validatePortal(account.baseUrl)
        require(account.acId.matches(Regex("[0-9]+"))) { "AC ID 应为数字" }
        val online = query()
        if (online.optString("error") == "not_online_error") return online
        val ip = online.optString("online_ip").ifBlank { online.optString("client_ip") }
        require(ip.isNotBlank()) { "未获取到在线 IP，请刷新后重试" }
        // 注销使用服务器给出的在线身份；保留输入框中的账户配置。
        val user = online.optString("user_name").ifBlank { account.fullUsername }
        val domain = online.optString("domain").removePrefix("@")
        val fullUser = if (domain.isNotEmpty() && '@' !in user) "$user@$domain" else user
        val response = request("/cgi-bin/srun_portal", mapOf(
            "action" to "logout", "username" to fullUser, "ip" to ip, "ac_id" to account.acId
        ))
        require(response.optString("error") == "ok") { message(response) }
        return verify("not_online_error", "注销已提交，暂未确认下线，请刷新状态")
    }

    private fun verify(expected: String, failure: String): JSONObject {
        repeat(3) {
            pause()
            val result = try { query() } catch (_: java.io.IOException) { null }
            if (result?.optString("error") == expected) return result
        }
        error(failure)
    }

    companion object {
        fun parse(body: String): JSONObject {
            val text = body.trim().removePrefix("\uFEFF").trim()
            if (text.startsWith("{")) return JSONObject(text)
            require(Regex("^[\\w.$]+\\s*\\([\\s\\S]*\\)\\s*;?$").matches(text)) { "认证响应格式异常，请确认 Wi-Fi 和认证地址" }
            return JSONObject(text.substring(text.indexOf('(') + 1, text.lastIndexOf(')')))
        }

        fun message(obj: JSONObject): String = listOf("error_msg", "ploy_msg", "error", "res")
            .map { obj.optString(it) }.firstOrNull { it.isNotBlank() } ?: "认证服务器返回未知错误"
    }
}
