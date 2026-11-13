package com.spektrum.kontrol

import android.annotation.SuppressLint
import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.net.wifi.WifiConfiguration
import android.net.wifi.WifiManager
import android.net.wifi.WifiNetworkSpecifier
import android.os.Build
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.delay
import kotlinx.coroutines.withTimeoutOrNull
import java.net.Inet4Address
import java.net.InetAddress

/** Cihazin kendi aginin sifresi (ESP32 AP_SIFRE ile ayni) */
const val KENDI_AG_SIFRE = "spektrum123"

/**
 * Ag secimi: her baglanti yolu soketini acikca kendi aginda acar, Android'in
 * "hangi agdan gideyim" tahminine birakmaz.
 *  - Ev agi: telefonun bagli oldugu, hedef adresi iceren Wi-Fi.
 *  - Dogrudan (Spektrum-XXXX): Android 10+ "yalniz yerel" istek (telefonun interneti
 *    mobil veride / ev Wi-Fi'inda kalir); Android 7-9 uygulama aga kendisi baglanir ve
 *    soketi o aga baglar (internetsiz Wi-Fi'da trafigin mobil veriye kacmasi olmaz).
 * Basarisiz olursa null doner; cagiran eski davranisa (duz soket) duser.
 */
object WifiAg {
    private var istek: ConnectivityManager.NetworkCallback? = null

    private fun cm(ctx: Context) = ctx.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager

    /** Hedef adresin alt aginda olan Wi-Fi agi (kesin = false ise yoksa herhangi bir Wi-Fi) */
    fun agBul(ctx: Context, ip: String, kesin: Boolean = false): Network? = try {
        val c = cm(ctx)
        val hedef = InetAddress.getByName(ip)
        @Suppress("DEPRECATION")
        val wifi = c.allNetworks.filter { c.getNetworkCapabilities(it)?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) == true }
        wifi.firstOrNull { n ->
            c.getLinkProperties(n)?.linkAddresses?.any { la ->
                val a = la.address
                a is Inet4Address && hedef is Inet4Address && ayniAg(a.address, hedef.address, la.prefixLength)
            } == true
        } ?: if (kesin) null else wifi.firstOrNull()
    } catch (_: Exception) { null }

    private fun ayniAg(a: ByteArray, b: ByteArray, onek: Int): Boolean {
        for (i in 0 until onek) {
            val m = 0x80 shr (i % 8)
            if ((a[i / 8].toInt() and m) != (b[i / 8].toInt() and m)) return false
        }
        return true
    }

    /** Su anki Wi-Fi internetsiz mi (dogrulanmamis)? Cihazin kendi agina elle baglanilmis olabilir. */
    fun internetsizWifi(ctx: Context): Boolean = try {
        val c = cm(ctx)
        @Suppress("DEPRECATION")
        c.allNetworks.any {
            val k = c.getNetworkCapabilities(it)
            k != null && k.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) &&
                !k.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED)
        }
    } catch (_: Exception) { false }

    /** Cihazin kendi agina (ssid) baglanir ve o agi dondurur. Kullanici onayi (Android 10+) icin 30 s bekler. */
    @SuppressLint("MissingPermission")
    suspend fun kendiAg(ctx: Context, ssid: String): Network? {
        uygCtx = ctx.applicationContext
        birak()
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return kendiAgEski(ctx, ssid)
        val c = cm(ctx)
        val istekB = NetworkRequest.Builder()
            .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
            .removeCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
            .setNetworkSpecifier(WifiNetworkSpecifier.Builder().setSsid(ssid).setWpa2Passphrase(KENDI_AG_SIFRE).build())
        val sonuc = CompletableDeferred<Network?>()
        val geri = object : ConnectivityManager.NetworkCallback() {
            override fun onAvailable(n: Network) { sonuc.complete(n) }
            override fun onUnavailable() { sonuc.complete(null) }
        }
        return try {
            c.requestNetwork(istekB.build(), geri)
            istek = geri
            val n = withTimeoutOrNull(30_000) { sonuc.await() }
            if (n == null) birak()
            n
        } catch (_: Exception) { birak(); null }
    }

    /** Android 7-9: aga uygulama baglanir (sistem Wi-Fi'i bu aga gecer, internet mobil veride kalir);
        192.168.4.x adresli Wi-Fi agi gorunene kadar 15 s bekler. */
    private suspend fun kendiAgEski(ctx: Context, ssid: String): Network? {
        agBul(ctx, KENDI_AG_IP, kesin = true)?.let { return it }		// Zaten bagli
        try {
            @Suppress("DEPRECATION")
            val wm = ctx.applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
            @Suppress("DEPRECATION")
            val yap = WifiConfiguration().apply { SSID = "\"$ssid\""; preSharedKey = "\"$KENDI_AG_SIFRE\"" }
            @Suppress("DEPRECATION", "MissingPermission")
            val id = wm.configuredNetworks?.firstOrNull { it.SSID == "\"$ssid\"" }?.networkId ?: wm.addNetwork(yap)
            @Suppress("DEPRECATION")
            if (id < 0 || !wm.enableNetwork(id, true)) return null
        } catch (_: Exception) { return null }
        repeat(30) {
            agBul(ctx, KENDI_AG_IP, kesin = true)?.let { return it }
            delay(500)
        }
        return null
    }

    /** Dogrudan baglanti istegini birakir (Android 10+: telefon eski agina doner) */
    fun birak() {
        val g = istek ?: return
        istek = null
        try { uygCtx?.let { cm(it).unregisterNetworkCallback(g) } } catch (_: Exception) { }
    }

    private var uygCtx: Context? = null
}
