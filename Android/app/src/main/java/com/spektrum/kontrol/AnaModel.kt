package com.spektrum.kontrol

import android.app.Application
import android.content.Context
import android.location.LocationManager
import android.os.Build
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.spektrum.kontrol.protokol.AyarId
import com.spektrum.kontrol.protokol.BleTasima
import com.spektrum.kontrol.protokol.Bilgi
import com.spektrum.kontrol.protokol.Cihaz
import com.spektrum.kontrol.protokol.HataYanit
import com.spektrum.kontrol.protokol.KopruBilgi
import com.spektrum.kontrol.protokol.MOD_GENEL
import com.spektrum.kontrol.protokol.Mod
import com.spektrum.kontrol.protokol.Okur
import com.spektrum.kontrol.protokol.Olay
import com.spektrum.kontrol.protokol.PROTOKOL_SURUM
import com.spektrum.kontrol.protokol.Tanim
import com.spektrum.kontrol.protokol.Tasima
import com.spektrum.kontrol.protokol.TcpTasima
import com.spektrum.kontrol.protokol.WifiAg
import com.spektrum.kontrol.protokol.WifiDurum
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

enum class Sekme { MUZIK, ORTAM, AYARLAR }

/** Uygulamanin tum durumu. Ekranlar sadece bunu okur; liste ve ayarlar her zaman cihazdan gelir. */
class AnaModel(uyg: Application) : AndroidViewModel(uyg) {
    private val tercih = uyg.getSharedPreferences("spektrum", Context.MODE_PRIVATE)

    // --- Kesif ---
    val bulunanlar = mutableStateListOf<Bulunan>()
    val taraniyor = mutableStateOf(false)
    /** Android 6-11: Bluetooth taramasi icin telefonun Konum ayari acik olmali, yoksa liste sessizce bos kalir. */
    val konumKapali = mutableStateOf(false)
    private var kesif: Kesif? = null

    // --- Baglanti ---
    val baglaniyor = mutableStateOf<String?>(null)     // baglanilan cihazin adi
    val mesaj = mutableStateOf<String?>(null)
    val cihaz = mutableStateOf<Cihaz?>(null)
    val bilgi = mutableStateOf<Bilgi?>(null)
    val surumUyari = mutableStateOf(false)
    val kopru = mutableStateOf<KopruBilgi?>(null)
    /** Kopru bilgisi alinamadiysa sebebi (Ayarlar > Baglanti'da gosterilir) */
    val kopruHata = mutableStateOf<String?>(null)

    // --- Icerik (cihazdan) ---
    val modlar = mutableStateListOf<Mod>()
    val tanimlar = mutableStateListOf<Tanim>()
    val genel = mutableStateMapOf<Int, Int>()                  // genel ayarlar: id -> deger
    val modDeger = mutableStateMapOf<Int, Map<Int, Int>>()     // mod -> (id -> deger)
    val aktifMod = mutableStateOf(0)
    val guc = mutableStateOf(true)
    val sekme = mutableStateOf(Sekme.MUZIK)

    // --- Ev agi ---
    val aglar = mutableStateListOf<WifiAg>()
    val wifi = mutableStateOf<WifiDurum?>(null)

    /** Hata mesaji; yeni dokunusla iptal edilen eski gonderim (CancellationException) hata degildir */
    private fun hata(e: Exception, on: String = "") {
        if (e is kotlinx.coroutines.CancellationException) throw e
        mesaj.value = on + (e.message ?: e.javaClass.simpleName)
    }

    /** Kullanicinin verdigi ad (cihazin yayin adi -> ad), listede gostermek icin telefonda tutulur. */
    fun gorunenAd(yayinAdi: String) = tercih.getString("ad_$yayinAdi", null)

    // =========================================================================
    private fun konumAcik(): Boolean {
        val lm = getApplication<Application>().getSystemService(Context.LOCATION_SERVICE) as LocationManager
        return if (Build.VERSION.SDK_INT >= 28) lm.isLocationEnabled
        else lm.isProviderEnabled(LocationManager.GPS_PROVIDER) || lm.isProviderEnabled(LocationManager.NETWORK_PROVIDER)
    }

    /**
     * Otomatik yeniden baglanma: baglanti koparsa (ya da uygulama acilinca) son cihaz
     * taramada gorulur gorulmez kendiliginden baglanilir. Kullanici "Birak" derse yapilmaz.
     */
    private var otoHedef: String? = tercih.getString("son_cihaz", null)
    private var otoSekme: Sekme? = null

