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
val RenkZemin = Color(0xFF8A47B5)  // Renk ayarlari kutusu (erik moru)
val HareketZemin = Color(0xFF3D52C2) // Hareket ayarlari kutusu (mavi-mor)
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
            Text(ad, color = Color.White, fontSize = 12.sp, fontWeight = FontWeight.SemiBold, maxLines = 1)
            Box(Modifier.fillMaxWidth().padding(top = 2.dp), contentAlignment = Alignment.CenterStart) { denetim() }
        }
        return
    }
    Row(Modifier.fillMaxWidth().heightIn(min = 38.dp), verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.width(80.dp)) {
            Text(ad, color = Color.White, fontSize = 12.sp, fontWeight = FontWeight.SemiBold, maxLines = 1)
            if (!alt.isNullOrEmpty()) Text(alt, color = Pasif, fontSize = 11.sp, maxLines = 1)
        }
        Box(Modifier.weight(1f), contentAlignment = Alignment.CenterStart) { denetim() }
    }
}

/** Acma-kapama: kapaliyken kabarik (alt kenar + golge, mor yuz), acikken basili (asagi inmis, sari yuz, ic golge) */
@Composable
fun BasmaDugme(yazi: String, acik: Boolean, degistir: (Boolean) -> Unit) {
    val sekil = RoundedCornerShape(8.dp)
    Box(Modifier.fillMaxWidth().height(33.dp).clickable { degistir(!acik) }) {
        if (!acik) Box(Modifier.fillMaxWidth().height(30.dp).align(Alignment.BottomCenter).shadow(3.dp, sekil).clip(sekil).background(DugmeKenar))
        Box(Modifier.fillMaxWidth().height(30.dp).offset(y = if (acik) 3.dp else 0.dp).clip(sekil)
            .background(if (acik) Brush.verticalGradient(listOf(SariKenar, Sari, Sari)) else Brush.verticalGradient(listOf(DugmeYuz, DugmeYuz))),
            contentAlignment = Alignment.Center) {
            Text(yazi, color = if (acik) SariYazi else Color.White, fontSize = 12.sp, fontWeight = FontWeight.Bold, maxLines = 1)
        }
    }
}

/** Basliklii ayar kutusu: zemin rengi hangi tur ayar oldugunu anlatir */
@Composable
fun AyarKutusu(baslik: String, zemin: Color, not: String? = null, icerik: @Composable () -> Unit) {
    // Baslik yok: kutunun turunu zemin rengi anlatir
    Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(14.dp)).background(zemin)
        .padding(start = 10.dp, end = 10.dp, top = 6.dp, bottom = 6.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) { icerik() }
}

/** Mod ayarlari kutularda; her ayar tek kutuda: Saat grubu -> SAAT, Hareket grubu -> HAREKET,
 *  renk turleri (renk secici, ton, renk bloklari) -> RENK, geri kalan -> GORUNUM. Bos kutu cikmaz. */
