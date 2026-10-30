package io.github.xjtunetlogin

import android.net.Network
import java.io.IOException
import java.net.HttpURLConnection
import java.net.Proxy
import java.net.URL
import java.net.URLEncoder
import java.net.URI

class WifiTransport(private val network: Network, private val baseUrl: String,
                    private val timeoutMs: Int = 8000) : PortalTransport {
    private var cancelled = false
    private var activeConnection: HttpURLConnection? = null

    @Synchronized fun cancel() {
        cancelled = true
        activeConnection?.disconnect()
    }
    override fun get(path: String, params: Map<String, String>): String {
        Account.validatePortal(baseUrl)
        val query = params.entries.joinToString("&") { encode(it.key) + "=" + encode(it.value) }
        val url = URL(baseUrl.trimEnd('/') + path + if (query.isEmpty()) "" else "?$query")
        return connection(url).useConnection { connection ->
            connection.setRequestProperty("User-Agent", "XJTUNetLogin/1.0 Android")
            connection.setRequestProperty("Accept", "application/json, text/javascript, */*; q=0.01")
            connection.setRequestProperty("X-Requested-With", "XMLHttpRequest")
            connection.setRequestProperty("Referer", "${baseUrl.trimEnd('/')}/srun_portal_pc")
            connection.setRequestProperty("Origin", baseUrl.trimEnd('/'))
            val code = connection.responseCode
            if (code != 200) throw IOException("认证服务器返回 HTTP $code")
            // 限制响应体大小，避免劫持页面占满内存；错误信息中不包含带凭据的 URL。
            connection.inputStream.use { stream ->
                val output = java.io.ByteArrayOutputStream()
                val buffer = ByteArray(4096)
                while (true) {
                    val count = stream.read(buffer)
                    if (count < 0) break
                    if (output.size() + count > 262144) throw IOException("认证响应过大")
                    output.write(buffer, 0, count)
                }
                val bytes = output.toByteArray()
                bytes.toString(Charsets.UTF_8)
            }
        }
    }

    fun checkInternet(): Boolean = connection(URL("http://firefox-portal-detection.com/generate_204"))
        .useConnection { it.responseCode == 204 }

    fun isCampusPortalRedirect(): Boolean = connection(URL("http://firefox-portal-detection.com/generate_204"))
        .useConnection { connection ->
            val code = connection.responseCode
            isConfiguredPortalRedirect(code, connection.getHeaderField("Location"), baseUrl)
        }

    @Synchronized private fun connection(url: URL): HttpURLConnection {
        if (cancelled) throw java.io.InterruptedIOException("网络已变化")
        return (network.openConnection(url, Proxy.NO_PROXY) as HttpURLConnection).apply {
            connectTimeout = timeoutMs
            readTimeout = timeoutMs
            instanceFollowRedirects = false
            useCaches = false
            activeConnection = this
        }
    }

    private fun <T> HttpURLConnection.useConnection(block: (HttpURLConnection) -> T): T =
        try { block(this) } finally {
            disconnect()
            synchronized(this@WifiTransport) { if (activeConnection === this) activeConnection = null }
        }

    companion object {
        internal fun isConfiguredPortalRedirect(code: Int, location: String?, baseUrl: String): Boolean {
            if (code !in 300..399 || location.isNullOrBlank()) return false
            val portal = runCatching { URI(baseUrl) }.getOrNull() ?: return false
            val redirect = runCatching { URI(location) }.getOrNull() ?: return false
            fun port(uri: URI) = if (uri.port != -1) uri.port else if (uri.scheme.equals("https", true)) 443 else 80
            return redirect.isAbsolute && redirect.rawUserInfo == null &&
                redirect.scheme.equals(portal.scheme, true) && redirect.host != null &&
                redirect.host.equals(portal.host, true) && port(redirect) == port(portal)
        }

        fun encode(value: String): String = URLEncoder.encode(value, "UTF-8").replace("+", "%20")
    }
}
