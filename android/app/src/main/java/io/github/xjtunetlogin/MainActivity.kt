package io.github.xjtunetlogin

import android.app.Activity
import android.content.Intent
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.os.Bundle
import android.os.Build
import android.net.wifi.WifiManager
import android.net.wifi.WifiNetworkSuggestion
import android.provider.Settings
import android.text.InputType
import android.view.View
import android.view.WindowInsets
import android.widget.*
import java.util.Locale

class MainActivity : Activity() {
    private lateinit var controller: SessionController
    private lateinit var status: TextView
    private lateinit var badge: TextView
    private lateinit var details: TextView
    private lateinit var logs: TextView
    private lateinit var progress: ProgressBar
    private lateinit var username: EditText
    private lateinit var password: EditText
    private lateinit var domain: EditText
    private lateinit var acId: EditText
    private lateinit var baseUrl: EditText
    private lateinit var remember: Switch
    private lateinit var auto: Switch
    private lateinit var logout: Button
    private val controls = mutableListOf<View>()
    private val green = Color.rgb(25, 102, 83)
    private val ink = Color.rgb(26, 45, 39)
    private val muted = Color.rgb(102, 120, 113)
    private var initialized = false
    private var loadedAccount = false
    private data class Retained(val controller: SessionController, val draft: Account)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val retained = lastNonConfigurationInstance as? Retained
        controller = retained?.controller ?: SessionHolder.get(applicationContext)
        buildUi()
        populate(retained?.draft ?: controller.account)
        loadedAccount = retained != null || controller.isReady
        initialized = true
        render(controller.state)
    }

    override fun onStart() {
        super.onStart()
        controller.listener = ::render
        controller.loaded = { applyLoadedAccount() }
        if (controller.isReady) applyLoadedAccount()
        render(controller.state)
        controller.start()
        if (controller.isReady) startSavedAutoLogin()
    }

    private fun applyLoadedAccount() {
        if (!loadedAccount) {
            populate(controller.account)
            loadedAccount = true
        }
        render(controller.state)
        startSavedAutoLogin()
    }

    private fun startSavedAutoLogin() {
        if (controller.account.remember && controller.account.autoLogin) {
            startForegroundService(Intent(this, AutoLoginService::class.java))
        }
    }

    override fun onStop() {
        controller.listener = null
        controller.loaded = null
        controller.stop()
        super.onStop()
    }

    override fun onRetainNonConfigurationInstance(): Any = Retained(controller, readAccount())

    override fun onDestroy() {
        super.onDestroy()
    }

    private fun buildUi() {
        val scroll = ScrollView(this).apply { isFillViewport = true; setBackgroundColor(Color.rgb(244, 246, 243)) }
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            isFocusableInTouchMode = true
            setPadding(dp(22), dp(20), dp(22), dp(28))
        }
        scroll.addView(root)
        scroll.setOnApplyWindowInsetsListener { view, insets ->
            if (Build.VERSION.SDK_INT >= 30) {
                val edges = insets.getInsets(WindowInsets.Type.systemBars() or WindowInsets.Type.ime())
                view.setPadding(edges.left, edges.top, edges.right, edges.bottom)
            } else {
                @Suppress("DEPRECATION")
                view.setPadding(insets.systemWindowInsetLeft, insets.systemWindowInsetTop,
                    insets.systemWindowInsetRight, insets.systemWindowInsetBottom)
            }
            insets
        }
        setContentView(scroll)
        root.addView(text("XJTU  /  CAMPUS NETWORK", 11, green, bold = true).apply { letterSpacing = 0.14f })
        root.addView(text("仙交网络助手", 30, ink, bold = true).apply { setPadding(0, dp(10), 0, dp(5)) })
        root.addView(text("连接校园，让每一次开始更轻松。", 14, muted))

        val connection = card(root, dark = true)
        badge = text("●  等待连接", 12, Color.rgb(177, 226, 202), true)
        connection.addView(badge)
        status = text("连接校园 Wi-Fi 后即可登录", 21, Color.WHITE, true).apply { setPadding(0, dp(14), 0, dp(12)) }
        connection.addView(status)
        progress = ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal).apply {
            isIndeterminate = true
            visibility = View.GONE
        }
        connection.addView(progress, LinearLayout.LayoutParams(-1, dp(3)))
        connection.addView(text("认证请求通过当前 Wi-Fi 发送", 12, Color.rgb(192, 216, 204)))

        val account = card(root)
        account.addView(text("校园网账户", 18, ink, true))
        username = field(account, "账号", "请输入学号 / 工号", InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD)
        password = field(account, "密码", "请输入校园网密码", InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_PASSWORD)
        val show = CheckBox(this).apply {
            text = "显示密码"
            setTextColor(muted)
            textSize = 12f
            setOnCheckedChangeListener { _, checked ->
                password.inputType = InputType.TYPE_CLASS_TEXT or
                    if (checked) InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD else InputType.TYPE_TEXT_VARIATION_PASSWORD
                password.setSelection(password.length())
            }
        }
        account.addView(show)
        controls.add(show)
        remember = toggle(account, "记住账户", "使用系统密钥加密保存在此设备")
        auto = toggle(account, "自动登录", "保存设置后在后台检测校园网并自动登录")
        remember.setOnCheckedChangeListener { _, checked ->
            if (!checked) auto.isChecked = false
            auto.isEnabled = checked && !controller.state.busy
            // 关闭记住账户立即清除磁盘密文。
            if (initialized && !checked) controller.forgetAccount()
        }
        action(account, "登录校园网", primary = true) { controller.login(readAccount()) }
        logout = action(account, "注销当前连接") { controller.logout(readAccount()) }
        action(account, "保存账户与设置") { controller.save(readAccount()) }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            account.addView(text("可向系统添加 XJTU-STU 自动连接建议，接入后由本应用完成认证。", 12, muted))
            action(account, "启用校园 Wi-Fi 自动连接") { suggestCampusWifi() }
        } else {
            account.addView(text("请在系统 Wi-Fi 设置中连接 XJTU-STU 并启用自动连接。", 12, muted))
        }

        val info = card(root)
        info.addView(text("在线信息", 18, ink, true))
        details = text("登录后显示账户、IP、套餐与用量", 14, muted).apply {
            setPadding(0, dp(14), 0, dp(8)); setTextIsSelectable(true)
        }
        info.addView(details)
        action(info, "刷新在线状态") { controller.refresh() }
        action(info, "测试 Wi-Fi 外网连接") { controller.checkInternet() }

        val advanced = card(root)
        val expand = text("连接设置  ＋", 16, ink, true).apply { setPadding(0, dp(6), 0, dp(6)); isFocusable = true }
        advanced.addView(expand)
        val settings = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; visibility = View.GONE }
        advanced.addView(settings)
        expand.setOnClickListener {
            settings.visibility = if (settings.visibility == View.GONE) View.VISIBLE else View.GONE
            expand.text = if (settings.visibility == View.VISIBLE) "连接设置  －" else "连接设置  ＋"
        }
        domain = field(settings, "账户域", "@stu，可留空", InputType.TYPE_CLASS_TEXT)
        acId = field(settings, "AC ID", "1", InputType.TYPE_CLASS_NUMBER)
        baseUrl = field(settings, "认证地址", "http://10.6.21.2", InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_URI)
        settings.addView(text("默认使用学校 HTTP 网关；自定义网关使用 HTTPS。", 12, muted))
        action(settings, "打开 Wi-Fi 设置") { startActivity(Intent(Settings.ACTION_WIFI_SETTINGS)) }

        val logCard = card(root)
        logCard.addView(text("运行记录", 16, ink, true))
        logs = text("准备就绪", 12, muted).apply { setPadding(0, dp(12), 0, 0); setTextIsSelectable(true) }
        logCard.addView(logs)
        root.addView(text("Android 1.0  ·  西安交通大学校园网", 11, muted).apply {
            gravity = android.view.Gravity.CENTER; setPadding(0, dp(24), 0, 0)
        })
    }

    private fun readAccount() = Account(username.text.toString().trim(), password.text.toString(),
        domain.text.toString().trim(), acId.text.toString().trim(), baseUrl.text.toString().trim().trimEnd('/'),
        remember.isChecked, auto.isChecked && remember.isChecked)

    private fun suggestCampusWifi() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return
        val wifi = applicationContext.getSystemService(WifiManager::class.java)
        val suggestion = WifiNetworkSuggestion.Builder().setSsid("XJTU-STU").build()
        val result = try {
            wifi.addNetworkSuggestions(listOf(suggestion))
        } catch (_: SecurityException) {
            WifiManager.STATUS_NETWORK_SUGGESTIONS_ERROR_APP_DISALLOWED
        }
        val message = when (result) {
            WifiManager.STATUS_NETWORK_SUGGESTIONS_SUCCESS,
            WifiManager.STATUS_NETWORK_SUGGESTIONS_ERROR_ADD_DUPLICATE ->
                "已提交 XJTU-STU 连接建议，请按系统提示允许。连接由系统选择。"
            WifiManager.STATUS_NETWORK_SUGGESTIONS_ERROR_APP_DISALLOWED ->
                "系统未允许 Wi-Fi 控制，请在应用的特殊访问权限中开启。"
            else -> "添加 Wi-Fi 连接建议失败（$result），请检查系统 Wi-Fi 设置。"
        }
        Toast.makeText(this, message, Toast.LENGTH_LONG).show()
    }

    private fun populate(account: Account) {
        username.setText(account.username)
        password.setText(account.password)
        domain.setText(account.domain)
        acId.setText(account.acId)
        baseUrl.setText(account.baseUrl)
        remember.isChecked = account.remember
        auto.isChecked = account.autoLogin
        auto.isEnabled = account.remember
    }

    private fun render(state: SessionState) {
        status.text = state.status
        badge.text = if (state.busy) "●  正在处理" else if (state.online != null) "●  校园网在线" else "●  等待连接"
        progress.visibility = if (state.busy) View.VISIBLE else View.GONE
        controls.forEach { it.isEnabled = !state.busy }
        auto.isEnabled = !state.busy && remember.isChecked
        logout.isEnabled = !state.busy && state.online != null
        val info = state.online
        details.text = if (info == null) "登录后显示账户、IP、套餐与用量" else {
            val bytes = info.optDouble("sum_bytes", 0.0)
            val seconds = info.optLong("sum_seconds", 0)
            val user = info.optString("user_name", "—")
            val realm = info.optString("domain").removePrefix("@")
            val identity = if (realm.isNotBlank() && '@' !in user) "$user@$realm" else user
            "账户    $identity\nIP        ${info.optString("online_ip", "—")}\n" +
                "套餐    ${info.optString("products_name", "—")}\n" +
                "流量    ${formatBytes(bytes)}\n" +
                "时长    ${seconds / 3600} 小时 ${seconds % 3600 / 60} 分\n" +
                "余额    ${String.format(Locale.CHINA, "%.2f", info.optDouble("user_balance", 0.0))} 元\n" +
                "设备    ${info.optInt("online_device_total", 0)} 台"
        }
        logs.text = state.logs.takeLast(8).joinToString("\n").ifBlank { "准备就绪" }
    }

    private fun field(parent: LinearLayout, label: String, hint: String, input: Int): EditText {
        parent.addView(text(label, 12, muted).apply { setPadding(0, dp(16), 0, dp(4)) })
        return EditText(this).apply {
            this.hint = hint; inputType = input; textSize = 16f; setTextColor(ink)
            setSingleLine(true); setPadding(dp(12), dp(10), dp(12), dp(10))
            background = shape(Color.rgb(246, 248, 246), 10)
            importantForAutofill = View.IMPORTANT_FOR_AUTOFILL_NO_EXCLUDE_DESCENDANTS
            isSaveEnabled = false
            contentDescription = label
            parent.addView(this, LinearLayout.LayoutParams(-1, dp(50)))
            controls.add(this)
        }
    }

    private fun toggle(parent: LinearLayout, label: String, hint: String) = Switch(this).apply {
        text = getString(R.string.setting_label, label, hint); textSize = 12f; setTextColor(ink); setPadding(0, dp(10), 0, dp(10))
        parent.addView(this, LinearLayout.LayoutParams(-1, -2)); controls.add(this)
    }

    private fun action(parent: LinearLayout, label: String, primary: Boolean = false, block: () -> Unit) = Button(this).apply {
        text = label; textSize = 14f; isAllCaps = false
        setTextColor(if (primary) Color.WHITE else green)
        background = shape(if (primary) green else Color.rgb(232, 240, 233), 12)
        parent.addView(this, LinearLayout.LayoutParams(-1, dp(50)).apply { topMargin = dp(10) })
        setOnClickListener { block() }; controls.add(this)
    }

    private fun card(parent: LinearLayout, dark: Boolean = false) = LinearLayout(this).apply {
        orientation = LinearLayout.VERTICAL; setPadding(dp(20), dp(20), dp(20), dp(20))
        background = shape(if (dark) Color.rgb(24, 71, 57) else Color.WHITE, 22)
        parent.addView(this, LinearLayout.LayoutParams(-1, -2).apply { topMargin = dp(18) })
    }

    private fun text(value: String, size: Int, color: Int, bold: Boolean = false) = TextView(this).apply {
        text = value; textSize = size.toFloat(); setTextColor(color)
        if (bold) setTypeface(typeface, Typeface.BOLD)
    }

    private fun shape(color: Int, radius: Int) = GradientDrawable().apply {
        setColor(color); cornerRadius = dp(radius).toFloat()
    }
    private fun dp(value: Int) = (value * resources.displayMetrics.density).toInt()
    private fun formatBytes(bytes: Double): String {
        var value = bytes
        val units = listOf("B", "KB", "MB", "GB", "TB")
        var index = 0
        while (value >= 1024 && index < units.lastIndex) { value /= 1024; index++ }
        return String.format(Locale.CHINA, "%.2f %s", value, units[index])
    }
}
