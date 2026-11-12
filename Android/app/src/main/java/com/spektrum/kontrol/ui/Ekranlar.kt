package com.spektrum.kontrol.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.FloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationBarItemDefaults
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import android.content.Intent
import android.provider.Settings
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.LocalContext
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.compose.LifecycleEventEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.spektrum.kontrol.AnaModel
import com.spektrum.kontrol.Bulunan
import com.spektrum.kontrol.KENDI_AG_IP
import com.spektrum.kontrol.Sekme
import com.spektrum.kontrol.protokol.AyarId
import com.spektrum.kontrol.protokol.MOD_GENEL
import com.spektrum.kontrol.protokol.Mod

@Composable
fun Uygulama(m: AnaModel) {
    val snack = remember { SnackbarHostState() }
    val mesaj = m.mesaj.value
    LaunchedEffect(mesaj) {
        if (mesaj != null) { snack.showSnackbar(mesaj); m.mesaj.value = null }
    }
    Box(Modifier.fillMaxSize().background(Mor)) {
        if (m.cihaz.value == null) BaglanEkrani(m) else AnaEkran(m)
        SnackbarHost(snack, Modifier.align(Alignment.BottomCenter).padding(bottom = 80.dp))
    }
}

// =============================================================================
//  Baglanti: her cihaz tek satir, yaninda ulasilabilen yollar (Bluetooth / Wi-Fi)
// =============================================================================
@Composable
fun BaglanEkrani(m: AnaModel) {
    var secili by remember { mutableStateOf<String?>(null) }
    val liste = m.bulunanlar
    val sec = liste.firstOrNull { it.ad == secili } ?: liste.firstOrNull()
    val ctx = LocalContext.current
    // Konum ayarindan donunce yeniden ara
    LifecycleEventEffect(Lifecycle.Event.ON_RESUME) { if (m.konumKapali.value) m.taramaBaslat() }

    Column(Modifier.fillMaxSize().padding(horizontal = 18.dp, vertical = 16.dp), verticalArrangement = Arrangement.spacedBy(16.dp)) {
        Row(Modifier.fillMaxWidth().height(48.dp), verticalAlignment = Alignment.CenterVertically) {
            Text("Spektrum", color = Color.White, fontSize = 20.sp, fontWeight = FontWeight.Bold)
            Spacer(Modifier.weight(1f))
            if (m.taraniyor.value) CircularProgressIndicator(Modifier.size(22.dp), color = Sari, strokeWidth = 2.dp)
            IconButton(onClick = { m.taramaBaslat() }) { Icon(Icons.Default.Refresh, "Yeniden ara", tint = Color.White) }
        }

        if (m.konumKapali.value) {
            Row(
                Modifier.fillMaxWidth().clip(RoundedCornerShape(14.dp)).background(Sari).padding(14.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text("Bluetooth cihazlarını bulmak için telefonun Konum ayarını açın.", color = SariYazi,
                    fontSize = 13.sp, modifier = Modifier.weight(1f))
                TextButton({ ctx.startActivity(Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS)) }) {
                    Text("AÇ", color = SariYazi, fontWeight = FontWeight.Bold)
                }
            }
        }

        Column(
            Modifier.weight(1f).fillMaxWidth().clip(RoundedCornerShape(16.dp)).background(MorKart).padding(8.dp)
        ) {
            Text("BULUNAN CİHAZLAR", color = Soluk, fontSize = 12.sp, fontWeight = FontWeight.SemiBold,
                modifier = Modifier.padding(start = 10.dp, top = 10.dp, bottom = 6.dp))
            if (liste.isEmpty()) {
                Text(if (m.taraniyor.value) "Aranıyor..." else "Cihaz bulunamadı. Cihazın açık ve yakında olduğundan emin olun.",
                    color = Soluk, fontSize = 13.sp, modifier = Modifier.padding(10.dp))
            }
            LazyColumn(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                items(liste, key = { it.ad }) { b ->
                    CihazSatiri(m, b, b == sec) { secili = b.ad }
                }
            }
        }

        Button(
            onClick = { sec?.let { m.baglan(it) } },
            enabled = sec != null && m.baglaniyor.value == null,
            modifier = Modifier.fillMaxWidth().height(56.dp),
            shape = RoundedCornerShape(28.dp),
            colors = ButtonDefaults.buttonColors(containerColor = Sari, contentColor = SariYazi,
                disabledContainerColor = MorKoyu, disabledContentColor = Pasif)
        ) {
            Text(if (m.baglaniyor.value != null) "BAĞLANIYOR..." else "BAĞLAN", fontSize = 17.sp, fontWeight = FontWeight.Bold)
        }
    }
}

