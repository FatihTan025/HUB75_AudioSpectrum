package com.spektrum.kontrol

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.os.ParcelUuid
import com.spektrum.kontrol.protokol.Cihaz
import com.spektrum.kontrol.protokol.Nus
import com.spektrum.kontrol.protokol.TcpTasima
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.launch

/**
 * Ayni cihaz Bluetooth, ev agi ve kendi aginda ayni adla (SPA12832-XXXX) gorunur; tek satirda birlesir.
 * ipler: ev agi adresi ve/veya 192.168.4.1 (kendi agi).
 */
data class Bulunan(val ad: String, val ble: BluetoothDevice? = null, val ipler: List<String> = emptyList(), val rssi: Int? = null,
                   val ssid: String? = null) {          // ssid: cihazin kendi agi (dogrudan Wi-Fi) gorulduyse
    val dogrudan get() = ssid != null || KENDI_AG_IP in ipler
    val ip get() = ipler.firstOrNull()
}

const val KENDI_AG_IP = "192.168.4.1"
/** Cihazin kendi agi / Bluetooth adi: model kodu + MAC (SPA12832-1A2B); eski kopru surumu Spektrum-1A2B */
private val KENDI_AG = Regex("^(Spektrum|SPA\\d+)-[0-9A-F]{4}$")
fun kendiAgMi(ad: String?) = ad != null && KENDI_AG.matches(ad)

/**
 * Bluetooth (NUS servisi) ve ev agi (mDNS _spektrum._tcp) birlikte taranir.
 * Telefon cihazin kendi agina (SPA12832-XXXX) bagliysa 192.168.4.1 de denenir.
 */
@SuppressLint("MissingPermission")
class Kesif(private val ctx: Context, private val kapsam: CoroutineScope, private val bildir: (Bulunan) -> Unit) {
    private val bt = (ctx.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter
    private val nsd = ctx.getSystemService(Context.NSD_SERVICE) as NsdManager
    private var nsdDinleyici: NsdManager.DiscoveryListener? = null
    private val cozulecek = Channel<NsdServiceInfo>(Channel.UNLIMITED)

    private val bleGeri = object : ScanCallback() {
        override fun onScanResult(tip: Int, r: ScanResult) {
            val ad = r.scanRecord?.deviceName ?: r.device.name ?: return
            // Kopru BLE adi = kendi aginin adi: dogrudan Wi-Fi yolu da kullanilabilir
            bildir(Bulunan(ad, ble = r.device, rssi = r.rssi, ssid = if (kendiAgMi(ad)) ad else null))
        }
    }

    val bluetoothAcik get() = bt?.isEnabled == true

    fun baslat() {
        // Bluetooth
        if (bluetoothAcik) {
            val filtre = ScanFilter.Builder().setServiceUuid(ParcelUuid(Nus.SERVIS)).build()
            val ayar = ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build()
            try { bt.bluetoothLeScanner?.startScan(listOf(filtre), ayar, bleGeri) } catch (_: Exception) { }
        }
        // Ev agi: mDNS. Android ayni anda tek cozumleme yapar -> kuyruk.
        kapsam.launch {
            for (s in cozulecek) cozumle(s)
        }
        val d = object : NsdManager.DiscoveryListener {
            override fun onServiceFound(s: NsdServiceInfo) { cozulecek.trySend(s) }
            override fun onServiceLost(s: NsdServiceInfo) {}
            override fun onDiscoveryStarted(t: String) {}
            override fun onDiscoveryStopped(t: String) {}
            override fun onStartDiscoveryFailed(t: String, k: Int) {}
            override fun onStopDiscoveryFailed(t: String, k: Int) {}
        }
        nsdDinleyici = d
        try { nsd.discoverServices("_spektrum._tcp", NsdManager.PROTOCOL_DNS_SD, d) } catch (_: Exception) { }
        // Cihazin kendi agi, Wi-Fi taramasindan (Android 7-11: konum izni BLE icin zaten var)
        try {
            @Suppress("DEPRECATION")
            val wm = ctx.applicationContext.getSystemService(Context.WIFI_SERVICE) as android.net.wifi.WifiManager
            @Suppress("DEPRECATION") wm.startScan()
            wm.scanResults?.forEach { r ->
                @Suppress("DEPRECATION") val s = r.SSID
                if (s != null && kendiAgMi(s)) bildir(Bulunan(s, ssid = s))
            }
        } catch (_: Exception) { }
        // Telefon cihazin agina elle baglanmissa (internetsiz Wi-Fi): adini kopruden sor.
        // Yalniz bu durumda yoklanir; ev agindayken PC aracinin baglantisi dusurulmez.
        if (AgSecici.internetsizWifi(ctx)) kapsam.launch(Dispatchers.IO) {
            val ag = AgSecici.agBul(ctx, KENDI_AG_IP, kesin = true) ?: return@launch
            val t = TcpTasima(KENDI_AG_IP, kapsam = kapsam, ag = ag)
            val ad = try { t.ac(); Cihaz(t).kopruBilgi(1500).ad } catch (_: Exception) { null } finally { t.kapat() }
            if (ad != null) bildir(Bulunan(ad, ipler = listOf(KENDI_AG_IP)))
        }
    }

    private suspend fun cozumle(s: NsdServiceInfo) {
        val bitti = kotlinx.coroutines.CompletableDeferred<Unit>()
        @Suppress("DEPRECATION")
        nsd.resolveService(s, object : NsdManager.ResolveListener {
            override fun onResolveFailed(s: NsdServiceInfo, k: Int) { bitti.complete(Unit) }
            override fun onServiceResolved(s: NsdServiceInfo) {
                @Suppress("DEPRECATION")
                val ip = s.host?.hostAddress
                if (ip != null) bildir(Bulunan(s.serviceName, ipler = listOf(ip)))
                bitti.complete(Unit)
            }
        })
        kotlinx.coroutines.withTimeoutOrNull(3000) { bitti.await() }
    }

    fun durdur() {
        try { if (bluetoothAcik) bt.bluetoothLeScanner?.stopScan(bleGeri) } catch (_: Exception) { }
        nsdDinleyici?.let { try { nsd.stopServiceDiscovery(it) } catch (_: Exception) { } }
        nsdDinleyici = null
        cozulecek.close()
    }
}