    fun taramaBaslat() {
        konumKapali.value = Build.VERSION.SDK_INT in 23..30 && !konumAcik()
        kesif?.durdur()
        bulunanlar.clear()
        val k = Kesif(getApplication<Application>(), viewModelScope) { b ->
            viewModelScope.launch(Dispatchers.Main) { ekle(b) }
        }
        kesif = k
        k.baslat()
        taraniyor.value = true
        if (!k.bluetoothAcik) mesaj.value = "Bluetooth kapalı: sadece ev ağındaki cihazlar görünür."
        viewModelScope.launch { delay(15000); if (kesif === k) { k.durdur(); taraniyor.value = false } }
    }

    private fun ekle(b: Bulunan) {
        val i = bulunanlar.indexOfFirst { it.ad == b.ad }
        if (i < 0) bulunanlar.add(b)
        else {
            val e = bulunanlar[i]
            bulunanlar[i] = e.copy(ble = b.ble ?: e.ble, ipler = (e.ipler + b.ipler).distinct(), rssi = b.rssi ?: e.rssi,
                ssid = b.ssid ?: e.ssid)
        }
        val h = otoHedef
        if (h != null && b.ad == h && cihaz.value == null && baglaniyor.value == null) {
            otoHedef = null                         // tek deneme; yeniden kopunca tekrar kurulur
            viewModelScope.launch { delay(800); bulunanlar.firstOrNull { it.ad == h }?.let { baglan(it) } }   // diger yollar da gelsin
        }
    }

    // =========================================================================
    /**
     * Yollar sirayla denenir, ilk cevap veren kalir. Her Wi-Fi soketi kendi aginda acilir (WifiAg):
     *  1) ev agi  2) cihazin kendi agi, telefon zaten bagliysa  3) Bluetooth
     *  4) cihazin kendi agi, uygulama baglanir (Android 10+ "yalniz yerel": internet kesilmez; sistem bir kez sorar)
     * Ag bulunamazsa duz sokete duser (eski davranis).
     */
    fun baglan(b: Bulunan) {
        if (baglaniyor.value != null) return
        kesif?.durdur(); kesif = null; taraniyor.value = false
        baglaniyor.value = b.ad
        mesaj.value = null
        viewModelScope.launch {
            val app = getApplication<Application>()
            AgSecici.birak()
            val yollar = mutableListOf<suspend () -> Tasima?>()
            for (ip in b.ipler.filter { it != KENDI_AG_IP })
                yollar.add { TcpTasima(ip, kapsam = viewModelScope, ag = AgSecici.agBul(app, ip)) }
            if (KENDI_AG_IP in b.ipler)
                yollar.add { TcpTasima(KENDI_AG_IP, kapsam = viewModelScope, ag = AgSecici.agBul(app, KENDI_AG_IP, kesin = true)) }
            b.ble?.let { d -> yollar.add { BleTasima(app, d) } }
            b.ssid?.let { ad -> yollar.add { AgSecici.kendiAg(app, ad)?.let { n -> TcpTasima(KENDI_AG_IP, kapsam = viewModelScope, ag = n) } } }
            var bulunanC: Cihaz? = null
            var bulunanB: Bilgi? = null
            var sonHata: Exception? = null
            for (yol in yollar) {
                val ts = try { yol() } catch (e: Exception) { if (e is kotlinx.coroutines.CancellationException) throw e; sonHata = e; null } ?: continue
                val a = Cihaz(ts)
                try { a.ac(); bulunanB = a.bilgi(zamanMs = 1500); bulunanC = a; break }
                catch (e: Exception) { a.kapat(); if (e is kotlinx.coroutines.CancellationException) throw e; sonHata = e }
            }
            val c = bulunanC
            val bl = bulunanB
            if (c == null || bl == null) {
                hata(sonHata ?: IllegalStateException("ulaşılabilir yol yok"), "Bağlanılamadı: ")
                baglaniyor.value = null
                return@launch
            }
            try {
                c.tasima.kopma = { m -> viewModelScope.launch(Dispatchers.Main) { koptu(m) } }
                c.olay = { tur, veri -> viewModelScope.launch(Dispatchers.Main) { olay(tur, veri) } }
                val ml = c.modlar()
                val tl = c.tanimlar()
                val gn = c.hepsi(MOD_GENEL)
                bilgi.value = bl
                surumUyari.value = bl.protokol > PROTOKOL_SURUM
                modlar.clear(); modlar.addAll(ml)
                tanimlar.clear(); tanimlar.addAll(tl)
                genel.clear(); genel.putAll(gn)
                modDeger.clear()
                aktifMod.value = bl.mod
                guc.value = bl.guc
                sekme.value = otoSekme ?: if (ml.firstOrNull { it.no == bl.mod }?.tur == 1) Sekme.ORTAM else Sekme.MUZIK
                otoSekme = null
                tercih.edit().putString("ad_${b.ad}", bl.ad).putString("son_cihaz", b.ad).apply()
                cihaz.value = c
                modDegerYukle(bl.mod)
                kopruSorgu(c, 1500)
            } catch (e: Exception) {
                c.kapat()
                hata(e, "Bağlanılamadı: ")
            } finally {
                baglaniyor.value = null
            }
        }
    }

