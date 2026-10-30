package io.github.xjtunetlogin

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.IBinder

/** 界面和后台服务共享同一会话，避免重复认证和注销后的自动重登。 */
object SessionHolder {
    private var session: SessionController? = null
    fun get(context: Context): SessionController = session ?: SessionController(context.applicationContext).also { session = it }
}

class AutoLoginService : Service() {
    private lateinit var controller: SessionController

    override fun onCreate() {
        super.onCreate()
        val notifications = getSystemService(NotificationManager::class.java)
        notifications.createNotificationChannel(NotificationChannel(CHANNEL, "校园网自动登录", NotificationManager.IMPORTANCE_LOW))
        val open = PendingIntent.getActivity(this, 0, Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
        val notification = Notification.Builder(this, CHANNEL)
            .setSmallIcon(R.drawable.ic_launcher)
            .setContentTitle("校园网自动登录已开启")
            .setContentText("连接 Wi-Fi 后自动检测校园网认证状态")
            .setContentIntent(open)
            .setOngoing(true)
            .build()
        startForeground(1, notification)
        controller = SessionHolder.get(this)
        controller.setBackgroundActive(true)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int = START_STICKY

    override fun onDestroy() {
        controller.setBackgroundActive(false)
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    companion object { private const val CHANNEL = "campus_auto_login" }
}
