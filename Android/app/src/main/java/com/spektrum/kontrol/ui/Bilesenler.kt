package com.spektrum.kontrol.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.gestures.drag
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.spektrum.kontrol.protokol.Tanim
import kotlin.math.roundToInt
import com.spektrum.kontrol.protokol.Tur

// --- Renkler (LED Chord'dan esinli: mor zemin, sari vurgu) -------------------------
val Mor = Color(0xFF5B3FD1)
val MorKart = Color(0xFF6E54DC)
val MorKoyu = Color(0xFF4A31B8)
val MorSecili = Color(0xFF7F67E6)
val Sari = Color(0xFFFFD23F)
val SariYazi = Color(0xFF2A1A00)
val Soluk = Color(0xFFE4DCFF)
val Pasif = Color(0xFFB9AEEB)

fun rgbRenk(v: Int) = Color(0xFF000000.toInt() or (v and 0xFFFFFF))
fun tonRenk(h: Float) = Color(android.graphics.Color.HSVToColor(floatArrayOf(h.coerceIn(0f, 359.9f), 1f, 1f)))
fun rgbTon(v: Int): Float { val hsv = FloatArray(3); android.graphics.Color.colorToHSV(0xFF000000.toInt() or v, hsv); return hsv[0] }
fun tonRgb(h: Float) = android.graphics.Color.HSVToColor(floatArrayOf(h, 1f, 1f)) and 0xFFFFFF

private val tonFircasi = Brush.horizontalGradient(listOf(0f, 60f, 120f, 180f, 240f, 300f, 359f).map { tonRenk(it) })

@Composable
fun Baslik(yazi: String) = Text(yazi, color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.SemiBold)

/**
 * Cihazdan gelen bir ayar tanimini turune gore cizer:
 * ton/renk = ton cubugu, blok = renk bloklari, enum = secmeli dugmeler, bool = anahtar, int = kaydirma cubugu.
 */
@Composable
fun AyarSatiri(t: Tanim, deger: Int, degisti: (Int) -> Unit) {
    when (t.tur) {
        Tur.BOOL -> Row(Modifier.fillMaxWidth().height(48.dp), verticalAlignment = Alignment.CenterVertically) {
            Baslik(t.ad); Spacer(Modifier.weight(1f))
            Switch(checked = deger != 0, onCheckedChange = { degisti(if (it) 1 else 0) }, enabled = !t.saltOkunur,
                colors = SwitchDefaults.colors(checkedTrackColor = Sari, checkedThumbColor = Color.White,
                    uncheckedTrackColor = MorKoyu, uncheckedThumbColor = Pasif, uncheckedBorderColor = MorKoyu))
        }
        Tur.ENUM -> Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Baslik(t.ad)
            Secmeli(t.secenekler, deger - t.min) { degisti(t.min + it) }
        }
        Tur.BLOK -> Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Baslik(t.ad)
            Bloklar(t.bloklar, deger - t.min) { degisti(t.min + it) }
        }
        Tur.TON -> Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Baslik(t.ad)
            TonCubugu(deger.toFloat() * 360f / (t.max + 1).coerceAtLeast(1)) { degisti((it * (t.max + 1) / 360f).toInt().coerceIn(t.min, t.max)) }
        }
        Tur.RENK -> Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Baslik(t.ad)
            TonCubugu(rgbTon(deger)) { degisti(tonRgb(it)) }
        }
        else -> Column {   // INT (ve bilinmeyen turler: sayi gibi)
            Row(verticalAlignment = Alignment.CenterVertically) {
                Baslik(t.ad); Spacer(Modifier.weight(1f))
                Text("$deger ${t.secenek}".trim(), color = Soluk, fontSize = 13.sp)
            }
            Cubuk(deger, t.min, t.max, t.adim, degisti)
        }
    }
}

@Composable
fun Cubuk(deger: Int, min: Int, max: Int, adim: Int, degisti: (Int) -> Unit) {
    val adimSayisi = if (adim > 0 && max > min) ((max - min) / adim - 1).coerceIn(0, 200) else 0
    Slider(
        value = deger.toFloat().coerceIn(min.toFloat(), max.toFloat()),
        onValueChange = { v: Float -> val a = maxOf(1, adim); degisti(min + ((v - min) / a).roundToInt() * a) },
        valueRange = min.toFloat()..maxOf(min + 1, max).toFloat(),
        steps = adimSayisi,
        colors = SliderDefaults.colors(thumbColor = Color.White, activeTrackColor = Sari, inactiveTrackColor = MorKoyu,
            activeTickColor = Color.Transparent, inactiveTickColor = Color.Transparent)
    )
}

