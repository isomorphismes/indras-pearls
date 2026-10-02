plugins {
    id("com.android.application")
}

val stableTestKeystorePath = providers.environmentVariable("INDRAS_TEST_KEYSTORE").orNull
val stableTestKeystorePassword = providers.environmentVariable("INDRAS_TEST_KEYSTORE_PASSWORD").orNull
    ?: "wegert-debug"
val stableTestKeyPassword = providers.environmentVariable("INDRAS_TEST_KEY_PASSWORD").orNull
    ?: stableTestKeystorePassword
val stableTestKeyAlias = providers.environmentVariable("INDRAS_TEST_KEY_ALIAS").orNull
    ?: "wegert-debug"

android {
    namespace = "org.isomorphisms.indraspearls"
    compileSdk = 36
    ndkVersion = "29.0.14206865"

    signingConfigs {
        stableTestKeystorePath?.let { keystorePath ->
            create("stableTest") {
                storeFile = rootProject.file(keystorePath)
                storePassword = stableTestKeystorePassword
                keyAlias = stableTestKeyAlias
                keyPassword = stableTestKeyPassword
                storeType = "pkcs12"
            }
        }
    }

    defaultConfig {
        applicationId = "org.isomorphisms.indraspearls"
        minSdk = 26
        targetSdk = 36
        versionCode = 1
        versionName = "0.0.1-miro-a1"

        ndk {
            abiFilters += listOf("armeabi-v7a")
        }

        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DANDROID_STL=none",
                    "-DINDRAS_MIRO_A1=ON"
                )
            }
        }
    }

    buildTypes {
        getByName("debug") {
            // Never fall back to a machine-local debug certificate. CI supplies
            // the shared stable test signer; without it this build stays unsigned.
            signingConfig = signingConfigs.findByName("stableTest")
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}
