package io.github.xjtunetlogin

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.os.Handler
import android.os.Looper
import org.json.JSONObject
import java.net.SocketTimeoutException
import java.util.concurrent.Executors

data class SessionState(
    val busy: Boolean = false,
    val status: String = "连接校园 Wi-Fi 后即可登录",
    val online: JSONObject? = null,
    val logs: List<String> = emptyList()
)

/** 持有一次前台会话，跨屏幕旋转保留；所有认证请求串行执行。 */
class SessionController(context: Context) {
    private val app = context.applicationContext
    private val store = AccountStore(app)
    private val executor = Executors.newSingleThreadExecutor()
    private val main = Handler(Looper.getMainLooper())
    private val connectivity = app.getSystemService(ConnectivityManager::class.java)
    var account = Account()
        private set
    var state = SessionState(busy = true, status = "正在读取账户设置…")
        private set
    var listener: ((SessionState) -> Unit)? = null
    var loaded: (() -> Unit)? = null
    private var active = false
    private var uiActive = false
    private var backgroundActive = false
    private var ready = false
    val isReady: Boolean get() = ready
    private var closed = false
    private var registered = false
    private var manualLogout = false
    private var currentNetwork: Network? = null
    private var taskTransport: WifiTransport? = null
    private var refreshPending = false
    private val wifiNetworks = linkedSetOf<Network>()
    private var authenticatedAccount: Account? = null
    private val callback = object : ConnectivityManager.NetworkCallback() {
        override fun onLost(network: Network) {
            if (!closed && wifiNetworks.remove(network)) networkChanged()
        }
        override fun onCapabilitiesChanged(network: Network, capabilities: NetworkCapabilities) {
            if (closed) return
            if (capabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) &&
                capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_NOT_VPN)) {
                wifiNetworks.add(network)
            } else {
                wifiNetworks.remove(network)
            }
            networkChanged()
        }
    }
    private val poll = object : Runnable {
        override fun run() {
            if (!active || closed) return
            refresh(automatic = true)
            main.postDelayed(this, 30000)
        }
    }

    init {
        executor.execute {
            val result = runCatching { store.load() }
            main.post {
                if (!closed) {
                    account = result.getOrDefault(Account())
                    ready = true
                    state = state.copy(busy = false)
                    if (result.isFailure) publish("已保存账户读取失败，请重新输入并保存")
                    loaded?.invoke()
                    listener?.invoke(state)
                    if (active) networkChanged(force = true)
                }
            }
        }
    }

    fun start() {
        uiActive = true
        updateActive()
    }

    fun setBackgroundActive(enabled: Boolean) {
        backgroundActive = enabled
        updateActive()
    }

    private fun updateActive() {
        if (!uiActive && !backgroundActive) {
            active = false
            main.removeCallbacks(poll)
            return
        }
        if (closed || active) return
        active = true
        if (!registered) {
            connectivity.registerNetworkCallback(NetworkRequest.Builder()
                .addTransportType(NetworkCapabilities.TRANSPORT_WIFI).build(), callback, main)
            registered = true
        }
        if (ready) networkChanged(force = true)
        main.postDelayed(poll, 30000)
    }

    fun stop() {
        uiActive = false
        updateActive()
    }

    private fun networkChanged(force: Boolean = false) {
        val network = wifiNetworks.firstOrNull()
        if (network != currentNetwork) {
            taskTransport?.cancel()
            currentNetwork = network
            manualLogout = false
            authenticatedAccount = null
            state = state.copy(online = null)
            publish(if (network == null) "请先连接校园 Wi-Fi" else "Wi-Fi 已连接，正在查询…")
        } else if (!force) return
        if (network == null && force) publish("请先连接校园 Wi-Fi", log = false)
        if (active && network != null && ready) {
            if (state.busy) refreshPending = true else refresh(automatic = true)
        }
    }

    fun forgetAccount() {
        if (!ready || state.busy) return
        runTask("清除已保存账户…", needsWifi = false) { _, _, _ ->
            store.save(account.copy(remember = false, autoLogin = false))
            main.post {
                if (!closed) {
                    account = account.copy(remember = false, autoLogin = false)
                    app.stopService(android.content.Intent(app, AutoLoginService::class.java))
                    publish("已清除设备上保存的账户")
                }
            }
        }
    }

    fun save(updated: Account) {
        if (!ready || state.busy) return
        val error = runCatching {
            Account.validatePortal(updated.baseUrl)
            require(updated.acId.matches(Regex("[0-9]+"))) { "AC ID 应为数字" }
            if (updated.autoLogin) updated.validate()
        }.exceptionOrNull()
        if (error != null) { publish(error.message ?: "账户设置有误"); return }
        runTask("保存设置…", needsWifi = false) { _, _, _ ->
            store.save(updated)
            main.post {
                if (!closed) {
                    if (account.baseUrl != updated.baseUrl || account.acId != updated.acId) {
                        state = state.copy(online = null)
                        authenticatedAccount = null
                    }
                    account = updated
                    updateBackgroundService(updated.autoLogin && updated.remember)
                    publish("设置已保存")
                }
            }
        }
    }

    fun login(updated: Account) = login(updated, automatic = false)

    private fun login(updated: Account, automatic: Boolean) {
        if (!ready || state.busy) return
        val error = runCatching { updated.validate() }.exceptionOrNull()
        if (error != null) { publish(error.message ?: "账户设置有误"); return }
        manualLogout = false
        runTask("开始登录…", baseOverride = updated.baseUrl) { client, network, _ ->
            if (!automatic) {
                store.save(updated)
                main.post { if (!closed) { account = updated; if (uiActive) updateBackgroundService(updated.autoLogin && updated.remember) } }
            }
            val result = client!!.login(updated) { stage -> main.post { if (!closed && network == currentNetwork) publish(stage, log = false) } }
            main.post {
                if (!closed && network == currentNetwork) {
                    authenticatedAccount = updated
                    state = state.copy(online = result)
                    publish("已连接校园网")
                }
            }
        }
    }

    fun logout(updated: Account) {
        if (state.busy) return
        manualLogout = true
        val identity = authenticatedAccount ?: updated
        runTask("正在注销…", baseOverride = identity.baseUrl) { client, _, _ ->
            client!!.logout(identity)
            main.post { if (!closed) { state = state.copy(online = null); publish("已注销，自动登录已暂停至下次连接 Wi-Fi") } }
        }
    }

    fun refresh(automatic: Boolean = false) {
        if (!ready || state.busy || (automatic && !active)) return
        if (automatic && currentNetwork == null) return
        val requestNetwork = currentNetwork
        var shouldLogin = false
        runTask("查询在线状态…", silent = automatic, timeoutMs = if (automatic) 2500 else 8000, after = {
            if (shouldLogin && active && requestNetwork != null && requestNetwork == currentNetwork &&
                account.autoLogin && account.remember && !manualLogout) {
                login(account, automatic = true)
            }
        }) { client, network, transport ->
            val result = try {
                client!!.query()
            } catch (error: Exception) {
                if (!automatic || !account.autoLogin || !account.remember || manualLogout || network == null ||
                    !runCatching { transport!!.isCampusPortalRedirect() }.getOrDefault(false)) {
                    throw error
                }
                shouldLogin = true
                null
            }
            if (result == null) return@runTask
            val online = result.optString("error") == "ok"
            shouldLogin = !online
            main.post {
                if (!closed && network == currentNetwork) {
                    state = state.copy(online = if (online) result else null)
                    publish(if (online) "已连接校园网" else "校园网未登录", log = !automatic)
                }
            }
        }
    }

    fun checkInternet() = runTask("检测 Wi-Fi 外网连接…") { _, network, transport ->
        val reachable = transport!!.checkInternet()
        main.post { if (!closed && network == currentNetwork) publish(if (reachable) "Wi-Fi 外网连接正常" else "Wi-Fi 外网尚未连通，请登录校园网") }
    }

    private fun runTask(label: String, needsWifi: Boolean = true, silent: Boolean = false,
                        baseOverride: String? = null, timeoutMs: Int = 8000, after: () -> Unit = {},
                        task: (SrunClient?, Network?, WifiTransport?) -> Unit) {
        if (closed || state.busy) return
        val network = if (needsWifi) currentNetwork else null
        if (needsWifi && network == null) { publish("请先连接校园 Wi-Fi"); return }
        state = state.copy(busy = true)
        publish(label, log = !silent)
        val baseUrl = baseOverride ?: account.baseUrl
        val transport = network?.let { WifiTransport(it, baseUrl, timeoutMs) }
        taskTransport = transport
        executor.execute {
            try {
                task(transport?.let { SrunClient(it) }, network, transport)
            } catch (e: Exception) {
                val message = when (e) {
                    is SocketTimeoutException -> "请求超时，请检查校园 Wi-Fi 后重试"
                    is java.io.IOException -> "网络请求失败，请检查校园 Wi-Fi 和认证地址"
                    is IllegalArgumentException, is IllegalStateException -> e.message ?: "操作失败"
                    else -> "操作失败，请检查配置后重试"
                }
                // 不记录异常堆栈、原始响应或请求 URL，避免暴露认证参数。
                main.post { if (!closed && (network == null || network == currentNetwork)) publish(message.take(200)) }
            } finally {
                main.post {
                    if (!closed) {
                        taskTransport = null
                        state = state.copy(busy = false)
                        listener?.invoke(state)
                        after()
                        if (refreshPending && !state.busy) {
                            refreshPending = false
                            if (active && currentNetwork != null) refresh(automatic = true)
                        }
                    }
                }
            }
        }
    }

    private fun publish(message: String, log: Boolean = true) {
        val logs = if (log) (state.logs + message).takeLast(40) else state.logs
        state = state.copy(status = message, logs = logs)
        listener?.invoke(state)
    }

    private fun updateBackgroundService(enabled: Boolean) {
        val intent = android.content.Intent(app, AutoLoginService::class.java)
        if (enabled && uiActive) app.startForegroundService(intent)
        else if (!enabled) app.stopService(intent)
    }

    fun close() {
        backgroundActive = false
        stop()
        closed = true
        if (registered) { connectivity.unregisterNetworkCallback(callback); registered = false }
        wifiNetworks.clear()
        currentNetwork = null
        listener = null
        loaded = null
        executor.shutdownNow()
        main.removeCallbacksAndMessages(null)
    }
}