@Composable
fun AyarGruplari(liste: List<Tanim>, deger: (Tanim) -> Int, yaz: (Tanim, Int) -> Unit) {
    fun kutu(t: Tanim) = when {
        t.grup == "Saat" -> 3
        t.grup == "Hareket" -> 2
        t.tur == Tur.RENK || t.tur == Tur.TON || t.tur == Tur.BLOK -> 1
        else -> 0
    }
    val adlar = listOf("GÖRÜNÜM" to MorKart, "RENK" to RenkZemin, "HAREKET" to HareketZemin, "SAAT" to SaatZemin)
    val gruplar = liste.groupBy { kutu(it) }
    var ilk = true
    for (k in adlar.indices) {
        val l = gruplar[k] ?: continue
        if (!ilk) Spacer(Modifier.height(4.dp))
        ilk = false
        AyarKutusu(adlar[k].first, adlar[k].second) {
            if (k == 3) SaatDuzeni(l, deger, yaz) else AyarListesi(l, deger, yaz)
        }
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
            startThumb = { Tutamac() },
            endThumb = { Tutamac() },
            track = { _ ->                         // Diger cubuklar gibi: koyu iz, secili aralik sari
                Canvas(Modifier.fillMaxWidth().height(6.dp)) {
                    val r = androidx.compose.ui.geometry.CornerRadius(size.height / 2)
                    drawRoundRect(IzKoyu, cornerRadius = r)
                    drawRoundRect(Sari, topLeft = Offset(size.width * ar.start, 0f),
                        size = androidx.compose.ui.geometry.Size(size.width * (ar.endInclusive - ar.start), size.height), cornerRadius = r)
                }
            }
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
        // Ayni satira: en cok 3 dugme (BOOL), ya da 2 kisa secim, ya da 2 kaydirma cubugu (yarim genislik)
        val a = liste[i]
        val n = when {
            a.tur == Tur.BOOL -> { var k = 1; while (k < 3 && liste.getOrNull(i + k)?.tur == Tur.BOOL) k++; k }
            kisaAyar(a) && liste.getOrNull(i + 1)?.let { kisaAyar(it) && it.tur != Tur.BOOL } == true -> 2
            kisaAyar(a) && liste.getOrNull(i + 1)?.tur == Tur.BOOL -> 2      // Kisa secim + yaninda tek dugme (Ruzgar | Oto)
            sayiAyar(a) && liste.getOrNull(i + 1)?.let { sayiAyar(it) } == true -> 2
            else -> 1
        }
        if (n == 1) AyarSatiri(a, deger(a)) { yaz(a, it) }
        else Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.Bottom) {
            for (k in 0 until n) { val t = liste[i + k]; Box(Modifier.weight(1f)) { AyarSatiri(t, deger(t), ust = true) { yaz(t, it) } } }
            if (a.tur == Tur.BOOL) repeat(3 - n) { Spacer(Modifier.weight(1f)) }		// Dugmeler hep ayni boyda
        }
        i += n
    }
}

/** Kaydirma cubuklu sayi ayari (iki tanesi yarim genislikte yan yana sigar) */
fun sayiAyar(t: Tanim) = t.tur != Tur.BOOL && t.tur != Tur.ENUM && t.tur != Tur.BLOK && t.tur != Tur.TON && t.tur != Tur.RENK

