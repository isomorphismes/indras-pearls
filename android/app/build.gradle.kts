plugins {
    id("com.android.application")
}

val useIckArmv7 = providers.gradleProperty("ickArmv7")
    .map { it.toBoolean() }
    .orElse(false)
    .get()
val ickOriginalKleinianObject = providers.gradleProperty("ickOriginalKleinianObject").orNull

if (useIckArmv7) {
    require(!ickOriginalKleinianObject.isNullOrBlank()) {
        "-PickOriginalKleinianObject=/absolute/path/to/original_kleinian.o is required with -PickArmv7=true"
    }
}

android {
    namespace = "org.isomorphisms.indraspearls"
    compileSdk = 36
    ndkVersion = "29.0.14206865"

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
                        "-DICK_ORIGINAL_KLEINIAN_OBJECT=${file(ickOriginalKleinianObject!!).absolutePath}"
                    )
                }
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
