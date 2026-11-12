package com.spektrum.kontrol

import android.Manifest
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
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
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme(colorScheme = darkColorScheme(primary = Sari, onPrimary = SariYazi, background = Mor, surface = MorKart)) {
                Uygulama(model)
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
