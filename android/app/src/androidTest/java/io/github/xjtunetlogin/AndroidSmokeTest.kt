package io.github.xjtunetlogin

import android.os.SystemClock
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class AndroidSmokeTest {
    private val context get() = InstrumentationRegistry.getInstrumentation().targetContext

    @Before fun clearStoredAccount() { AccountStore(context).save(Account()) }

    @Test fun keystoreRoundTripAndForget() {
        val store = AccountStore(context)
        val account = Account("smoke-test-user", "P@ss\"\\中文", remember = true, autoLogin = true)
        store.save(account)
        assertEquals(account, store.load())
        val prefs = context.getSharedPreferences("account", 0)
        val payload = prefs.getString("ciphertext", "")!!
        assertFalse(payload.contains(account.username))
        assertFalse(payload.contains(account.password))
        store.save(account.copy(remember = false, autoLogin = false, domain = "@staff"))
        assertFalse(prefs.contains("ciphertext"))
        assertEquals("", store.load().username)
        assertEquals("", store.load().password)
        assertEquals("@staff", store.load().domain)
    }

    @Test fun draftSurvivesActivityRecreation() {
        ActivityScenario.launch(MainActivity::class.java).use { scenario ->
            awaitReady(scenario)
            scenario.onActivity { activity ->
                val views = descendants(activity.window.decorView)
                views.filterIsInstance<EditText>().first { it.contentDescription == "账号" }.setText("draft-user")
                views.filterIsInstance<EditText>().first { it.contentDescription == "密码" }.setText("draft-secret")
            }
            scenario.recreate()
            scenario.onActivity { activity ->
                val views = descendants(activity.window.decorView)
                assertEquals("draft-user", views.filterIsInstance<EditText>().first { it.contentDescription == "账号" }.text.toString())
                assertEquals("draft-secret", views.filterIsInstance<EditText>().first { it.contentDescription == "密码" }.text.toString())
            }
        }
    }

    @Test fun emptyAccountShowsValidationBeforeNetworkRequest() {
        ActivityScenario.launch(MainActivity::class.java).use { scenario ->
            awaitReady(scenario)
            scenario.onActivity { activity ->
                val views = descendants(activity.window.decorView)
                views.filterIsInstance<Button>().first { it.text == "登录校园网" }.performClick()
                assertTrue(descendants(activity.window.decorView).filterIsInstance<TextView>()
                    .any { it.text == "请输入校园网账号" })
            }
        }
    }

    private fun awaitReady(scenario: ActivityScenario<MainActivity>) {
        val deadline = SystemClock.uptimeMillis() + 20000
        while (SystemClock.uptimeMillis() < deadline) {
            var ready = false
            scenario.onActivity { activity ->
                ready = descendants(activity.window.decorView).filterIsInstance<EditText>()
                    .first { it.contentDescription == "账号" }.isEnabled
            }
            if (ready) return
            SystemClock.sleep(50)
        }
        fail("Activity did not become ready")
    }

    private fun descendants(view: View): List<View> = listOf(view) +
        if (view is ViewGroup) (0 until view.childCount).flatMap { descendants(view.getChildAt(it)) } else emptyList()
}
