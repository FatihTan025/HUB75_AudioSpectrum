package com.spektrum.kontrol.protokol

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withTimeoutOrNull
import java.util.concurrent.ConcurrentHashMap

class HataYanit(val kod: Int) : Exception("kart: ${DURUM[kod] ?: kod.toString()}")

/** Komutlar sirayla gider, cevap beklenir. Olaylar (SEQ 0) olay geri cagrisina gider. */
class Cihaz(val tasima: Tasima) {
    var olay: ((tur: Int, veri: ByteArray) -> Unit)? = null

    private val ayr = Ayristirici()
    private val kilit = Mutex()
    private var seq = 0
    private val bekleyen = ConcurrentHashMap<Int, CompletableDeferred<Paket>>()

    init {
        tasima.alici = { v -> synchronized(ayr) { ayr.besle(v) }.forEach(::gelen) }
    }

    private fun gelen(p: Paket) {
        if (p.seq == 0 && p.cmd == Kom.EVENT) {
            if (p.yuk.isNotEmpty()) olay?.invoke(p.yuk[0].toInt() and 0xFF, p.yuk.copyOfRange(1, p.yuk.size))
            return
        }
        if (p.cmd and 0x80 != 0) bekleyen[p.seq]?.complete(p)
    }

    suspend fun ac() = tasima.ac()
    fun kapat() = tasima.kapat()

    /** Cevabin durum baytindan sonrasini dondurur. izin: hata sayilmayan durumlar. */
    suspend fun komut(cmd: Int, yuk: ByteArray = ByteArray(0), izin: Set<Int> = setOf(0),
                      zamanMs: Long = 400, deneme: Int = 3): Pair<Int, ByteArray> = kilit.withLock {
        seq = seq % 255 + 1
        val s = seq
        try {
            repeat(deneme) {
                val d = CompletableDeferred<Paket>()
                bekleyen[s] = d
                tasima.gonder(cerceve(s, cmd, yuk))
                val p = withTimeoutOrNull(zamanMs) { d.await() }
                if (p != null && (p.cmd and 0x7F) == cmd && p.yuk.isNotEmpty()) {
                    val st = p.yuk[0].toInt() and 0xFF
                    if (st !in izin) throw HataYanit(st)
                    return@withLock st to p.yuk.copyOfRange(1, p.yuk.size)
                }
            }
            throw java.util.concurrent.TimeoutException("kart cevap vermiyor (komut %02X)".format(cmd))
        } finally {
            bekleyen.remove(s)
        }
    }

    // --- Komutlar ---------------------------------------------------------------
    suspend fun bilgi() = Coz.bilgi(komut(Kom.INFO).second)

    suspend fun modlar(): List<Mod> {
        val liste = ArrayList<Mod>()
        var bas = 0
        while (true) {
            val (toplam, b, parca) = Coz.modlar(komut(Kom.MODE_LIST, byteArrayOf(bas.toByte())).second)
            liste += parca
            bas = b + parca.size
            if (bas >= toplam || parca.isEmpty()) return liste
        }
    }

    suspend fun mod(yeni: Int? = null): Int =
        komut(Kom.MODE, if (yeni == null) ByteArray(0) else byteArrayOf(yeni.toByte())).second[0].toInt() and 0xFF

    suspend fun tanim(sira: Int) = Coz.tanim(komut(Kom.PARAM_DESCRIBE, Yazar().u16(sira).al()).second)

    suspend fun tanimlar(): List<Tanim> {
        val ilk = tanim(0)
        return listOf(ilk) + (1 until ilk.toplam).map { tanim(it) }
    }

    suspend fun hepsi(mod: Int = MOD_GENEL) = Coz.hepsi(komut(Kom.GET_ALL, byteArrayOf(mod.toByte())).second)

    /** Uygulanan (gerekirse sinirlanmis) degeri dondurur. */
    suspend fun yaz(id: Int, deger: Int, mod: Int = MOD_GENEL): Int {
        val (_, d) = komut(Kom.SET_PARAM, Yazar().u8(mod).u16(id).s32(deger).al(), izin = setOf(0, 3))
        return Okur(d, 3).s32()
    }

    suspend fun adVer(ad: String) {
        val b = ad.toByteArray(Charsets.UTF_8)
        komut(Kom.SET_NAME, b.copyOf(minOf(16, b.size)))
    }

    suspend fun guc(acik: Boolean? = null): Boolean =
        komut(Kom.POWER, if (acik == null) ByteArray(0) else byteArrayOf(if (acik) 1 else 0)).second[0].toInt() != 0

    suspend fun fabrika() { komut(Kom.FACTORY_RESET) }

    /** Cihaz saatini ayarlar (eski firmware "desteklenmiyor" der; cagiran yok sayar). */
    suspend fun saatAyarla(sa: Int, dk: Int, sn: Int) {
        komut(Kom.TIME, byteArrayOf(sa.toByte(), dk.toByte(), sn.toByte()))
    }

    // --- ESP32 kopru (0x7F, STM32'ye gitmez) -----------------------------------
    suspend fun kopruBilgi() = Coz.kopruBilgi(komut(Kom.BRIDGE, byteArrayOf(0), zamanMs = 1000).second)
    suspend fun wifiTara() = Coz.wifiAglar(komut(Kom.BRIDGE, byteArrayOf(1), zamanMs = 8000, deneme = 1).second)
    suspend fun wifiDurum() = Coz.wifiDurum(komut(Kom.BRIDGE, byteArrayOf(3), zamanMs = 1000).second)

    suspend fun wifiAyarla(ad: String, sifre: String) {
        val a = ad.toByteArray(Charsets.UTF_8).let { it.copyOf(minOf(32, it.size)) }
        val s = sifre.toByteArray(Charsets.UTF_8).let { it.copyOf(minOf(64, it.size)) }
        komut(Kom.BRIDGE, Yazar().u8(2).u8(a.size).bayt(a).u8(s.size).bayt(s).al(), zamanMs = 1000)
    }
}
