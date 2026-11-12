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
import com.spektrum.kontrol.protokol.KopruBilgi
import com.spektrum.kontrol.protokol.MOD_GENEL
import com.spektrum.kontrol.protokol.Mod
import com.spektrum.kontrol.protokol.Okur
import com.spektrum.kontrol.protokol.Olay
import com.spektrum.kontrol.protokol.PROTOKOL_SURUM
import com.spektrum.kontrol.protokol.Tanim
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
    val cihazSaat = mutableStateOf<IntArray?>(null)        // sa dk sn gun ay yil (okundugu an)

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

    /** Kullanicinin verdigi ad (cihazin yayin adi -> ad), listede gostermek icin telefonda tutulur. */
    fun gorunenAd(yayinAdi: String) = tercih.getString("ad_$yayinAdi", null)

    // =========================================================================
    private fun konumAcik(): Boolean {
        val lm = getApplication<Application>().getSystemService(Context.LOCATION_SERVICE) as LocationManager
        return if (Build.VERSION.SDK_INT >= 28) lm.isLocationEnabled
        else lm.isProviderEnabled(LocationManager.GPS_PROVIDER) || lm.isProviderEnabled(LocationManager.NETWORK_PROVIDER)
    }

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
            bulunanlar[i] = e.copy(ble = b.ble ?: e.ble, ip = b.ip ?: e.ip, rssi = b.rssi ?: e.rssi)
        }
    }

    // =========================================================================
    /** Wi-Fi varsa o (hizli), yoksa Bluetooth. */
    fun baglan(b: Bulunan) {
        if (baglaniyor.value != null) return
        kesif?.durdur(); kesif = null; taraniyor.value = false
        baglaniyor.value = b.ad
        mesaj.value = null
        viewModelScope.launch {
            val tasima = when {
                b.ip != null -> TcpTasima(b.ip, kapsam = viewModelScope)
                b.ble != null -> BleTasima(getApplication<Application>(), b.ble)
                else -> null
            }
            if (tasima == null) { baglaniyor.value = null; return@launch }
            val c = Cihaz(tasima)
            try {
                c.ac()
                tasima.kopma = { m -> viewModelScope.launch(Dispatchers.Main) { koptu(m) } }
                c.olay = { tur, veri -> viewModelScope.launch(Dispatchers.Main) { olay(tur, veri) } }
                val bl = c.bilgi()
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
                sekme.value = if (ml.firstOrNull { it.no == bl.mod }?.tur == 1) Sekme.ORTAM else Sekme.MUZIK
                tercih.edit().putString("ad_${b.ad}", bl.ad).apply()
                cihazSaat.value = try { c.saatOku() } catch (_: Exception) { null }   // Saat otomatik gonderilmez
                cihaz.value = c
                modDegerYukle(bl.mod)
                kopru.value = try { c.kopruBilgi() } catch (_: Exception) { null }   // UART'tan bagliysa kopru yok
            } catch (e: Exception) {
                c.kapat()
                mesaj.value = "Bağlanılamadı: ${e.message}"
            } finally {
                baglaniyor.value = null
            }
        }
    }

    fun birak() {
        cihaz.value?.kapat()
        cihaz.value = null
        taramaBaslat()
    }

    private fun koptu(m: String) {
        if (cihaz.value == null) return
        cihaz.value = null
        mesaj.value = m
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
            try { cihaz.value?.mod(no) } catch (e: Exception) { mesaj.value = e.message }
        }
    }

    private val yazIs = HashMap<Pair<Int, Int>, Job>()

    /** Kaydirma cubuklari icin: son deger gecerli, 60 ms'de bir gonderilir. */
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
            } catch (e: Exception) { mesaj.value = e.message }
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
        viewModelScope.launch { try { guc.value = c.guc(!guc.value) } catch (e: Exception) { mesaj.value = e.message } }
    }

    val otoAcik get() = (genel[AyarId.OTO_SN] ?: 0) > 0

    /** Oto dugmesi: kapaliysa son sure (yoksa 60 sn) ile ac. */
    fun otoDegistir() {
        val t = tanimBul(AyarId.OTO_SN) ?: return
        val sure = if (otoAcik) 0 else tercih.getInt("oto_sn", 60).coerceIn(maxOf(t.min, t.adim), t.max)
        if (otoAcik) tercih.edit().putInt("oto_sn", genel[AyarId.OTO_SN] ?: 60).apply()
        ayarYaz(t, sure, MOD_GENEL)
    }

    fun saatOku() {
        val c = cihaz.value ?: return
        viewModelScope.launch { cihazSaat.value = try { c.saatOku() } catch (_: Exception) { null } }
    }

    /** Kullanici "Saati ayarla" ile sectigi saat ve tarihi gonderir */
    fun saatAyarla(z: java.util.Calendar) {
        val c = cihaz.value ?: return
        viewModelScope.launch {
            try {
                c.saatAyarla(z.get(java.util.Calendar.HOUR_OF_DAY), z.get(java.util.Calendar.MINUTE), z.get(java.util.Calendar.SECOND),
                    z.get(java.util.Calendar.DAY_OF_MONTH), z.get(java.util.Calendar.MONTH) + 1, z.get(java.util.Calendar.YEAR))
                cihazSaat.value = c.saatOku()
                mesaj.value = "Saat ayarlandı"
            } catch (e: Exception) { mesaj.value = e.message }
        }
    }

    fun adVer(ad: String) {
        val c = cihaz.value ?: return
        viewModelScope.launch {
            try { c.adVer(ad); bilgi.value = c.bilgi() } catch (e: Exception) { mesaj.value = e.message }
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
            } catch (e: Exception) { mesaj.value = e.message }
        }
    }

    fun wifiTara() {
        val c = cihaz.value ?: return
        viewModelScope.launch {
            try { aglar.clear(); aglar.addAll(c.wifiTara()) } catch (e: Exception) { mesaj.value = "Taranamadı: ${e.message}" }
        }
    }

    fun wifiDurumSor() {
        val c = cihaz.value ?: return
        viewModelScope.launch { wifi.value = try { c.wifiDurum() } catch (_: Exception) { null } }
    }

    fun wifiAyarla(ad: String, sifre: String) {
        val c = cihaz.value ?: return
        viewModelScope.launch {
            try { c.wifiAyarla(ad, sifre); delay(6000); wifiDurumSor() } catch (e: Exception) { mesaj.value = e.message }
        }
    }

    override fun onCleared() {
        kesif?.durdur()
        cihaz.value?.kapat()
    }
}