/** Saat kutusu: Font + Saniye / Sol Orta Alt / Isi-Nem / Renk / Parlaklik + Saydamlik. Bilinmeyen saat ayarlari altta. */
@Composable
fun SaatDuzeni(liste: List<Tanim>, deger: (Tanim) -> Int, yaz: (Tanim, Int) -> Unit) {
    fun bul(id: Int) = liste.firstOrNull { it.id == id }
    val font = bul(AyarId.SAAT_YAZI); val sn = bul(AyarId.SAAT_SN); val yer = bul(AyarId.SAAT_YER)
    val alt = bul(AyarId.SAAT_ALT); val isi = bul(AyarId.SAAT_ISI); val rgb = bul(AyarId.SAAT_RGB)
    val isik = bul(AyarId.SAAT_ISIK); val say = bul(AyarId.SAAT_SAYDAM)
    val bilinen = setOfNotNull(font, sn, yer, alt, isi, rgb, isik, say)
    if (font != null || sn != null) Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
        if (font != null) Box(Modifier.weight(2f)) {
            SecmeliResim(font.secenekler, deger(font) - font.min, saniye = sn != null && deger(sn) != 0) { yaz(font, font.min + it) }
        }
        if (sn != null) Box(Modifier.weight(1f)) { BasmaDugme(sn.ad, deger(sn) != 0) { yaz(sn, if (it) 1 else 0) } }
    }
    if (yer != null || alt != null) Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        yer?.secenekler?.forEachIndexed { k, s -> Box(Modifier.weight(1f)) { BasmaDugme(s, deger(yer) - yer.min == k) { yaz(yer, yer.min + k) } } }
        if (alt != null) Box(Modifier.weight(1f)) { BasmaDugme(alt.ad, deger(alt) != 0) { yaz(alt, if (it) 1 else 0) } }
    }
    if (isi != null) Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        Box(Modifier.weight(1f)) { BasmaDugme(isi.ad, deger(isi) != 0) { yaz(isi, if (it) 1 else 0) } }
        Spacer(Modifier.weight(2f))
    }
    if (rgb != null) TekSatir("Renk") { SaatRenk(deger(rgb)) { yaz(rgb, it) } }
    val cubuklar = listOfNotNull(isik?.copy(ad = "Parlaklık"), say)
    if (cubuklar.isNotEmpty()) AyarListesi(cubuklar, { t -> deger(liste.first { it.id == t.id }) }) { t, v -> yaz(liste.first { it.id == t.id }, v) }
    val kalan = liste.filter { it !in bilinen }
    if (kalan.isNotEmpty()) AyarListesi(kalan, deger, yaz)
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
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(Modifier.weight(1f)) { RenkCubugu(rgbKonum(deger.coerceAtLeast(0)), beyaz = true) { degisti(konumRgb(it)) } }
                // Fabrika rengi: -1 gonderilir, cihaz bu modun ilk rengini koyup geri bildirir
                Box(Modifier.padding(start = 8.dp).size(30.dp).clip(CircleShape).background(DugmeYuz).clickable { degisti(-1) },
                    contentAlignment = Alignment.Center) { Text("↺", color = Color.White, fontSize = 16.sp, fontWeight = FontWeight.Bold) }
            }
        }
        else -> if (ust) TekSatir("${t.ad}  $deger${t.secenek}", ust = true) { Cubuk(deger, t.min, t.max, t.adim, degisti) }
        else TekSatir(t.ad) {   // INT (ve bilinmeyen turler: sayi gibi)
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
fun SecmeliResim(secenekler: List<String>, secili: Int, saniye: Boolean = false, sec: (Int) -> Unit) {
    val ctx = LocalContext.current
    Row(
        Modifier.clip(RoundedCornerShape(20.dp)).background(MorKoyu).padding(3.dp).horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.spacedBy(2.dp)
    ) {
        secenekler.forEachIndexed { i, s ->
            val on = i == secili
            val resim = remember(s, saniye) {     // Saniye aciksa "12:34:56" ornegi (panelde nasilsa oyle)
                runCatching { ctx.assets.open("saatyazi/${sadeAd(s)}${if (saniye) "_sn" else ""}.png").use { BitmapFactory.decodeStream(it)?.asImageBitmap() } }.getOrNull()
            }
            Box(
                Modifier.clip(RoundedCornerShape(18.dp)).background(if (on) Sari else Color.Transparent)
                    .clickable { sec(i) }.padding(horizontal = 8.dp, vertical = 5.dp),
                contentAlignment = Alignment.Center
            ) {
                if (resim != null) Image(resim, s, Modifier.height(18.dp), filterQuality = FilterQuality.None,
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
fun RenkCubugu(konum: Float, beyaz: Boolean, firca: Brush? = null, degisti: (Float) -> Unit) {
    val guncel by rememberUpdatedState(degisti)      // pointerInput(Unit) eski lambdayi tutmasin
    BoxWithConstraints(Modifier.fillMaxWidth().height(28.dp), contentAlignment = Alignment.CenterStart) {
        val g = maxWidth
        Box(
            Modifier.fillMaxWidth().height(12.dp).clip(RoundedCornerShape(6.dp))
                .background(firca ?: if (beyaz) beyazTonFircasi else tonFircasi)
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

/* Saat rengi: ton (ilk cubuk) + aciklik (ikinci cubuk: sol koyu, orta rengin kendisi, sag pastel).
   Kart tek RGB tutar; iki deger gelen RGB'den geri hesaplanir. Beyaz ton: koyu tarafta gri kademe. */
private const val KOYU_TABAN = 0.15f        // En sol: rengin %15'i
private const val PASTEL_UST = 0.8f         // En sag: beyaza %80 (ton secilebilir kalsin)

private fun acikRgb(taban: Int, a: Float): Int {
    fun k(c: Int): Int = if (a < 0.5f) (c * (KOYU_TABAN + (1f - KOYU_TABAN) * a * 2f)).roundToInt()
                         else (c + (255 - c) * (a - 0.5f) * 2f * PASTEL_UST).roundToInt()
    return (k(taban shr 16 and 255).coerceIn(0, 255) shl 16) or (k(taban shr 8 and 255).coerceIn(0, 255) shl 8) or k(taban and 255).coerceIn(0, 255)
}

/** RGB -> (taban renk, aciklik 0..1) */
private fun rgbAyir(v: Int): Pair<Int, Float> {
    val r = v shr 16 and 255; val g = v shr 8 and 255; val b = v and 255
    val m = maxOf(r, g, b); val n = minOf(r, g, b)
    if (m == 0) return 0xFFFFFF to 0f
    val hsv = FloatArray(3); android.graphics.Color.colorToHSV(0xFF000000.toInt() or v, hsv)
    if (hsv[1] < 0.1f) return 0xFFFFFF to (0.5f * ((m / 255f - KOYU_TABAN) / (1f - KOYU_TABAN))).coerceIn(0f, 0.5f)
    val taban = tonRgb(hsv[0])
    return taban to (if (n > 0 && m >= 250) 0.5f + 0.5f * (n / 255f / PASTEL_UST).coerceIn(0f, 1f)
                     else (0.5f * ((m / 255f - KOYU_TABAN) / (1f - KOYU_TABAN))).coerceIn(0f, 0.5f))
}

@Composable
fun SaatRenk(deger: Int, degisti: (Int) -> Unit) {
    val (taban, acik) = rgbAyir(deger.coerceAtLeast(0))
    fun renk(c: Int) = Color(0xFF000000.toInt() or c)
    Row(verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f)) {
            RenkCubugu(rgbKonum(taban), beyaz = true) { degisti(acikRgb(konumRgb(it), acik)) }
            RenkCubugu(acik, beyaz = false,
                firca = Brush.horizontalGradient(listOf(renk(acikRgb(taban, 0f)), renk(taban), renk(acikRgb(taban, 1f))))) {
                degisti(acikRgb(taban, it))
            }
        }
        // Fabrika rengi: -1 gonderilir, cihaz bu modun ilk rengini koyup geri bildirir (iki cubuk birden doner)
        Box(Modifier.padding(start = 8.dp).size(30.dp).clip(CircleShape).background(DugmeYuz).clickable { degisti(-1) },
            contentAlignment = Alignment.Center) { Text("↺", color = Color.White, fontSize = 16.sp, fontWeight = FontWeight.Bold) }
    }
}

/** Hoparlor simgesi (material-icons-core'da yok): ses dalgalariyla. */
@Composable
fun HoparlorSimgesi(renk: Color, modifier: Modifier = Modifier) {
    Canvas(modifier.size(20.dp)) {
        val w = size.width; val s = 1.8.dp.toPx()
        val p = androidx.compose.ui.graphics.Path().apply {
            moveTo(w * 0.08f, w * 0.38f); lineTo(w * 0.26f, w * 0.38f); lineTo(w * 0.48f, w * 0.18f)
            lineTo(w * 0.48f, w * 0.82f); lineTo(w * 0.26f, w * 0.62f); lineTo(w * 0.08f, w * 0.62f); close()
        }
        drawPath(p, renk)
        for (k in 1..2) {
            val r = w * 0.16f * k
            drawArc(renk, -50f, 100f, false, topLeft = Offset(w * 0.50f - r, w * 0.5f - r),
                size = androidx.compose.ui.geometry.Size(r * 2, r * 2), style = Stroke(s, cap = StrokeCap.Round))
        }
    }
}

/** Yalniz simgeli basma dugmesi: acikken sari ve basik, kapaliyken soluk ve kabarik. */
@Composable
fun SimgeDugme(acik: Boolean, degistir: (Boolean) -> Unit, simge: @Composable (Color) -> Unit) {
    val sekil = RoundedCornerShape(8.dp)
    Box(Modifier.size(width = 46.dp, height = 33.dp).clickable { degistir(!acik) }) {
        if (!acik) Box(Modifier.fillMaxWidth().height(30.dp).align(Alignment.BottomCenter).shadow(3.dp, sekil).clip(sekil).background(DugmeKenar))
        Box(Modifier.fillMaxWidth().height(30.dp).offset(y = if (acik) 3.dp else 0.dp).clip(sekil)
            .background(if (acik) Brush.verticalGradient(listOf(SariKenar, Sari, Sari)) else Brush.verticalGradient(listOf(DugmeYuz, DugmeYuz))),
            contentAlignment = Alignment.Center) { simge(if (acik) SariYazi else Soluk.copy(alpha = 0.6f)) }
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

/** Bagli yol simgesi (yazisiz): 0 Bluetooth, 1 ev agi (Wi-Fi + ev), 2 dogrudan Wi-Fi (panelin kendi agi) */
@Composable
fun BaglantiSimgesi(yol: Int, renk: Color, modifier: Modifier = Modifier) {
    Canvas(modifier.size(20.dp)) {
        val w = size.width; val s = 1.8.dp.toPx()
        val st = Stroke(s, cap = StrokeCap.Round, join = androidx.compose.ui.graphics.StrokeJoin.Round)
        if (yol == 0) {                                   // Bluetooth runu
            val p = androidx.compose.ui.graphics.Path().apply {
                moveTo(w * 0.28f, w * 0.30f); lineTo(w * 0.70f, w * 0.68f); lineTo(w * 0.50f, w * 0.88f)
                lineTo(w * 0.50f, w * 0.12f); lineTo(w * 0.70f, w * 0.32f); lineTo(w * 0.28f, w * 0.70f)
            }
            drawPath(p, renk, style = st)
        } else {                                          // Wi-Fi yaylari
            val mx = if (yol == 1) w * 0.40f else w * 0.5f
            for (k in 1..3) {
                val r = w * 0.13f * k
                drawArc(renk, 225f, 90f, false, topLeft = Offset(mx - r, w * 0.72f - r),
                    size = androidx.compose.ui.geometry.Size(r * 2, r * 2), style = st)
            }
            drawCircle(renk, s * 0.9f, Offset(mx, w * 0.72f))
            if (yol == 1) {                               // Ev: sag altta kucuk ev
                val e = androidx.compose.ui.graphics.Path().apply {
                    moveTo(w * 0.66f, w * 0.96f); lineTo(w * 0.66f, w * 0.74f); lineTo(w * 0.80f, w * 0.60f)
                    lineTo(w * 0.94f, w * 0.74f); lineTo(w * 0.94f, w * 0.96f); close()
                }
                drawPath(e, renk, style = Stroke(s * 0.8f, join = androidx.compose.ui.graphics.StrokeJoin.Round))
            }
        }
    }
}

/** Bluetooth / Wi-Fi rozeti: sari = bu yoldan ulasilabiliyor. */
@Composable
fun YolRozeti(yazi: String, var_: Boolean) {
    Box(
        Modifier.clip(RoundedCornerShape(14.dp)).background(if (var_) Sari else Mor).padding(horizontal = 10.dp, vertical = 5.dp)
    ) { Text(yazi, color = if (var_) SariYazi else Pasif, fontSize = 11.sp, fontWeight = FontWeight.Bold) }
}
