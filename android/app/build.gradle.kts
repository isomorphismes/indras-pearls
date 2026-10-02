plugins {
    id("com.android.application")
}

android {
    namespace = "org.isomorphisms.indraspearls"
    compileSdk = 36
    ndkVersion = "29.0.14206865"

    val stableKeystore = System.getenv("ANDROID_KEYSTORE")
    val miroA1 = providers.gradleProperty("miroA1").orNull == "true"
    if (stableKeystore != null) {
        signingConfigs {
            create("stableTest") {
                storeFile = file(stableKeystore)
                storePassword = System.getenv("ANDROID_KEYSTORE_PASSWORD") ?: "wegert-debug"
                keyAlias = System.getenv("ANDROID_KEY_ALIAS") ?: "wegert-debug"
                keyPassword = System.getenv("ANDROID_KEY_PASSWORD") ?: storePassword
            }
        }
    }

    defaultConfig {
        applicationId = "org.isomorphisms.indraspearls"
        minSdk = 26
        targetSdk = 36
        versionCode = 1
        versionName = if (miroA1) "0.0.1-miro-a1" else "0.0.1"

        ndk {
            abiFilters += if (miroA1) listOf("armeabi-v7a") else listOf("arm64-v8a", "armeabi-v7a", "x86_64")
        }

        externalNativeBuild {
            cmake {
                arguments += listOf("-DANDROID_STL=none")
                if (miroA1) {
                    arguments += "-DINDRAS_MIRO_A1=ON"
                }
            }
        }
    }

    buildTypes {
        getByName("debug") {
            if (stableKeystore != null) {
                signingConfig = signingConfigs.getByName("stableTest")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
}
