import java.security.MessageDigest
import javax.inject.Inject
import org.gradle.api.DefaultTask
import org.gradle.api.file.DirectoryProperty
import org.gradle.api.file.FileSystemOperations
import org.gradle.api.tasks.InputDirectory
import org.gradle.api.tasks.OutputDirectory
import org.gradle.api.tasks.PathSensitive
import org.gradle.api.tasks.PathSensitivity
import org.gradle.api.tasks.TaskAction

plugins {
    alias(libs.plugins.android.application)
}

val surgeDataSourceDir = layout.projectDirectory.dir("../../resources/data")
val generatedSurgeAssetsRoot = layout.buildDirectory.dir("generated/surge-assets")

abstract class StageSurgeAssetsTask : DefaultTask() {
    @get:InputDirectory
    @get:PathSensitive(PathSensitivity.RELATIVE)
    abstract val sourceDir: DirectoryProperty

    @get:OutputDirectory
    abstract val outputDir: DirectoryProperty

    @get:Inject
    abstract val fileSystemOperations: FileSystemOperations

    @TaskAction
    fun stage() {
        val sourceRoot = sourceDir.get().asFile
        val outputRoot = outputDir.get().asFile
        val dataOutput = outputRoot.resolve("SurgeXTData")

        fileSystemOperations.delete {
            delete(outputRoot)
        }
        fileSystemOperations.copy {
            from(sourceRoot)
            into(dataOutput)
        }

        val digest = MessageDigest.getInstance("SHA-256")
        sourceRoot.walkTopDown()
            .filter { it.isFile }
            .sortedBy { it.relativeTo(sourceRoot).invariantSeparatorsPath }
            .forEach { file ->
                digest.update(file.relativeTo(sourceRoot).invariantSeparatorsPath.toByteArray(Charsets.UTF_8))
                digest.update(0)
                file.inputStream().use { input ->
                    val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
                    while (true) {
                        val count = input.read(buffer)
                        if (count < 0) {
                            break
                        }
                        digest.update(buffer, 0, count)
                    }
                }
                digest.update(0)
            }

        val hash = digest.digest().joinToString("") { "%02x".format(it) }
        dataOutput.resolve("surge-xt-assets-version.txt").writeText("$hash\n")
    }
}

val stageSurgeAssets by tasks.registering(StageSurgeAssetsTask::class) {
    description = "Stages Surge factory assets for Android packaging"
    sourceDir.set(surgeDataSourceDir)
    outputDir.set(generatedSurgeAssetsRoot)
}

android {
    namespace = "org.vicwomg.surge_xt"
    compileSdk {
        version = release(36) {
            minorApiLevel = 1
        }
    }

    defaultConfig {
        applicationId = "org.vicwomg.surge_xt"
        minSdk = 29
        targetSdk = 36
        versionCode = 1
        versionName = "1.0"
        ndk {
            abiFilters += listOf("arm64-v8a")
        }
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        externalNativeBuild {
            cmake {
                arguments(
                    "-DANDROID_PLATFORM=android-29",
                    "-DSURGE_SKIP_DISTRIBUTION=TRUE",
                    "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
                )
                abiFilters("arm64-v8a")
                targets("surge-xt_Standalone")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            signingConfig = signingConfigs.getByName("debug")
            externalNativeBuild {
                cmake {
                    cppFlags("-O3", "-fomit-frame-pointer", "-flto")
                }
            }
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }

    externalNativeBuild {
        cmake {
            path = file("../../CMakeLists.txt")
        }
    }
    ndkVersion = "28.2.13676358"
}

androidComponents {
    onVariants(selector().all()) { variant ->
        variant.sources.assets?.addGeneratedSourceDirectory(stageSurgeAssets) {
            it.outputDir
        }
    }
}

dependencies {
    testImplementation(libs.junit)
    androidTestImplementation(libs.espresso.core)
    androidTestImplementation(libs.ext.junit)
}