@Composable
private fun CihazSatiri(m: AnaModel, b: Bulunan, secili: Boolean, tikla: () -> Unit) {
    val ad = m.gorunenAd(b.ad) ?: b.ad
    // Ulasilabilen yollar; baglanirken bu siraya gore denenir (calisan secilir)
    val yollar = buildList {
        if (b.ipler.any { it != KENDI_AG_IP }) add("ev ağı")
        if (KENDI_AG_IP in b.ipler) add("doğrudan")
        if (b.ble != null) add("Bluetooth")
    }.joinToString(" · ")
    val alt = if (ad != b.ad) "${b.ad} · $yollar" else yollar
    Row(
        Modifier.fillMaxWidth().clip(RoundedCornerShape(12.dp)).background(if (secili) MorSecili else Color.Transparent)
            .clickable(onClick = tikla).padding(horizontal = 12.dp, vertical = 14.dp),
        verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Column(Modifier.weight(1f)) {
            Text(ad, color = Color.White, fontSize = 16.sp, fontWeight = FontWeight.SemiBold)
            Text(alt, color = Soluk, fontSize = 12.sp)
        }
        YolRozeti("BT", b.ble != null)
        YolRozeti("Wi-Fi", b.ip != null)
    }
}

// =============================================================================
//  Ana ekran: Muzik / Ortam / Ayarlar
// =============================================================================
@Composable
fun AnaEkran(m: AnaModel) {
    val sekme = m.sekme.value
    Scaffold(
        containerColor = Mor,
        topBar = { UstCubuk(m) },
        bottomBar = {
            NavigationBar(containerColor = MorKoyu) {
                val renk = NavigationBarItemDefaults.colors(selectedIconColor = Mor, selectedTextColor = Color.White,
                    indicatorColor = Color.White, unselectedIconColor = Soluk, unselectedTextColor = Soluk)
                NavigationBarItem(sekme == Sekme.MUZIK, { m.sekme.value = Sekme.MUZIK },
                    { Icon(Icons.Default.PlayArrow, null) }, label = { Text("Müzik") }, colors = renk)
                NavigationBarItem(sekme == Sekme.ORTAM, { m.sekme.value = Sekme.ORTAM },
                    { Icon(Icons.Default.Home, null) }, label = { Text("Ortam") }, colors = renk)
                NavigationBarItem(sekme == Sekme.AYARLAR, { m.sekme.value = Sekme.AYARLAR },
                    { Icon(Icons.Default.Settings, null) }, label = { Text("Ayarlar") }, colors = renk)
            }
        },
        floatingActionButton = {
            if (sekme == Sekme.MUZIK && m.tanimBul(AyarId.OTO_SN) != null) {
                FloatingActionButton(onClick = { m.otoDegistir() }, shape = CircleShape,
                    containerColor = if (m.otoAcik) Sari else MorKoyu, contentColor = if (m.otoAcik) SariYazi else Soluk) {
                    Text("Oto", fontWeight = FontWeight.Bold)
                }
            }
        }
    ) { ic ->
        Box(Modifier.padding(ic).fillMaxSize()) {
            when (sekme) {
                Sekme.MUZIK -> ModSekmesi(m, muziksiz = false, "Cihazda müzik modu yok.")
                Sekme.ORTAM -> ModSekmesi(m, muziksiz = true, "Henüz ortam görünümü yok.\nCihaz yazılımı güncellenince burada görünecek.")
                Sekme.AYARLAR -> AyarlarSekmesi(m)
            }
        }
    }
}

@Composable
private fun UstCubuk(m: AnaModel) {
    Column {
        Row(Modifier.fillMaxWidth().padding(horizontal = 6.dp, vertical = 6.dp), verticalAlignment = Alignment.CenterVertically) {
            IconButton(onClick = { m.birak() }) { Icon(Icons.AutoMirrored.Filled.ArrowBack, "Cihazlar", tint = Color.White) }
            Text(m.bilgi.value?.ad ?: "", color = Color.White, fontSize = 17.sp, fontWeight = FontWeight.SemiBold,
                modifier = Modifier.weight(1f), textAlign = TextAlign.Center)
            IconButton(onClick = { m.gucDegistir() }) {
                Box(Modifier.size(40.dp).clip(CircleShape).background(if (m.guc.value) Sari else MorKoyu), contentAlignment = Alignment.Center) {
                    GucSimgesi(if (m.guc.value) SariYazi else Soluk)
                }
            }
        }
        if (m.surumUyari.value) {
            Text("Cihaz yazılımı bu uygulamadan yeni. Bazı özellikler görünmeyebilir; uygulamayı güncelleyin.",
                color = SariYazi, fontSize = 12.sp, modifier = Modifier.fillMaxWidth().background(Sari).padding(10.dp))
        }
    }
}

