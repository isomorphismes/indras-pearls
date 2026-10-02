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

val useIckArmv7 = providers.gradleProperty("ickArmv7")
    .map { it.toBoolean() }
    .orElse(false)
    .get()
val ickMobiusObject = providers.gradleProperty("ickMobiusObject").orNull
val ickRendererPacketObject = providers.gradleProperty("ickRendererPacketObject").orNull

if (useIckArmv7) {
    require(!ickMobiusObject.isNullOrBlank()) {
        "-PickMobiusObject=/absolute/path/to/mobius_math.o is required with -PickArmv7=true"
    }
    require(!ickRendererPacketObject.isNullOrBlank()) {
        "-PickRendererPacketObject=/absolute/path/to/renderer_packet.o is required with -PickArmv7=true"
    }
}

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
        versionName = "0.0.1"

        ndk {
            if (useIckArmv7) {
                abiFilters += listOf("armeabi-v7a")
            } else {
                abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86_64")
            }
        }

        externalNativeBuild {
            cmake {
                arguments += listOf("-DANDROID_STL=none")
                if (useIckArmv7) {
                    arguments += listOf(
                        "-DICK_ARMV7_OBJECTS=ON",
                        "-DICK_MOBIUS_OBJECT=${file(ickMobiusObject!!).absolutePath}",
                        "-DICK_RENDERER_PACKET_OBJECT=${file(ickRendererPacketObject!!).absolutePath}"
                    )
                }
            }
        }
    }

    buildTypes {
        getByName("debug") {
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
