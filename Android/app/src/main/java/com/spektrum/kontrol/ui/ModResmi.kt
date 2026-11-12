package com.spektrum.kontrol.ui

import android.graphics.BitmapFactory
import androidx.compose.foundation.Canvas
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.FilterQuality
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntSize
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import java.util.Locale

/* Mod resimleri: assets/modlar/<sade ad>.png -- 128 genis, 32*N yuksek kare seridi.
   Firmware'in kendi cizim kodundan uretilir (Tools/Onizleme/uret.sh). Resim moda ADIYLA baglanir;
   sira degisse de kaymaz. Resmi olmayan (yeni) mod icin genel bir gorunum cizilir. */

private const val PW = 128
private const val PH = 32
private const val KARE_MS = 64L          // ~15 kare/sn (uretimle ayni)

private val TR_HARF = mapOf('Ç' to 'C', 'Ğ' to 'G', 'İ' to 'I', 'I' to 'I', 'Ö' to 'O', 'Ş' to 'S', 'Ü' to 'U',
    'ç' to 'c', 'ğ' to 'g', 'ı' to 'i', 'ö' to 'o', 'ş' to 's', 'ü' to 'u')

/** png_uret.py ile ayni kural: Turkce harf -> Latin, kucuk harf, harf/rakam disi -> _ */
fun sadeAd(ad: String): String =
    ad.map { TR_HARF[it] ?: it }.joinToString("").lowercase(Locale.ROOT)
        .replace(Regex("[^a-z0-9]+"), "_").trim('_')

private val onbellek = HashMap<String, ImageBitmap?>()

@Composable
fun ModResmi(ad: String, oynat: Boolean, modifier: Modifier = Modifier) {
    val ctx = LocalContext.current
    val dosya = remember(ad) { "modlar/${sadeAd(ad)}.png" }
    val resim by produceState(initialValue = onbellek[dosya], dosya) {
        if (!onbellek.containsKey(dosya)) {
            onbellek[dosya] = withContext(Dispatchers.IO) {
                try { ctx.assets.open(dosya).use { BitmapFactory.decodeStream(it)?.asImageBitmap() } } catch (_: Exception) { null }
            }
        }
        value = onbellek[dosya]
    }
    val r = resim
    val kareSayisi = if (r != null) maxOf(1, r.height / PH) else 1
    var kare by remember { mutableIntStateOf(0) }
    LaunchedEffect(oynat, kareSayisi) {
        while (oynat) { delay(KARE_MS); kare = (kare + 1) % kareSayisi }
    }

    Canvas(modifier) {
        drawRect(Color.Black)
        val olc = minOf(size.width / PW, size.height / PH)
        val w = PW * olc
        val h = PH * olc
        val x0 = (size.width - w) / 2f
        val y0 = (size.height - h) / 2f
        if (r != null) {
            drawImage(
                r, srcOffset = IntOffset(0, (kare % kareSayisi) * PH), srcSize = IntSize(PW, PH),
                dstOffset = IntOffset(x0.toInt(), y0.toInt()), dstSize = IntSize(w.toInt(), h.toInt()),
                filterQuality = FilterQuality.None
            )
        } else {
            // Genel gorunum: soluk spektrum cubuklari
            for (i in 0 until 32) {
                val yuk = (0.25f + 0.6f * kotlin.math.abs(kotlin.math.sin(i * 0.55f))) * h
                drawRect(Pasif.copy(alpha = 0.35f), Offset(x0 + i * 4 * olc, y0 + h - yuk), Size(3 * olc, yuk))
            }
        }
        // LED aralari: her pikselin etrafinda ince siyah cizgi -> panel gorunumu
        val b = olc * 0.28f
        for (i in 0..PW) drawRect(Color.Black, Offset(x0 + i * olc - b / 2f, y0), Size(b, h))
        for (j in 0..PH) drawRect(Color.Black, Offset(x0, y0 + j * olc - b / 2f), Size(w, b))
    }
}
