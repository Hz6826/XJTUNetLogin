package io.github.xjtunetlogin

import org.junit.Assert.*
import org.junit.Test
import java.io.IOException

class SrunClientTest {
    @Test fun captiveRedirectMustMatchConfiguredGateway() {
        val gateway = "http://10.6.21.2"
        assertTrue(WifiTransport.isConfiguredPortalRedirect(302, "http://10.6.21.2/srun_portal_pc", gateway))
        assertFalse(WifiTransport.isConfiguredPortalRedirect(204, "http://10.6.21.2/", gateway))
        assertFalse(WifiTransport.isConfiguredPortalRedirect(302, "http://other.example/", gateway))
        assertFalse(WifiTransport.isConfiguredPortalRedirect(302, "http://10.6.21.2:8080/", gateway))
        assertFalse(WifiTransport.isConfiguredPortalRedirect(302, "/login", gateway))
    }
    private val account = Account("20260001", "test-password")
    private val token = "0123456789abcdef0123456789abcdef"
    private val infoVector = "{SRBX1}kJZ+dbb4ZNLKXE3Yb6b7RiR+LVAsPcmULgbr+FGBo/fyjynzkW3nelKVIiyu2nFbYq/yQnJaqiqRXMTKN1rHtEqmuQmmlAjJZrKHJidJ86q5BNx7l294F7Varq6inmcFaqX8rxJr7zoFBeC5"

    private class Fake(vararg bodies: String) : PortalTransport {
        val replies = ArrayDeque(bodies.toList())
        val calls = mutableListOf<Pair<String, Map<String, String>>>()
        override fun get(path: String, params: Map<String, String>): String {
            calls.add(path to params)
            return replies.removeFirst()
        }
    }

    @Test fun cryptoMatchesIndependentUnsignedReference() {
        // 固定向量来自独立的 32 位无符号参考运算，锁定门户特有的加法分组。
        assertEquals("dad1d07f6292b0ec", SrunCrypto.xencode("abc".toByteArray(), token.toByteArray())
            .joinToString("") { "%02x".format(it.toInt() and 255) })
        assertEquals(infoVector, SrunCrypto.info(account.fullUsername, account.password, "10.1.2.3", "1", token))
        assertTrue(SrunCrypto.xencode(byteArrayOf(), token.toByteArray()).isEmpty())
    }

    @Test fun hmacMatchesRfc2202() {
        assertEquals("750c783e6ab0b503eaa86e310a5db738", SrunCrypto.hmacMd5("what do ya want for nothing?", "Jefe"))
    }

    @Test fun checksumMatchesIndependentSha1Vector() {
        assertEquals("4b25b2520d849191a190745c209e04a102f1ed8c", SrunCrypto.checksum(
            "Jefe", "user@stu", "750c783e6ab0b503eaa86e310a5db738", "1", "10.1.2.3", "encoded"))
    }

    @Test fun acceptsJsonAndJsonp() {
        assertEquals("ok", SrunClient.parse("\uFEFF \n{\"error\":\"ok\"}").getString("error"))
        assertEquals("ok", SrunClient.parse("jQuery_12(\n{\"error\":\"ok\"}\n);").getString("error"))
        assertThrows(IllegalArgumentException::class.java) { SrunClient.parse("<html>portal</html>") }
    }

    @Test fun loginResolvesIpThenFetchesFreshTokenAndVerifiesOnline() {
        val fake = Fake("""{"client_ip":"10.1.2.3","challenge":"discard"}""",
            """{"challenge":"$token"}""", """{"error":"ok"}""",
            """{"error":"not_online_error"}""", """{"error":"ok","online_ip":"10.1.2.3"}""")
        val result = SrunClient(fake) {}.login(account)
        assertEquals("ok", result.getString("error"))
        assertEquals("", fake.calls[0].second["ip"])
        assertEquals("10.1.2.3", fake.calls[1].second["ip"])
        val auth = fake.calls[2].second
        assertEquals(account.fullUsername, auth["username"])
        assertEquals(infoVector, auth["info"])
        assertEquals("Android", auth["os"])
        assertEquals("10.1.2.3", auth["ip"])
        assertTrue(auth.getValue("password").startsWith("{MD5}"))
        assertEquals(5, fake.calls.size)
    }

    @Test fun rejectedAuthenticationStopsBeforePolling() {
        val fake = Fake("""{"client_ip":"10.1.2.3"}""", """{"challenge":"$token"}""",
            """{"error":"E2553","error_msg":"密码错误"}""")
        val error = assertThrows(IllegalArgumentException::class.java) { SrunClient(fake) {}.login(account) }
        assertEquals("密码错误", error.message)
        assertEquals(3, fake.calls.size)
    }

    @Test fun logoutUsesOnlineIpAndServerIdentityWithoutChangingSavedDomain() {
        val fake = Fake("""{"error":"ok","user_name":"other","domain":"staff","online_ip":"10.9.8.7","client_ip":"10.1.2.3"}""",
            """{"error":"ok"}""", """{"error":"not_online_error"}""")
        SrunClient(fake) {}.logout(account)
        assertEquals("other@staff", fake.calls[1].second["username"])
        assertEquals("10.9.8.7", fake.calls[1].second["ip"])
        assertEquals("@stu", account.domain)
    }

    @Test fun alreadyOfflineDoesNotSendLogout() {
        val fake = Fake("""{"error":"not_online_error"}""")
        SrunClient(fake) {}.logout(Account())
        assertEquals(1, fake.calls.size)
    }

    @Test fun loginHasBoundedVerificationRetries() {
        val fake = Fake("""{"client_ip":"10.1.2.3"}""", """{"challenge":"$token"}""",
            """{"error":"ok"}""", *Array(3) { """{"error":"not_online_error"}""" })
        assertThrows(IllegalStateException::class.java) { SrunClient(fake) {}.login(account) }
        assertEquals(6, fake.calls.size)
    }

    @Test fun verificationRecoversFromTemporaryNetworkFailure() {
        var polls = 0
        val fake = Fake("""{"client_ip":"10.1.2.3"}""", """{"challenge":"$token"}""", """{"error":"ok"}""")
        val transport = PortalTransport { path, params ->
            if (path.endsWith("rad_user_info")) {
                if (++polls == 1) throw IOException("timeout")
                """{"error":"ok"}"""
            } else fake.get(path, params)
        }
        assertEquals("ok", SrunClient(transport) {}.login(account).getString("error"))
        assertEquals(2, polls)
    }

    @Test fun validatesAccountAndGateway() {
        assertEquals("20260001@stu", account.copy(username = "20260001@stu").fullUsername)
        assertEquals("20260001", account.copy(domain = "").fullUsername)
        account.validate()
        Account.validatePortal("https://portal.example.edu")
        for (address in listOf("http://example.com", "file:///tmp/a", "https://user:pass@example.com", "https://example.com/path")) {
            assertThrows(IllegalArgumentException::class.java) { Account.validatePortal(address) }
        }
        assertThrows(IllegalArgumentException::class.java) { account.copy(password = "").validate() }
        assertThrows(IllegalArgumentException::class.java) { account.copy(acId = "abc").validate() }
    }
}
