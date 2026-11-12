package com.spektrum.kontrol

import android.content.Context
import android.os.Build
import java.io.File
import java.io.PrintWriter
import java.io.StringWriter
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Uygulama cokerse hatayi dosyaya yazar; sonraki acilista kullaniciya gosterilir,
 * kopyalanip gonderilebilir (Android Studio / logcat gerekmeden).
 */
object HataKaydi {
    private const val DOSYA = "hata_raporu.txt"

    fun kur(ctx: Context) {
        val uyg = ctx.applicationContext
        val onceki = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { is_, hata ->
            try {
                val iz = StringWriter().also { hata.printStackTrace(PrintWriter(it)) }.toString()
                val surum = try { uyg.packageManager.getPackageInfo(uyg.packageName, 0).versionName } catch (_: Exception) { "?" }
                val zaman = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(Date())
                File(uyg.filesDir, DOSYA).writeText(
                    "Spektrum $surum · $zaman\n" +
                    "Telefon: ${Build.MANUFACTURER} ${Build.MODEL} · Android ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})\n" +
                    "Is parcacigi: ${is_.name}\n\n" + iz.take(12000)
                )
            } catch (_: Exception) { }
            onceki?.uncaughtException(is_, hata)
        }
    }

    fun oku(ctx: Context): String? = File(ctx.filesDir, DOSYA).takeIf { it.exists() }?.readText()

    fun sil(ctx: Context) { File(ctx.filesDir, DOSYA).delete() }
}
