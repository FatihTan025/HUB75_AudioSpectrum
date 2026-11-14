package com.spektrum.kontrol.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.gestures.drag
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Slider
import androidx.compose.material3.RangeSlider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.runtime.remember
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
import android.graphics.BitmapFactory
import androidx.compose.foundation.Image
import androidx.compose.ui.graphics.ColorFilter
import androidx.compose.ui.graphics.FilterQuality
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.platform.LocalContext
import com.spektrum.kontrol.protokol.AyarId

// --- Renkler (LED Chord'dan esinli: mor zemin, sari vurgu) -------------------------
val Mor = Color(0xFF5B3FD1)
val MorKart = Color(0xFF6E54DC)
val MorKoyu = Color(0xFF4A31B8)
val MorSecili = Color(0xFF7F67E6)
val Sari = Color(0xFFFFD23F)
val SariYazi = Color(0xFF2A1A00)
val Soluk = Color(0xFFE4DCFF)
val Pasif = Color(0xFFB9AEEB)
val SariAcik = Color(0xFFFFE680)
val SariKenar = Color(0xFFC79A00)   // Kabarik dugmenin alt kenari
val IzKoyu = Color(0xFF2A1A78)     // Kaydirma cubugunun bos tarafi (zeminden belirgin)
val SaatZemin = Color(0xFF26306E)  // Saat ayarlari kutusu (lacivert)
val PanelZemin = Color(0xFF34304A) // Panel (tum modlar) ayarlari kutusu (koyu gri)
val DugmeYuz = Color(0xFF7A62E6)   // Basilmamis dugme yuzu
val DugmeKenar = Color(0xFF3A2896) // Basilmamis dugmenin alt kenari

fun rgbRenk(v: Int) = Color(0xFF000000.toInt() or (v and 0xFFFFFF))
fun tonRenk(h: Float) = Color(android.graphics.Color.HSVToColor(floatArrayOf(h.coerceIn(0f, 359.9f), 1f, 1f)))
fun tonRgb(h: Float) = android.graphics.Color.HSVToColor(floatArrayOf(h, 1f, 1f)) and 0xFFFFFF

@Composable
fun Baslik(yazi: String) = Text(yazi, color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.SemiBold)

/** Ayar satiri: ad solda (altinda secili deger kucuk), denetim sagda; tek satir, sayfa uzamasin. */
@Composable
fun TekSatir(ad: String, alt: String? = null, ust: Boolean = false, denetim: @Composable () -> Unit) {
    if (ust) {                                  // Dar (yan yana iki ayar): ad ustte, denetim altta
        Column(Modifier.fillMaxWidth()) {
            Text(ad, color = Color.White, fontSize = 13.sp, fontWeight = FontWeight.SemiBold, maxLines = 1)
            Box(Modifier.fillMaxWidth().padding(top = 4.dp), contentAlignment = Alignment.CenterStart) { denetim() }
        }
        return
    }
    Row(Modifier.fillMaxWidth().heightIn(min = 52.dp), verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.width(96.dp)) {
            Text(ad, color = Color.White, fontSize = 13.sp, fontWeight = FontWeight.SemiBold, maxLines = 1)
            if (!alt.isNullOrEmpty()) Text(alt, color = Pasif, fontSize = 11.sp, maxLines = 1)
        }
        Box(Modifier.weight(1f), contentAlignment = Alignment.CenterStart) { denetim() }
    }
}

/** Acma-kapama: kapaliyken kabarik (alt kenar + golge, mor yuz), acikken basili (asagi inmis, sari yuz, ic golge) */
@Composable
fun BasmaDugme(yazi: String, acik: Boolean, degistir: (Boolean) -> Unit) {
    val sekil = RoundedCornerShape(10.dp)
    Box(Modifier.fillMaxWidth().height(46.dp).clickable { degistir(!acik) }) {
        if (!acik) Box(Modifier.fillMaxWidth().height(42.dp).align(Alignment.BottomCenter).shadow(4.dp, sekil).clip(sekil).background(DugmeKenar))
        Box(Modifier.fillMaxWidth().height(42.dp).offset(y = if (acik) 4.dp else 0.dp).clip(sekil)
            .background(if (acik) Brush.verticalGradient(listOf(SariKenar, Sari, Sari)) else Brush.verticalGradient(listOf(DugmeYuz, DugmeYuz))),
            contentAlignment = Alignment.Center) {
            Text(yazi, color = if (acik) SariYazi else Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold, maxLines = 1)
        }
    }
}

