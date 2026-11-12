plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

android {
    namespace = "com.spektrum.kontrol"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.spektrum.kontrol"
        minSdk = 24          // Android 7.0
        targetSdk = 35
        // GitHub'da her derleme numarasi artar: telefon "Guncelle" der
        val no = System.getenv("GITHUB_RUN_NUMBER")?.toIntOrNull() ?: 1
        versionCode = no
        versionName = "0.1.$no"
    }
    // Sabit imza: her APK ayni anahtarla imzalanir, eskisinin ustune kurulur (depo gizli)
    signingConfigs {
        create("sabit") {
            storeFile = file("imza.jks")
            storePassword = "spektrum"
            keyAlias = "spektrum"
            keyPassword = "spektrum"
        }
    }
    buildTypes {
        debug { signingConfig = signingConfigs.getByName("sabit") }
        release { isMinifyEnabled = false; signingConfig = signingConfigs.getByName("sabit") }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }
    buildFeatures { compose = true }
}

dependencies {
    val bom = platform("androidx.compose:compose-bom:2024.12.01")
    implementation(bom)
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.foundation:foundation")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.material:material-icons-core")
    implementation("androidx.activity:activity-compose:1.9.3")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.8.7")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.8.7")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.8.7")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.9.0")
}