    fun birak() {
        cihaz.value?.kapat()
        cihaz.value = null
        AgSecici.birak()                                  // Dogrudan Wi-Fi istegi birakilir: telefon eski agina doner
        otoHedef = null
        tercih.edit().remove("son_cihaz").apply()
        taramaBaslat()
    }

    private fun koptu(m: String) {
        if (cihaz.value == null) return
        cihaz.value = null
        otoHedef = tercih.getString("son_cihaz", null)
        otoSekme = sekme.value
        mesaj.value = if (otoHedef != null) "$m · yeniden bağlanılıyor" else m
        taramaBaslat()
    }

    private fun olay(tur: Int, veri: ByteArray) {
        val o = Okur(veri)
        when (tur) {
            Olay.MOD -> if (veri.isNotEmpty()) { aktifMod.value = o.u8(); modDegerYukle(aktifMod.value) }
            Olay.GUC -> if (veri.isNotEmpty()) guc.value = o.u8() != 0
            Olay.PARAM -> if (veri.size >= 7) {
                val m = o.u8(); val id = o.u16(); val d = o.s32()
                if (m == MOD_GENEL) genel[id] = d else modDeger[m]?.let { modDeger[m] = it + (id to d) }
            }
        }
    }

    // =========================================================================
    fun modDegerYukle(mod: Int) {
        val c = cihaz.value ?: return
        viewModelScope.launch {
            try { modDeger[mod] = c.hepsi(mod) } catch (_: Exception) { }
        }
    }

    private var modIs: Job? = null

    /** Kaydirirken her kartta komut gitmesin: durunca gonder. */
    fun modSec(no: Int) {
        if (no == aktifMod.value) return
        aktifMod.value = no
        if (!modDeger.containsKey(no)) modDegerYukle(no)
        modIs?.cancel()
        modIs = viewModelScope.launch {
            delay(150)
            try { cihaz.value?.mod(no) } catch (e: Exception) { hata(e) }
        }
    }

    private val yazIs = HashMap<Pair<Int, Int>, Job>()

    /** Kaydirma cubuklari icin: son deger gecerli, 60 ms'de bir gonderilir. */
    /** Favoriler (kalp): kartta tek gizli ayar, bit n = mod n. Otomatik gecis yalniz bunlarda doner. */
    fun favori(no: Int) = ((genel[AyarId.FAV] ?: 0) shr no) and 1 != 0
    fun favoriDegistir(no: Int) {
        val t = tanimBul(AyarId.FAV) ?: return
        ayarYaz(t, (genel[AyarId.FAV] ?: 0) xor (1 shl no), MOD_GENEL)
    }

    fun ayarYaz(t: Tanim, deger: Int, mod: Int) {
        val m = if (t.modBasina) mod else MOD_GENEL
        if (m == MOD_GENEL) genel[t.id] = deger else modDeger[m] = (modDeger[m] ?: emptyMap()) + (t.id to deger)
        val anahtar = m to t.id
        yazIs[anahtar]?.cancel()
        yazIs[anahtar] = viewModelScope.launch {
            delay(60)
            try {
                val uygulanan = cihaz.value?.yaz(t.id, deger, m) ?: return@launch
                if (uygulanan != deger) {
                    if (m == MOD_GENEL) genel[t.id] = uygulanan else modDeger[m] = (modDeger[m] ?: emptyMap()) + (t.id to uygulanan)
                }
            } catch (e: Exception) { hata(e) }
        }
    }

    fun tanimBul(id: Int) = tanimlar.firstOrNull { it.id == id }

    /** Bu modda kullaniciya gosterilecek ayarlar: cihazin o modda bildirdikleri, gizli/gelismis olmayanlar. */
    fun modAyarlari(mod: Int): List<Tanim> {
        val d = modDeger[mod] ?: return emptyList()
        return tanimlar.filter { it.modBasina && !it.gizli && !it.gelismis && d.containsKey(it.id) && gorunur(it, mod) }
    }

    /** Ayarlar sekmesindeki genel ayarlar (parlaklik ve oto haric; onlarin yeri ayri). */
    fun genelAyarlar() = tanimlar.filter {
        !it.modBasina && !it.gizli && !it.gelismis && it.id != AyarId.PARLAKLIK && it.id != AyarId.OTO_SN &&
            genel.containsKey(it.id) && gorunur(it, MOD_GENEL)
    }

