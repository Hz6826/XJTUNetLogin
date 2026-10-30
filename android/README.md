# 仙交网络助手 · Android

使用 Kotlin 编写，支持 Android 8.0 及以上版本。

## 使用

1. 打开应用，输入学号或工号及校园网密码。默认账户域为 `@stu`，AC ID 为 `1`，认证地址为 `http://10.6.21.2`；可在「连接设置」中修改。
2. 如需后台自动认证，勾选「记住账户」和「自动登录」，点击「保存账户与设置」。也可在连接 Wi-Fi 后点击「登录校园网」手动登录。
3. Android 10 及以上可点击「启用校园 Wi-Fi 自动连接」，按系统提示允许后，系统会将开放网络 `XJTU-STU` 纳入自动连接候选。Android 8 和 9 可在系统 Wi-Fi 设置中连接并启用自动连接。
4. 在线后可查看账户、IP、套餐、流量、时长、余额和设备数，也可刷新、注销或测试 Wi-Fi 外网连接。

「记住账户」使用 Android Keystore 和 AES-GCM 加密存储账户配置。关闭后立即清除已保存的数据。勾选「自动登录」后点击「保存账户与设置」或登录，即可保存选项。

启用并保存自动登录后，后台服务在 Wi-Fi 接入时检查认证状态，并每 30 秒刷新；发现校园网未认证时尝试登录。手动注销后暂停自动登录，重新连接 Wi-Fi 后恢复。关闭自动登录并保存设置会停止后台服务。系统对服务通知的显示受通知权限影响；强制停止应用后需重新打开应用。认证和外网检测均通过 Wi-Fi 发送。

## 构建

在 Android Studio 中打开本目录，使用 **JDK 17、21 或 25**，安装 **Android SDK Platform 36** 和 **Build-Tools 36.1.0**。通过 Android Studio 配置 SDK，或在本目录的 `local.properties` 中设置 `sdk.dir`。

Windows：

```powershell
.\gradlew.bat :app:assembleDebug :app:testDebugUnitTest :app:lintDebug
```

macOS / Linux：

```sh
sh gradlew :app:assembleDebug :app:testDebugUnitTest :app:lintDebug
```

安装包输出：`app/build/outputs/apk/debug/app-debug.apk`。正式发布时通过 Android Studio 的 Generate Signed App Bundle / APK 使用自己的签名密钥。

协议测试涵盖加密固定向量、JSON/JSONP、两次 challenge、认证拒绝、在线确认重试和注销时使用服务器在线 IP。校园网实机验收需在校内 Wi-Fi 上完成登录、刷新、注销及重连自动登录。

连接模拟器或 Android 设备后，可运行 `:app:connectedDebugAndroidTest`，验证系统密钥存储、账户清除、页面重建和输入校验。
