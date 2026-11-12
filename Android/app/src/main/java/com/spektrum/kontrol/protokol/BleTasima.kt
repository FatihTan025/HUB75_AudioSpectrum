package com.spektrum.kontrol.protokol

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothProfile
import android.content.Context
import android.os.Build
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withTimeout
import java.util.UUID

/** BLE Nordic UART servisi (ESP32 kopru). */
object Nus {
    val SERVIS: UUID = UUID.fromString("6e400001-b5a3-f393-e0a9-e50e24dcca9e")
    val YAZ: UUID = UUID.fromString("6e400002-b5a3-f393-e0a9-e50e24dcca9e")    // telefon -> kart
    val OKU: UUID = UUID.fromString("6e400003-b5a3-f393-e0a9-e50e24dcca9e")    // kart -> telefon (bildirim)
    val CCCD: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
}

@SuppressLint("MissingPermission")     // izinler aktivitede istenir
class BleTasima(private val ctx: Context, private val aygit: BluetoothDevice) : Tasima {
    override val ad = "Bluetooth"
    override var alici: ((ByteArray) -> Unit)? = null
    override var kopma: ((String) -> Unit)? = null

    private var gatt: BluetoothGatt? = null
    private var yazKar: BluetoothGattCharacteristic? = null
    @Volatile private var mtu = 23
    @Volatile private var kapandi = false
    private val hazir = CompletableDeferred<Unit>()
    @Volatile private var yazmaBitti: CompletableDeferred<Unit>? = null
    private val yazKilit = Mutex()

    private val geri = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, durum: Int, yeni: Int) {
            if (yeni == BluetoothProfile.STATE_CONNECTED && durum == BluetoothGatt.GATT_SUCCESS) {
                g.requestMtu(247)
            } else if (yeni == BluetoothProfile.STATE_DISCONNECTED || durum != BluetoothGatt.GATT_SUCCESS) {
                g.close()
                if (!hazir.isCompleted) hazir.completeExceptionally(IllegalStateException("Bluetooth baglanamadi ($durum)"))
                else if (!kapandi) kopma?.invoke("Bluetooth baglantisi koptu")
                yazmaBitti?.complete(Unit)
            }
        }

        override fun onMtuChanged(g: BluetoothGatt, m: Int, durum: Int) {
            if (durum == BluetoothGatt.GATT_SUCCESS) mtu = m
            g.discoverServices()
        }

        override fun onServicesDiscovered(g: BluetoothGatt, durum: Int) {
            val srv = g.getService(Nus.SERVIS)
            val oku = srv?.getCharacteristic(Nus.OKU)
            yazKar = srv?.getCharacteristic(Nus.YAZ)
            if (oku == null || yazKar == null) {
                hazir.completeExceptionally(IllegalStateException("Spektrum servisi bulunamadi"))
                return
            }
            g.setCharacteristicNotification(oku, true)
            val d = oku.getDescriptor(Nus.CCCD)
            if (Build.VERSION.SDK_INT >= 33) {
                g.writeDescriptor(d, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
            } else {
                @Suppress("DEPRECATION")
                d.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                @Suppress("DEPRECATION")
                g.writeDescriptor(d)
            }
        }

        override fun onDescriptorWrite(g: BluetoothGatt, d: BluetoothGattDescriptor, durum: Int) {
            if (durum == BluetoothGatt.GATT_SUCCESS) hazir.complete(Unit)
            else hazir.completeExceptionally(IllegalStateException("Bildirim acilamadi ($durum)"))
        }

        // Android 13+
        override fun onCharacteristicChanged(g: BluetoothGatt, k: BluetoothGattCharacteristic, deger: ByteArray) {
            alici?.invoke(deger)
        }

        // Android 12 ve oncesi
        @Deprecated("Android 12 ve oncesi")
        override fun onCharacteristicChanged(g: BluetoothGatt, k: BluetoothGattCharacteristic) {
            @Suppress("DEPRECATION")
            k.value?.let { alici?.invoke(it.copyOf()) }
        }

        override fun onCharacteristicWrite(g: BluetoothGatt, k: BluetoothGattCharacteristic, durum: Int) {
            yazmaBitti?.complete(Unit)
        }
    }

    override suspend fun ac() {
        gatt = aygit.connectGatt(ctx, false, geri, BluetoothDevice.TRANSPORT_LE)
        try {
            withTimeout(12000) { hazir.await() }
        } catch (e: Exception) {
            kapat()
            throw e
        }
    }

    /** MTU-3'luk parcalar halinde, her parca bitince digeri (Android ayni anda tek yazmaya izin verir). */
    override suspend fun gonder(v: ByteArray) = yazKilit.withLock {
        val g = gatt ?: throw IllegalStateException("bagli degil")
        val k = yazKar ?: throw IllegalStateException("bagli degil")
        val parca = maxOf(20, mtu - 3)
        var i = 0
        while (i < v.size) {
            val p = v.copyOfRange(i, minOf(v.size, i + parca))
            val bitti = CompletableDeferred<Unit>()
            yazmaBitti = bitti
            val tamam = if (Build.VERSION.SDK_INT >= 33) {
                g.writeCharacteristic(k, p, BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE) == 0
            } else {
                k.writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
                @Suppress("DEPRECATION")
                k.value = p
                @Suppress("DEPRECATION")
                g.writeCharacteristic(k)
            }
            if (!tamam) throw IllegalStateException("Bluetooth yazilamadi")
            withTimeout(2000) { bitti.await() }
            i += p.size
        }
    }

    override fun kapat() {
        kapandi = true
        try { gatt?.disconnect(); gatt?.close() } catch (_: Exception) { }
        gatt = null
    }
}
