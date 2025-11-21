plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
}

android {
    namespace = "com.adamfull.edge"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.adamfull.edge"
        minSdk = 29
        targetSdk = 35
        versionCode = 1
        versionName = "1.0"

        ndkVersion = "29.0.13113456"

        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DCMAKE_C_FLAGS=-std=c11 -fms-extensions",
                    "-DANDROID_STL=c++_shared",

                    "-D_GNU_SOURCE=1",

                    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",

                    // Build configuration flags
                    "-DBUILD_TOOLS=OFF",
                    "-DBUILD_REGRESS=OFF",
                    "-DBUILD_OSSFUZZ=OFF",
                    "-DBUILD_EXAMPLES=OFF",
                    "-DBUILD_DOC=OFF",
                    "-DBUILD_TESTING=OFF",

                    // Library configuration
                    "-DBUILD_SHARED_LIBS=ON",

                    // mimalloc configuration
                    "-DMI_OVERRIDE=OFF",
                    "-DMI_BUILD_SHARED=OFF",
                    "-DMI_BUILD_TESTS=OFF",

                    // FreeType configuration
                    "-DFT_DISABLE_TESTS=ON",

                    // Install configuration
                    "-DSKIP_INSTALL_ALL=ON",

                    // CURL configuration
                    "-DBUILD_CURL_EXE=OFF",
                    "-DCURL_STATICLIB=ON",
                    "-DCURL_DISABLE_LDAP=ON",
                    "-DCURL_DISABLE_LDAPS=ON",
                    "-DCURL_USE_LIBPSL=OFF",
                    "-DCURL_USE_LIBSSH2=OFF",
                    "-DCURL_ENABLE_SSL=ON",
                    "-DHTTP_ONLY=ON",
                    "-DCURL_DISABLE_COOKIES=OFF",

                    // zlib configuration
                    "-DZLIB_BUILD_TESTING=OFF",

                    // zstd configuration
                    "-DZSTD_BUILD_PROGRAMS=OFF",

                    // Sanitizers (typically OFF for Android release builds)
                    "-DEDGE_USE_ASAN=OFF",
                    "-DEDGE_USE_TSAN=OFF"
                )
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
    kotlinOptions {
        jvmTarget = "11"
    }
    buildFeatures {
        prefab = true
    }
    externalNativeBuild {
        cmake {
            path = file("../../../CMakeLists.txt")
            version = "3.31.6"
        }
    }
    //sourceSets.getByName("main") {
    //    assets.setSrcDirs(listOf("../../assets"))
    //}
}

dependencies {

    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.appcompat)
    implementation(libs.material)
    implementation(libs.androidx.games.activity)
    implementation(libs.androidx.games.controller)
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
}