    fun deger(t: Tanim, mod: Int): Int =
        (if (t.modBasina) modDeger[mod]?.get(t.id) else genel[t.id]) ?: t.varsayilan

    /** GosterId: baska bir ayarin degeri [alt, ust] arasindaysa gorunur. */
    private fun gorunur(t: Tanim, mod: Int): Boolean {
        if (t.gosterId == 0) return true
        val d = modDeger[mod]?.get(t.gosterId) ?: genel[t.gosterId] ?: return true
        return d in t.gosterAlt..t.gosterUst
    }

    // --- Genel islemler ---------------------------------------------------------
    fun gucDegistir() {
        val c = cihaz.value ?: return
        viewModelScope.launch { try { guc.value = c.guc(!guc.value) } catch (e: Exception) { hata(e) } }
    }

    val otoAcik get() = (genel[AyarId.OTO_SN] ?: 0) > 0

    /** Oto dugmesi: kapaliysa son sure (yoksa 60 sn) ile ac. */
    fun otoDegistir() {
        val t = tanimBul(AyarId.OTO_SN) ?: return
        val sure = if (otoAcik) 0 else tercih.getInt("oto_sn", 60).coerceIn(maxOf(t.min, t.adim), t.max)
        if (otoAcik) tercih.edit().putInt("oto_sn", genel[AyarId.OTO_SN] ?: 60).apply()
        ayarYaz(t, sure, MOD_GENEL)
    }

    /** "Saati ayarla": telefonun saati ve tarihi karta gider (otomatik gonderilmez) */
    fun saatAyarla() {
        val c = cihaz.value ?: return
        viewModelScope.launch {
            try {
                val z = java.util.Calendar.getInstance()
                c.saatAyarla(z.get(java.util.Calendar.HOUR_OF_DAY), z.get(java.util.Calendar.MINUTE), z.get(java.util.Calendar.SECOND),
                    z.get(java.util.Calendar.DAY_OF_MONTH), z.get(java.util.Calendar.MONTH) + 1, z.get(java.util.Calendar.YEAR))
                mesaj.value = "Saat ayarlandı"
            } catch (e: Exception) { hata(e) }
        }
    }

    fun adVer(ad: String) {
        val c = cihaz.value ?: return
        viewModelScope.launch {
            try { c.adVer(ad); bilgi.value = c.bilgi() } catch (e: Exception) { hata(e) }
        }
    }

    fun fabrika() {
        val c = cihaz.value ?: return
        viewModelScope.launch {
            try {
                c.fabrika()
                genel.clear(); genel.putAll(c.hepsi(MOD_GENEL))
                modDeger.clear(); modDegerYukle(aktifMod.value)
                mesaj.value = "Fabrika ayarlarına dönüldü."
            } catch (e: Exception) { hata(e) }
        }
    }

    fun wifiTara() {
        val c = cihaz.value ?: return
        viewModelScope.launch {
            try { aglar.clear(); aglar.addAll(c.wifiTara()) } catch (e: Exception) { hata(e, "Taranamadı: ") }
        }
    }

    /** ESP32 kopru bilgisini sorar; alinamazsa sebebini kopruHata'ya yazar */
    private suspend fun kopruSorgu(c: Cihaz, zamanMs: Long): Boolean {
        kopru.value = try { c.kopruBilgi(zamanMs) } catch (e: Exception) {
            if (e is kotlinx.coroutines.CancellationException) throw e
            kopruHata.value = when (e) {
                is java.util.concurrent.TimeoutException -> "Köprü cevap vermedi"
                is HataYanit -> "Köprü hata verdi (${e.message})"
                else -> e.message ?: "Köprüye ulaşılamadı"
            }
            null
        }
        if (kopru.value != null) kopruHata.value = null
        return kopru.value != null
    }

    /** Ev agi satirina dokununca: kopru bilinmiyorsa uzun bekleyerek yeniden sor */
    fun kopruYenidenSor(sonra: () -> Unit) {
        val c = cihaz.value ?: return
        viewModelScope.launch { if (kopruSorgu(c, 3000)) { wifiDurumSor(); sonra() } else hata(IllegalStateException(kopruHata.value ?: "Köprü yok")) }
    }

    fun wifiDurumSor() {
        val c = cihaz.value ?: return
        viewModelScope.launch { wifi.value = try { c.wifiDurum() } catch (_: Exception) { null } }
    }

    fun wifiAyarla(ad: String, sifre: String) {
        val c = cihaz.value ?: return
        viewModelScope.launch {
            try { c.wifiAyarla(ad, sifre); delay(6000); wifiDurumSor() } catch (e: Exception) { hata(e) }
        }
    }

    override fun onCleared() {
        kesif?.durdur()
        cihaz.value?.kapat()
    }
}
