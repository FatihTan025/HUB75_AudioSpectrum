package com.spektrum.kontrol.protokol

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.InputStream
import java.io.OutputStream
import java.net.InetSocketAddress
import java.net.Socket

/** Tasima katmani: BLE ya da TCP. Gelen baytlar alici'ya, kopma kopma'ya bildirilir. */
interface Tasima {
    val ad: String
    var alici: ((ByteArray) -> Unit)?
    var kopma: ((String) -> Unit)?
    suspend fun ac()
    suspend fun gonder(v: ByteArray)
    fun kapat()
}

/** ESP32 kopru: TCP 5000 (cihazin kendi agi 192.168.4.1 ya da ev agi). */
class TcpTasima(private val adres: String, private val port: Int = 5000, private val kapsam: CoroutineScope) : Tasima {
    override val ad = "Wi-Fi"
    override var alici: ((ByteArray) -> Unit)? = null
    override var kopma: ((String) -> Unit)? = null
    private var soket: Socket? = null
    private var cikis: OutputStream? = null
    private var okuma: Job? = null
    @Volatile private var kapandi = false

    override suspend fun ac() = withContext(Dispatchers.IO) {
        val s = Socket()
        s.tcpNoDelay = true
        s.connect(InetSocketAddress(adres, port), 4000)
        soket = s
        cikis = s.getOutputStream()
        val giris: InputStream = s.getInputStream()
        okuma = kapsam.launch(Dispatchers.IO) {
            val t = ByteArray(1024)
            try {
                while (true) {
                    val n = giris.read(t)
                    if (n < 0) break
                    if (n > 0) alici?.invoke(t.copyOf(n))
                }
            } catch (_: Exception) {
            }
            if (!kapandi) kopma?.invoke("Wi-Fi baglantisi koptu")
        }
    }

    override suspend fun gonder(v: ByteArray) = withContext(Dispatchers.IO) {
        val c = cikis ?: throw IllegalStateException("bagli degil")
        c.write(v)
        c.flush()
    }

    override fun kapat() {
        kapandi = true
        try { soket?.close() } catch (_: Exception) { }
        okuma?.cancel()
    }
}
