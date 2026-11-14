package com.spektrum.kontrol.protokol

/* Spektrum Analizor haberlesme protokolu (firmware Protocol.c, PC protokol.py ile ayni).
   Cerceve: A5 | LEN(2) | SEQ | CMD | PAYLOAD(LEN) | CRC16(2)   (kucuk uclu)
   Yanit:   CMD | 0x80, ayni SEQ, yukun ilk bayti durum. */

object Kom {
    const val INFO = 0x01
    const val SET_NAME = 0x02
    const val PING = 0x03
    const val MODE_LIST = 0x10
    const val MODE = 0x11
    const val GET_PARAM = 0x20
    const val SET_PARAM = 0x21
    const val GET_ALL = 0x22
    const val PARAM_DESCRIBE = 0x23
    const val SAVE = 0x30
    const val FACTORY_RESET = 0x32
    const val TIME = 0x40
    const val POWER = 0x51
    const val EVENT = 0x60
    const val STATUS = 0x70
    const val BRIDGE = 0x7F
}

object Olay { const val ACILIS = 1; const val MOD = 2; const val PARAM = 3; const val GUC = 4 }

object Tur { const val BOOL = 0; const val INT = 1; const val ENUM = 2; const val RENK = 3; const val PALET = 4; const val TON = 5; const val BLOK = 6 }

object Bayrak { const val MOD = 0x01; const val SALT_OKUNUR = 0x02; const val GIZLI = 0x08; const val GELISMIS = 0x10 }

object AyarId { const val PARLAKLIK = 0x0001; const val OTO_SN = 0x0002; const val FREQ_LO = 0x0003; const val FREQ_HI = 0x0004
    const val FAV = 0x0006; const val MUZIK_GEC = 0x0007; const val SAAT_YAZI = 0x0304; const val SAAT_YER = 0x0305; const val SAAT_RGB = 0x030C
    const val SAAT_ISIK = 0x030D; const val SAAT_ALT = 0x030F; const val SAAT_ISI = 0x0310
    const val SAAT_SN = 0x0311 }

const val MOD_GENEL = 0xFF
const val SOF = 0xA5
const val MAX_YUK = 256

/** Desteklenen en yuksek protokol surumu; cihaz daha yenisini bildirirse uyari. */
const val PROTOKOL_SURUM = 1

val DURUM = mapOf(0 to "tamam", 1 to "bilinmeyen komut", 2 to "bilinmeyen parametre", 3 to "aralik disi",
    4 to "CRC hatasi", 5 to "mesgul", 6 to "desteklenmiyor", 7 to "guncelleme hatasi")

fun crc16(v: ByteArray, bas: Int = 0, n: Int = v.size - bas, ilk: Int = 0xFFFF): Int {
    var c = ilk
    for (i in bas until bas + n) {
        c = c xor ((v[i].toInt() and 0xFF) shl 8)
        repeat(8) { c = if (c and 0x8000 != 0) ((c shl 1) xor 0x1021) and 0xFFFF else (c shl 1) and 0xFFFF }
    }
    return c
}

fun cerceve(seq: Int, cmd: Int, yuk: ByteArray = ByteArray(0)): ByteArray {
    val n = yuk.size
    val f = ByteArray(n + 7)
    f[0] = SOF.toByte(); f[1] = n.toByte(); f[2] = (n shr 8).toByte(); f[3] = seq.toByte(); f[4] = cmd.toByte()
    yuk.copyInto(f, 5)
    val c = crc16(f, 1, n + 4)
    f[5 + n] = c.toByte(); f[6 + n] = (c shr 8).toByte()
    return f
}

class Paket(val seq: Int, val cmd: Int, val yuk: ByteArray)

/** Bayt akisindan gecerli cerceveleri cikarir; bozuk olani atlar. */
class Ayristirici {
    private val t = ArrayList<Byte>()

    fun besle(v: ByteArray): List<Paket> {
        v.forEach { t.add(it) }
        val cikis = ArrayList<Paket>()
        while (true) {
            val s = t.indexOf(SOF.toByte())
            if (s < 0) { t.clear(); break }
            if (s > 0) t.subList(0, s).clear()
            if (t.size < 7) break
            val n = u(1) or (u(2) shl 8)
            if (n > MAX_YUK) { t.removeAt(0); continue }
            if (t.size < n + 7) break
            val b = ByteArray(n + 7) { t[it] }
            val c = (b[5 + n].toInt() and 0xFF) or ((b[6 + n].toInt() and 0xFF) shl 8)
            if (crc16(b, 1, n + 4) != c) { t.removeAt(0); continue }
            cikis.add(Paket(b[3].toInt() and 0xFF, b[4].toInt() and 0xFF, b.copyOfRange(5, 5 + n)))
            t.subList(0, n + 7).clear()
        }
        return cikis
    }

    private fun u(i: Int) = t[i].toInt() and 0xFF
}

