package com.spektrum.kontrol

import android.Manifest
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import com.spektrum.kontrol.ui.Mor
import com.spektrum.kontrol.ui.MorKart
import com.spektrum.kontrol.ui.Sari
import com.spektrum.kontrol.ui.SariYazi
import com.spektrum.kontrol.ui.Uygulama

class AnaAktivite : ComponentActivity() {
    private val model: AnaModel by viewModels()

    private val izinIste = registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
        model.taramaBaslat()     // izin verilmese de ev agi aramasi calisir
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        HataKaydi.kur(this)
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme(colorScheme = darkColorScheme(primary = Sari, onPrimary = SariYazi, background = Mor, surface = MorKart)) {
                Uygulama(model)

                // Onceki calismada cokme olduysa raporu goster
                var rapor by remember { mutableStateOf(HataKaydi.oku(this@AnaAktivite)) }
                rapor?.let { r ->
                    val kapat = { HataKaydi.sil(this@AnaAktivite); rapor = null }
                    AlertDialog(
                        onDismissRequest = kapat,
                        title = { Text("Hata raporu") },
                        text = { Text("Uygulama son kullanımda beklenmedik şekilde kapandı. Raporu kopyalayıp gönderirseniz sorun düzeltilir.") },
                        confirmButton = {
                            TextButton({
                                val pano = getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                                pano.setPrimaryClip(ClipData.newPlainText("Spektrum hata raporu", r))
                                Toast.makeText(this@AnaAktivite, "Rapor kopyalandı", Toast.LENGTH_SHORT).show()
                                kapat()
                            }) { Text("Kopyala") }
                        },
                        dismissButton = {
                            TextButton({
                                startActivity(Intent.createChooser(
                                    Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_TEXT, r),
                                    "Hata raporunu gönder"))
                                kapat()
                            }) { Text("Paylaş") }
                        }
                    )
                }
            }
        }
        if (savedInstanceState == null) {
            izinIste.launch(
                if (Build.VERSION.SDK_INT >= 31) arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
                else arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
            )
        }
    }
}