/** Basliklii ayar kutusu: zemin rengi hangi tur ayar oldugunu anlatir */
@Composable
fun AyarKutusu(baslik: String, zemin: Color, not: String? = null, icerik: @Composable () -> Unit) {
    Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(16.dp)).background(zemin).padding(horizontal = 12.dp, vertical = 10.dp)) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(baslik, color = Soluk, fontSize = 12.sp, fontWeight = FontWeight.Bold)
            if (not != null) Text("  ·  $not", color = Pasif, fontSize = 11.sp)
        }
        Spacer(Modifier.height(6.dp))
        icerik()
    }
}

/** Mod ayarlari iki kutuda: animasyonun kendi ayarlari (mor) ve saat ayarlari (lacivert) */
@Composable
fun AyarGruplari(liste: List<Tanim>, deger: (Tanim) -> Int, yaz: (Tanim, Int) -> Unit) {
    val (saat, diger) = liste.partition { it.grup == "Saat" }
    if (diger.isNotEmpty()) AyarKutusu("ANİMASYON", MorKart) { AyarListesi(diger, deger, yaz) }
    if (saat.isNotEmpty()) {
        if (diger.isNotEmpty()) Spacer(Modifier.height(10.dp))
        AyarKutusu("SAAT", SaatZemin) { AyarListesi(saat, deger, yaz) }
    }
}

/** Frekans araligi: tek cubukta iki tutamac (60 Hz - 20 kHz, logaritmik). Ust en az 4 x alt. */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun FrekansCubugu(alt: Int, ust: Int, yaz: (Int, Int) -> Unit) {
    val oran = kotlin.math.ln(20000f / 60f)
    fun konum(f: Int) = (kotlin.math.ln(f / 60f) / oran).coerceIn(0f, 1f)
    fun frekans(k: Float) = ((60f * kotlin.math.exp(k * oran)) / 10f).roundToInt() * 10
    fun yazi(f: Int) = if (f >= 1000) "%.1f kHz".format(f / 1000f).replace(".0 ", " ") else "$f Hz"
    var ar by remember(alt, ust) { mutableStateOf(konum(alt)..konum(ust)) }
    Column(Modifier.fillMaxWidth()) {
        Text("${yazi(frekans(ar.start))}  –  ${yazi(frekans(ar.endInclusive))}", color = Color.White, fontSize = 13.sp, fontWeight = FontWeight.SemiBold)
        RangeSlider(
            value = ar,
            onValueChange = { y -> if (frekans(y.endInclusive) >= 4 * frekans(y.start)) ar = y },
            onValueChangeFinished = { yaz(frekans(ar.start).coerceIn(60, 5000), frekans(ar.endInclusive).coerceIn(240, 20000)) },
            colors = SliderDefaults.colors(thumbColor = Color.White, activeTrackColor = Sari, inactiveTrackColor = IzKoyu)
        )
        Row(Modifier.fillMaxWidth()) {
            Text("60 Hz", color = Pasif, fontSize = 11.sp)
            Spacer(Modifier.weight(1f))
            Text("20 kHz", color = Pasif, fontSize = 11.sp)
        }
    }
}

/** Kisa ayar: anahtar ya da en cok 3 kisa secenekli secim -> iki tanesi yan yana sigar */
fun kisaAyar(t: Tanim) = t.tur == Tur.BOOL ||
    (t.tur == Tur.ENUM && t.id != AyarId.SAAT_YAZI && t.secenekler.size <= 3 && t.secenekler.sumOf { it.length } <= 12)

/** Ayar listesi: art arda gelen iki kisa ayar ayni satirda, digerleri tam satir (sira degismez) */
@Composable
fun AyarListesi(liste: List<Tanim>, deger: (Tanim) -> Int, yaz: (Tanim, Int) -> Unit) {
    var i = 0
    while (i < liste.size) {
        val a = liste[i]
        val b = liste.getOrNull(i + 1)
        if (b != null && kisaAyar(a) && kisaAyar(b)) {
            Row(Modifier.fillMaxWidth().padding(vertical = 4.dp), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                Box(Modifier.weight(1f)) { AyarSatiri(a, deger(a), ust = true) { yaz(a, it) } }
                Box(Modifier.weight(1f)) { AyarSatiri(b, deger(b), ust = true) { yaz(b, it) } }
            }
            i += 2
        } else {
            AyarSatiri(a, deger(a)) { yaz(a, it) }
            i++
        }
    }
}

/**
 * Cihazdan gelen bir ayar tanimini turune gore cizer:
 * ton = ton cubugu, renk = beyaz + ton cubugu, blok = renk bloklari, enum = secmeli dugmeler,
 * bool = anahtar, int = kaydirma cubugu.
 */