/** Kucuk uclu okuyucu. */
class Okur(private val b: ByteArray, var i: Int = 0) {
    fun u8() = b[i++].toInt() and 0xFF
    fun s8() = b[i++].toInt()
    fun u16() = u8() or (u8() shl 8)
    fun s16() = u16().toShort().toInt()
    fun s32() = u16() or (u16() shl 16)
    fun bayt(n: Int) = b.copyOfRange(i, i + n).also { i += n }
    fun metin(n: Int) = String(bayt(n), Charsets.UTF_8)
    fun kalan() = b.size - i
    /** 0 ile biten metin (yoksa sona kadar). */
    fun metin0(): String {
        var j = i
        while (j < b.size && b[j] != 0.toByte()) j++
        val s = String(b, i, j - i, Charsets.UTF_8)
        i = minOf(j + 1, b.size)
        return s
    }
}

class Yazar {
    private val b = java.io.ByteArrayOutputStream()
    fun u8(v: Int) = apply { b.write(v and 0xFF) }
    fun u16(v: Int) = apply { u8(v); u8(v shr 8) }
    fun s32(v: Int) = apply { u16(v); u16(v ushr 16) }
    fun bayt(v: ByteArray) = apply { b.write(v) }
    fun al(): ByteArray = b.toByteArray()
}

// --- Veri ---------------------------------------------------------------------

data class Bilgi(val protokol: Int, val fw: String, val genislik: Int, val yukseklik: Int, val bitplane: Int,
                 val modSayisi: Int, val paramSayisi: Int, val mod: Int, val guc: Boolean, val ad: String)

/** tur: 0 muzikli, 1 muziksiz (sekmeyi belirler). aile: grup (0 Klasik, 1 Modern, 2 Animasyon, 3 Ortam), sadece siralama/PC */
data class Mod(val no: Int, val tur: Int, val aile: Int, val ad: String)

data class Tanim(val sira: Int, val toplam: Int, val id: Int, val tur: Int, val bayrak: Int, val maske: Int,
                 val min: Int, val max: Int, val adim: Int, val varsayilan: Int,
                 val gosterId: Int, val gosterAlt: Int, val gosterUst: Int,
                 val ad: String, val grup: String, val secenek: String) {
    val modBasina get() = bayrak and Bayrak.MOD != 0
    val gizli get() = bayrak and Bayrak.GIZLI != 0
    val gelismis get() = bayrak and Bayrak.GELISMIS != 0
    val saltOkunur get() = bayrak and Bayrak.SALT_OKUNUR != 0
    /** ENUM secenekleri */
    val secenekler: List<String> get() = if (secenek.isEmpty()) emptyList() else secenek.split('|')
    /** BLOK: "Ad:RRGGBB|..." ya da gecisli "Ad:RRGGBB,RRGGBB" (renk ailesi) */
    val bloklar: List<Pair<String, List<Int>>> get() = secenekler.map {
        val k = it.lastIndexOf(':')
        if (k < 0) it to listOf(0x808080)
        else it.substring(0, k) to it.substring(k + 1).split(',').map { h -> h.trim().toIntOrNull(16) ?: 0x808080 }
    }
}

data class WifiAg(val ad: String, val rssi: Int, val sifreli: Boolean)
data class WifiDurum(val durum: Int, val rssi: Int, val ip: String, val ad: String)   // durum: 0 kayit yok, 1 baglaniyor, 2 bagli
data class KopruBilgi(val surum: String, val mac: String, val ad: String)

object Coz {
    fun bilgi(d: ByteArray) = Okur(d).run {
        val p = u8(); val fw = u16()
        Bilgi(p, "${fw shr 8}.${fw and 0xFF}", u16(), u16(), u8(), u8(), u16(), u8(), u8() != 0, metin(kalan()))
    }

    /** MODE_LIST cevabi -> (toplam, bas, liste) */
    fun modlar(d: ByteArray): Triple<Int, Int, List<Mod>> = Okur(d).run {
        val toplam = u8(); val bas = u8(); val adet = u8()
        Triple(toplam, bas, List(adet) { val no = u8(); val tur = u8(); val aile = u8(); Mod(no, tur, aile, metin(u8())) })
    }

    fun tanim(d: ByteArray) = Okur(d).run {
        Tanim(u16(), u16(), u16(), u8(), u8(), s32(), s32(), s32(), s32(), s32(), u16(), s16(), s16(),
            metin0(), metin0(), metin0())
    }

    /** GET_ALL -> id -> deger */
    fun hepsi(d: ByteArray): Map<Int, Int> = Okur(d).run {
        u8(); val adet = u8()
        (0 until adet).associate { u16() to s32() }
    }

    fun wifiAglar(d: ByteArray): List<WifiAg> = Okur(d).run {
        val adet = if (kalan() > 0) u8() else 0
        List(adet) { val r = s8(); val s = u8() != 0; WifiAg(metin(u8()), r, s) }.sortedByDescending { it.rssi }
    }

    fun wifiDurum(d: ByteArray) = Okur(d).run {
        val du = u8(); val r = s8(); val ip = (0 until 4).joinToString(".") { u8().toString() }
        WifiDurum(du, r, ip, metin(u8()))
    }

    fun kopruBilgi(d: ByteArray) = Okur(d).run {
        val s = u16(); val mac = bayt(6).joinToString(":") { "%02X".format(it.toInt() and 0xFF) }
        KopruBilgi("${s shr 8}.${s and 0xFF}", mac, metin(u8()))
    }
}