/** Kaydirmali mod kartlari (resim yerine simdilik bos kart) + o modun ayarlari + parlaklik. */
@Composable
private fun ModSekmesi(m: AnaModel, muziksiz: Boolean, bosYazi: String) {
    // Sekme: cihazin bildirdigi tur (0 muzikli, 1 muziksiz); sira = cihazdaki sira (aileye gore gruplu)
    val liste: List<Mod> = m.modlar.filter { (it.tur == 1) == muziksiz }
    if (liste.isEmpty()) {
        Text(bosYazi, color = Soluk, fontSize = 14.sp, textAlign = TextAlign.Center,
            modifier = Modifier.fillMaxWidth().padding(32.dp))
        return
    }
    val bas = liste.indexOfFirst { it.no == m.aktifMod.value }.coerceAtLeast(0)
    val sayfa = rememberPagerState(initialPage = bas) { liste.size }

    // Kullanici kaydirinca cihazda mod degissin; cihazda (dugmeyle/oto) degisince kart kaysin.
    LaunchedEffect(sayfa.settledPage) { liste.getOrNull(sayfa.settledPage)?.let { m.modSec(it.no) } }
    LaunchedEffect(m.aktifMod.value) {
        val i = liste.indexOfFirst { it.no == m.aktifMod.value }
        if (i >= 0 && i != sayfa.currentPage) sayfa.animateScrollToPage(i)
    }

    val mod = liste[sayfa.settledPage.coerceIn(liste.indices)].no
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState())) {
        HorizontalPager(
            state = sayfa, contentPadding = PaddingValues(horizontal = 48.dp), pageSpacing = 12.dp,
            modifier = Modifier.padding(top = 12.dp)
        ) { i ->
            val on = i == sayfa.currentPage
            Box(
                Modifier.fillMaxWidth().aspectRatio(3f).clip(RoundedCornerShape(16.dp)).background(Color.Black)
                    .border(2.dp, if (on) Sari else Color.Transparent, RoundedCornerShape(16.dp))
            ) {
                // Mod resmi: firmware'in cizim kodundan uretilmis hareketli onizleme (sadece ortadaki kart oynar)
                ModResmi(liste[i].ad, oynat = on, modifier = Modifier.fillMaxSize().padding(horizontal = 10.dp, vertical = 8.dp))
                Text("${i + 1} / ${liste.size}", color = Soluk, fontSize = 12.sp,
                    modifier = Modifier.align(Alignment.TopEnd).padding(12.dp))
            }
        }
        Row(Modifier.fillMaxWidth().padding(vertical = 8.dp), horizontalArrangement = Arrangement.Center) {
            liste.indices.forEach { i ->
                Box(Modifier.padding(3.dp).size(6.dp).clip(CircleShape).background(if (i == sayfa.currentPage) Sari else Color(0xFF8C77E8)))
            }
        }
        Column(Modifier.padding(horizontal = 18.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            m.modAyarlari(mod).forEach { t -> AyarSatiri(t, m.deger(t, mod)) { m.ayarYaz(t, it, mod) } }
            m.tanimBul(AyarId.PARLAKLIK)?.let { t ->
                TekSatir("Parlaklık") {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Box(Modifier.weight(1f)) { Cubuk(m.deger(t, MOD_GENEL), t.min, t.max, t.adim) { m.ayarYaz(t, it, MOD_GENEL) } }
                        Text("%${m.deger(t, MOD_GENEL)}", color = Soluk, fontSize = 12.sp, modifier = Modifier.padding(start = 6.dp))
                    }
                }
            }
            Spacer(Modifier.height(80.dp))      // Oto dugmesinin altinda kalmasin
        }
    }
}

