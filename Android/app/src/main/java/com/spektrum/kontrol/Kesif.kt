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
import com.spektrum.kontrol.protokol.Nus
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.launch
import java.net.InetSocketAddress
import java.net.Socket

/** Ayni cihaz Bluetooth ve Wi-Fi'de ayni adla (Spektrum-XXXX) gorunur; tek satirda birlesir. */
data class Bulunan(val ad: String, val ble: BluetoothDevice? = null, val ip: String? = null, val rssi: Int? = null)

const val KENDI_AG_IP = "192.168.4.1"
const val KENDI_AG_AD = "Cihazın kendi ağı"

/**
 * Bluetooth (NUS servisi) ve ev agi (mDNS _spektrum._tcp) birlikte taranir.
 * Telefon cihazin kendi agina (Spektrum-XXXX) bagliysa 192.168.4.1 de denenir.
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
            bildir(Bulunan(ad, ble = r.device, rssi = r.rssi))
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
        // Cihazin kendi agi
        kapsam.launch(Dispatchers.IO) {
            try {
                Socket().use { it.connect(InetSocketAddress(KENDI_AG_IP, 5000), 600) }
                bildir(Bulunan(KENDI_AG_AD, ip = KENDI_AG_IP))
            } catch (_: Exception) { }
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
                if (ip != null) bildir(Bulunan(s.serviceName, ip = ip))
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