@Composable
fun AyarSatiri(t: Tanim, deger: Int, ust: Boolean = false, degisti: (Int) -> Unit) {
    when (t.tur) {
        Tur.BOOL -> BasmaDugme(t.ad, deger != 0) { if (!t.saltOkunur) degisti(if (it) 1 else 0) }
        Tur.ENUM -> TekSatir(t.ad, ust = ust) {
            if (t.id == AyarId.SAAT_YAZI) SecmeliResim(t.secenekler, deger - t.min) { degisti(t.min + it) }   // Font ornegi
            else Secmeli(t.secenekler, deger - t.min) { degisti(t.min + it) }
        }
        Tur.BLOK -> TekSatir(t.ad, t.bloklar.getOrNull(deger - t.min)?.first) { Bloklar(t.bloklar, deger - t.min) { degisti(t.min + it) } }
        Tur.TON -> TekSatir(t.ad) {
            RenkCubugu(deger.toFloat() / (t.max + 1).coerceAtLeast(1), beyaz = false) {
                degisti((it * (t.max + 1)).toInt().coerceIn(t.min, t.max))
            }
        }
        Tur.RENK -> TekSatir(t.ad) {
            RenkCubugu(rgbKonum(deger), beyaz = true) { degisti(konumRgb(it)) }
        }
        else -> TekSatir(t.ad) {   // INT (ve bilinmeyen turler: sayi gibi)
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(Modifier.weight(1f)) { Cubuk(deger, t.min, t.max, t.adim, degisti) }
                Text("$deger ${t.secenek}".trim(), color = Soluk, fontSize = 12.sp, maxLines = 1,
                    modifier = Modifier.padding(start = 6.dp))
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun Cubuk(deger: Int, min: Int, max: Int, adim: Int, degisti: (Int) -> Unit) {
    val adimSayisi = if (adim > 0 && max > min) ((max - min) / adim - 1).coerceIn(0, 200) else 0
    Slider(
        value = deger.toFloat().coerceIn(min.toFloat(), max.toFloat()),
        onValueChange = { v: Float -> val a = maxOf(1, adim); degisti(min + ((v - min) / a).roundToInt() * a) },
        valueRange = min.toFloat()..maxOf(min + 1, max).toFloat(),
        steps = adimSayisi,
        thumb = { Tutamac() },
        track = { st ->
            val ar = st.valueRange.endInclusive - st.valueRange.start
            val f = if (ar > 0f) ((st.value - st.valueRange.start) / ar).coerceIn(0f, 1f) else 0f
            Box(Modifier.fillMaxWidth().height(6.dp).clip(RoundedCornerShape(3.dp)).background(IzKoyu)) {
                Box(Modifier.fillMaxWidth(f).fillMaxHeight().background(Sari))
            }
        }
    )
}

/** Kabarik (3B) dugme: altta koyu kenar; basinca yuz asagi iner, kenar kaybolur. */
@Composable
fun KabarikDugme(yazi: String, tikla: () -> Unit) {
    val ka = remember { MutableInteractionSource() }
    val basili by ka.collectIsPressedAsState()
    val sekil = RoundedCornerShape(10.dp)
    Box(Modifier.width(96.dp).height(42.dp).clickable(interactionSource = ka, indication = null, onClick = tikla)) {
        Box(Modifier.fillMaxWidth().height(38.dp).align(Alignment.BottomCenter)
            .shadow(if (basili) 0.dp else 4.dp, sekil).clip(sekil).background(SariKenar))
        Box(Modifier.fillMaxWidth().height(38.dp).offset(y = if (basili) 4.dp else 0.dp).clip(sekil)
            .background(Brush.verticalGradient(listOf(SariAcik, if (basili) SariKenar else Sari))),
            contentAlignment = Alignment.Center) {
            Text(yazi, color = SariYazi, fontSize = 14.sp, fontWeight = FontWeight.Bold)
        }
    }
}

/** Kaydirma ve renk cubugu tutamaci: siyah daire, beyaz halka (mor zeminde secilsin) */
@Composable
fun Tutamac() {
    Box(Modifier.size(22.dp).shadow(3.dp, CircleShape).clip(CircleShape).background(Color.Black).border(2.dp, Color.White, CircleShape))
}

@Composable
fun Secmeli(secenekler: List<String>, secili: Int, sec: (Int) -> Unit) {
    Row(
        Modifier.clip(RoundedCornerShape(20.dp)).background(MorKoyu).padding(3.dp).horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.spacedBy(2.dp)
    ) {
        secenekler.forEachIndexed { i, s ->
            val on = i == secili
            Box(
                Modifier.clip(RoundedCornerShape(18.dp)).background(if (on) Sari else Color.Transparent)
                    .clickable { sec(i) }.padding(horizontal = 14.dp, vertical = 8.dp),
                contentAlignment = Alignment.Center
            ) { Text(s, color = if (on) SariYazi else Soluk, fontSize = 13.sp, fontWeight = FontWeight.SemiBold, maxLines = 1) }
        }
    }
}

/** Saat yazisi: secenekler o fontla yazilmis "12:34" resmi (assets/saatyazi/<ad>.png); resim yoksa ad yazilir */
@Composable
fun SecmeliResim(secenekler: List<String>, secili: Int, sec: (Int) -> Unit) {
    val ctx = LocalContext.current
    Row(
        Modifier.clip(RoundedCornerShape(20.dp)).background(MorKoyu).padding(3.dp).horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.spacedBy(2.dp)
    ) {
        secenekler.forEachIndexed { i, s ->
            val on = i == secili
            val resim = remember(s) {
                runCatching { ctx.assets.open("saatyazi/${sadeAd(s)}.png").use { BitmapFactory.decodeStream(it)?.asImageBitmap() } }.getOrNull()
            }
            Box(
                Modifier.clip(RoundedCornerShape(18.dp)).background(if (on) Sari else Color.Transparent)
                    .clickable { sec(i) }.padding(horizontal = 12.dp, vertical = 6.dp),
                contentAlignment = Alignment.Center
            ) {
                if (resim != null) Image(resim, s, Modifier.height(20.dp), filterQuality = FilterQuality.None,
                    colorFilter = ColorFilter.tint(if (on) SariYazi else Soluk))
                else Text(s, color = if (on) SariYazi else Soluk, fontSize = 13.sp, fontWeight = FontWeight.SemiBold, maxLines = 1)
            }
        }
    }
}

@Composable
fun Bloklar(bloklar: List<Pair<String, List<Int>>>, secili: Int, sec: (Int) -> Unit) {
    Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(8.dp),
        verticalAlignment = Alignment.CenterVertically) {
        bloklar.forEachIndexed { i, (_, renkler) ->
            val ic = Modifier.size(32.dp).clip(CircleShape).clickable { sec(i) }
                .border(3.dp, if (i == secili) Sari else Color.Transparent, CircleShape)
                .padding(4.dp).clip(CircleShape)
            Box(
                if (renkler.size > 1) ic.background(Brush.linearGradient(renkler.map { rgbRenk(it) }))   // renk ailesi: gecis
                else ic.background(rgbRenk(renkler.firstOrNull() ?: 0x808080))
            )
        }
    }
}

/** Renk cubugunda beyaz bolumun payi (bastan) */
private const val BEYAZ = 0.08f
private val tonDuraklari = listOf(0f, 60f, 120f, 180f, 240f, 300f, 359f)
private val tonFircasi = Brush.horizontalGradient(tonDuraklari.map { tonRenk(it) })
private val beyazTonFircasi = Brush.horizontalGradient(
    *(listOf(0f to Color.White, BEYAZ to Color.White) +
      tonDuraklari.map { BEYAZ + (1f - BEYAZ) * it / 359f to tonRenk(it) }).toTypedArray()
)

/** RGB <-> cubuk konumu (0..1). Doygunlugu dusuk renk = beyaz bolum. */
fun rgbKonum(v: Int): Float {
    val hsv = FloatArray(3); android.graphics.Color.colorToHSV(0xFF000000.toInt() or v, hsv)
    return if (hsv[1] < 0.25f) BEYAZ / 2 else BEYAZ + (1f - BEYAZ) * hsv[0] / 359f
}
fun konumRgb(k: Float): Int = if (k < BEYAZ) 0xFFFFFF else tonRgb(((k - BEYAZ) / (1f - BEYAZ)).coerceIn(0f, 1f) * 359f)

/** Renk cubugu: konum 0..1; beyaz = basta beyaz bolum (renk ayarlari). */
@Composable
fun RenkCubugu(konum: Float, beyaz: Boolean, degisti: (Float) -> Unit) {
    val guncel by rememberUpdatedState(degisti)      // pointerInput(Unit) eski lambdayi tutmasin
    BoxWithConstraints(Modifier.fillMaxWidth().height(28.dp), contentAlignment = Alignment.CenterStart) {
        val g = maxWidth
        Box(
            Modifier.fillMaxWidth().height(12.dp).clip(RoundedCornerShape(6.dp))
                .background(if (beyaz) beyazTonFircasi else tonFircasi)
                .pointerInput(Unit) {
                    awaitEachGesture {
                        val d = awaitFirstDown()
                        fun al(x: Float) = guncel((x / size.width).coerceIn(0f, 0.999f))
                        al(d.position.x)
                        drag(d.id) { al(it.position.x) }
                    }
                }
        )
        Box(Modifier.offset(x = (g - 22.dp) * konum.coerceIn(0f, 1f))) { Tutamac() }
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