@Composable
fun Secmeli(secenekler: List<String>, secili: Int, sec: (Int) -> Unit) {
    Row(
        Modifier.clip(RoundedCornerShape(22.dp)).background(MorKoyu).padding(4.dp).horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.spacedBy(4.dp)
    ) {
        secenekler.forEachIndexed { i, s ->
            val on = i == secili
            Box(
                Modifier.clip(RoundedCornerShape(18.dp)).background(if (on) Sari else Color.Transparent)
                    .clickable { sec(i) }.padding(horizontal = 16.dp, vertical = 9.dp),
                contentAlignment = Alignment.Center
            ) { Text(s, color = if (on) SariYazi else Soluk, fontSize = 13.sp, fontWeight = FontWeight.SemiBold) }
        }
    }
}

@Composable
fun Bloklar(bloklar: List<Pair<String, Int>>, secili: Int, sec: (Int) -> Unit) {
    Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
        bloklar.forEachIndexed { i, (ad, rgb) ->
            Column(Modifier.width(52.dp).clickable { sec(i) }, horizontalAlignment = Alignment.CenterHorizontally) {
                Box(
                    Modifier.size(44.dp).clip(CircleShape).background(rgbRenk(rgb))
                        .border(3.dp, if (i == secili) Sari else Color.Transparent, CircleShape)
                )
                Text(ad, color = Soluk, fontSize = 10.sp, textAlign = TextAlign.Center, maxLines = 1)
            }
        }
    }
}

/** Ton 0..360 secen gokkusagi cubugu. */
@Composable
fun TonCubugu(ton: Float, degisti: (Float) -> Unit) {
    val guncel by rememberUpdatedState(degisti)      // pointerInput(Unit) eski lambdayi tutmasin
    BoxWithConstraints(Modifier.fillMaxWidth().height(28.dp), contentAlignment = Alignment.CenterStart) {
        val g = maxWidth
        Box(
            Modifier.fillMaxWidth().height(12.dp).clip(RoundedCornerShape(6.dp)).background(tonFircasi)
                .pointerInput(Unit) {
                    awaitEachGesture {
                        val d = awaitFirstDown()
                        fun al(x: Float) = guncel((x / size.width).coerceIn(0f, 1f) * 359f)
                        al(d.position.x)
                        drag(d.id) { al(it.position.x) }
                    }
                }
        )
        Box(
            Modifier.offset(x = (g - 24.dp) * (ton / 359f).coerceIn(0f, 1f)).size(24.dp)
                .shadow(3.dp, CircleShape).clip(CircleShape).background(Color.White)
        )
    }
}

/** Aç/kapa simgesi (material-icons-core'da yok). */
@Composable
fun GucSimgesi(renk: Color, modifier: Modifier = Modifier) {
    Canvas(modifier.size(20.dp)) {
        val w = size.width; val s = 2.4.dp.toPx()
        drawArc(renk, startAngle = -60f, sweepAngle = 300f, useCenter = false,
            topLeft = Offset(w * 0.12f, w * 0.18f), size = androidx.compose.ui.geometry.Size(w * 0.76f, w * 0.76f),
            style = Stroke(s, cap = StrokeCap.Round))
        drawLine(renk, Offset(w / 2, w * 0.06f), Offset(w / 2, w * 0.5f), s, StrokeCap.Round)
    }
}

/** Bluetooth / Wi-Fi rozeti: sari = bu yoldan ulasilabiliyor. */
@Composable
fun YolRozeti(yazi: String, var_: Boolean) {
    Box(
        Modifier.clip(RoundedCornerShape(14.dp)).background(if (var_) Sari else Mor).padding(horizontal = 10.dp, vertical = 5.dp)
    ) { Text(yazi, color = if (var_) SariYazi else Pasif, fontSize = 11.sp, fontWeight = FontWeight.Bold) }
}