// =============================================================================
//  Ayarlar: cihaz, cihazdan gelen genel ayarlar, baglanti, bakim
// =============================================================================
@Composable
private fun AyarlarSekmesi(m: AnaModel) {
    var adPencere by remember { mutableStateOf(false) }
    var agPencere by remember { mutableStateOf(false) }
    var fabrikaPencere by remember { mutableStateOf(false) }
    val b = m.bilgi.value

    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(18.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Grup("CİHAZ") {
            Satir("Cihaz adı", b?.ad ?: "") { adPencere = true }
            Satir("Saati ayarla", "telefonun saati") { m.saatAyarla() }
            m.tanimBul(AyarId.OTO_SN)?.let { t ->
                Box(Modifier.padding(horizontal = 14.dp, vertical = 2.dp)) {
                    TekSatir("Oto geçiş") {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            val d = m.deger(t, MOD_GENEL)
                            Box(Modifier.weight(1f)) { Cubuk(d, t.min, t.max, t.adim) { m.ayarYaz(t, it, MOD_GENEL) } }
                            Text(if (d == 0) "Kapalı" else "$d sn", color = Soluk, fontSize = 12.sp, modifier = Modifier.padding(start = 6.dp))
                        }
                    }
                }
            }
        }
        // Cihazin bildirdigi diger genel ayarlar (panel modeli, ses kaynagi... firmware ekledikce burada cikar)
        val genel = m.genelAyarlar()
        if (genel.isNotEmpty()) Grup("GENEL") {
            genel.forEach { t -> Box(Modifier.padding(horizontal = 14.dp, vertical = 2.dp)) { AyarSatiri(t, m.deger(t, MOD_GENEL)) { m.ayarYaz(t, it, MOD_GENEL) } } }
        }
        Grup("BAĞLANTI") {
            if (m.kopru.value != null) Satir("Ev ağı (Wi-Fi)", "") { m.wifiDurumSor(); agPencere = true }
            Satir("Yazılım sürümü", "Panel ${b?.fw ?: "-"}" + (m.kopru.value?.let { " · Köprü ${it.surum}" } ?: ""))
        }
        Grup("BAKIM") {
            Satir("Ana yazılım güncelleme", "Yakında", pasif = true)
            Satir("Fabrika ayarları", "") { fabrikaPencere = true }
        }
    }

    if (adPencere) {
        var ad by remember { mutableStateOf(b?.ad ?: "") }
        AlertDialog(onDismissRequest = { adPencere = false },
            title = { Text("Cihaz adı") },
            text = { OutlinedTextField(ad, { if (it.toByteArray().size <= 16) ad = it }, singleLine = true) },
            confirmButton = { TextButton({ m.adVer(ad.trim()); adPencere = false }) { Text("Kaydet") } },
            dismissButton = { TextButton({ adPencere = false }) { Text("Vazgeç") } })
    }
    if (fabrikaPencere) {
        AlertDialog(onDismissRequest = { fabrikaPencere = false },
            title = { Text("Fabrika ayarları") },
            text = { Text("Tüm mod ve renk ayarları ilk haline döner. Emin misiniz?") },
            confirmButton = { TextButton({ m.fabrika(); fabrikaPencere = false }) { Text("Sıfırla") } },
            dismissButton = { TextButton({ fabrikaPencere = false }) { Text("Vazgeç") } })
    }
    if (agPencere) EvAgiPenceresi(m) { agPencere = false }
}


@Composable
private fun EvAgiPenceresi(m: AnaModel, kapat: () -> Unit) {
    var ad by remember { mutableStateOf("") }
    var sifre by remember { mutableStateOf("") }
    LaunchedEffect(Unit) { m.wifiTara() }
    val d = m.wifi.value
    AlertDialog(onDismissRequest = kapat,
        title = { Text("Ev ağı") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(when (d?.durum) {
                    2 -> "Bağlı: ${d?.ad} · ${d?.ip}"
                    1 -> "Bağlanıyor: ${d?.ad}"
                    0 -> "Kayıtlı ev ağı yok"
                    else -> ""
                }, fontSize = 13.sp)
                Column(Modifier.height(180.dp).verticalScroll(rememberScrollState())) {
                    if (m.aglar.isEmpty()) Text("Ağlar aranıyor...", fontSize = 13.sp)
                    m.aglar.forEach { a ->
                        Text("${a.ad}   ${a.rssi} dBm${if (a.sifreli) "  (şifreli)" else ""}",
                            fontSize = 14.sp, modifier = Modifier.fillMaxWidth().clickable { ad = a.ad }.padding(vertical = 8.dp))
                    }
                }
                OutlinedTextField(ad, { ad = it }, label = { Text("Ağ adı") }, singleLine = true)
                OutlinedTextField(sifre, { sifre = it }, label = { Text("Şifre") }, singleLine = true,
                    visualTransformation = PasswordVisualTransformation())
            }
        },
        confirmButton = { TextButton({ if (ad.isNotBlank()) { m.wifiAyarla(ad.trim(), sifre); kapat() } }) { Text("Bağlan") } },
        dismissButton = { TextButton(kapat) { Text("Kapat") } })
}

@Composable
private fun Grup(baslik: String, icerik: @Composable () -> Unit) {
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Text(baslik, color = Soluk, fontSize = 12.sp, fontWeight = FontWeight.SemiBold, modifier = Modifier.padding(start = 4.dp))
        Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(14.dp)).background(MorKart)) { icerik() }
    }
}

@Composable
private fun Satir(ad: String, deger: String, pasif: Boolean = false, tikla: (() -> Unit)? = null) {
    Row(
        Modifier.fillMaxWidth().height(52.dp).let { if (tikla != null && !pasif) it.clickable(onClick = tikla) else it }
            .padding(horizontal = 14.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(ad, color = if (pasif) Pasif else Color.White, fontSize = 15.sp, modifier = Modifier.weight(1f))
        Text(deger, color = if (pasif) Pasif else Soluk, fontSize = 13.sp)
    }
}
