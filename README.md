# PlayStation Go

**An Android-first Flutter frontend and native integration prototype for the PSG
emulation project.**

PlayStation Go combines a Flutter application shell with Android/Kotlin platform
integration and a C++ native layer. The project is under active development: the
current home screen is a placeholder, and this repository does not include the
console core libraries or BIOS files needed to run games.

> [!IMPORTANT]
> This is a development preview, not a game-ready emulator or a production
> release. The APK currently produced by the release build is **unsigned** and
> cannot be installed as a normal Android app. A signed release and a
> device-side installation/management flow for legally obtained cores and BIOS
> files are still required before public distribution.

## Project profile

| | |
| --- | --- |
| **Platform** | Android |
| **Application ID** | `com.psg.app` |
| **Flutter / Dart** | Flutter 3.19.x / Dart 3.3.x |
| **Android minimum** | Android 8.0 (API 26) |
| **Android target / compile SDK** | API 34 |
| **Native ABI** | `arm64-v8a` only |
| **Application version** | `1.0.0+1` |
| **Status** | Early development / preview |

## What is in the repository

- Flutter app entry point, splash screen, and a placeholder home screen.
- Kotlin platform code for emulator/core operations, controller input, and
  thermal monitoring.
- C++ sources for the native bridge, input, renderer, audio, and thermal
  components.
- Flutter method-channel interfaces connecting Dart UI code to Android.
- Android Bluetooth/controller permissions and ARM64 native build configuration.

The native bridge has interfaces for loading cores, launching a game, renderer
setup, controller input, performance information, and save-state operations.
These interfaces do not mean that the project currently ships working console
cores or a complete playable frontend.

### Important runtime requirements

- Core libraries are not included. The setup notes refer to libraries expected
  under `android/app/src/main/jniLibs/arm64-v8a/` at build time and under the
  app-private `files/cores/` directory at runtime.
- BIOS files are not included. Use only core libraries and BIOS files you are
  legally entitled to use.
- The repository does not yet provide a complete in-app core/BIOS installation
  flow. Adding files to `jniLibs` alone does not install them in the app-private
  directory expected by the current loader.
- The current home screen is intentionally a placeholder. Do not expect to
  browse or launch games from the app yet.

For the detailed native setup and expected core/BIOS paths, see
[SETUP.md](SETUP.md).

## Build from source

### Requirements

Install the following tools before building:

1. **Flutter 3.19.x** with its bundled Dart SDK (3.3.x). Confirm with
   `flutter --version`.
2. **Java Development Kit 17**. Confirm with `java -version`.
3. **Android SDK Platform 34** and Android SDK Build-Tools 34.
4. **Android NDK `25.2.9519653`** and **CMake `3.22.1`** from Android Studio's
   SDK Manager (enable **Show Package Details** to select exact versions).
5. An `arm64-v8a` Android device or emulator for device testing.

The repository's Gradle wrapper is pinned to Gradle 8.0.2. The Android
application compiles both Java and Kotlin to JVM target 17.

### Build an APK

From the repository root:

```sh
flutter doctor
flutter pub get
flutter build apk --release --target-platform android-arm64
```

The APK is written to:

```text
build/app/outputs/flutter-apk/app-release.apk
```

This project currently builds only the `arm64-v8a` architecture. The native
build uses the NDK and CMake versions listed above. Release code minification
and resource shrinking are disabled in the current Android configuration.

> [!WARNING]
> The repository does not configure a release signing key. The APK produced by
> the command above is unsigned and is not suitable for sideloading or public
> distribution as-is. Do not publish an unsigned APK or commit a signing
> keystore, passwords, or signing credentials to the repository. A maintainer
> must configure a private release key before creating a downloadable release.

### Run a development build on a device

For local development and smoke testing, connect an Android device with USB
debugging enabled or start an Android emulator, then run:

```sh
flutter devices
flutter run
```

Alternatively, use Android Studio with the Flutter plugin and run the `app`
configuration. A debug build is for development; it is not a production release.

## Install an APK

### Install a signed APK from a future GitHub Release

Once a signed APK has been published, open the repository's
[GitHub Releases](https://github.com/Clone80-c9/Playstation-go/releases) page on
the Android device and download the APK asset for the release. In Android
settings, allow the browser or file manager to **Install unknown apps** if
prompted, open the downloaded APK, review the installer prompt, and choose
**Install**.

The signed APK must have a valid signature. Android may block installation if
the app is already installed with a different signing key; uninstalling first
will remove that app's local data. Updates must continue to use the same
maintainer-controlled signing key.

### Install a signed APK using ADB

With Android Platform Tools installed and USB debugging authorized:

```sh
adb devices
adb install -r path/to/signed-app-release.apk
```

The APK presently produced by this repository is unsigned, so use a signed APK
for these installation steps. Do not try to bypass Android's signature checks.

## Testing and verification

Run the Flutter analyzer and available Dart tests with:

```sh
flutter analyze
flutter test
```

Build the Android release APK with the command in [Build from source](#build-from-source).
After a signed build is available, verify installation and launch on a physical
ARM64 Android device. Hardware-dependent controller, thermal, renderer, and core
loading behavior should be tested on target devices; a successful APK build
alone does not verify those runtime features.

## Repository layout

```text
lib/                              Flutter application and platform channels
lib/screens/                      Splash and home screens
android/app/src/main/java/        Kotlin Android integration
android/app/src/main/cpp/         C++ native bridge and engine components
android/app/src/main/AndroidManifest.xml
                                  Android permissions and app declaration
SETUP.md                          Native core/BIOS and Android setup notes
```

## Contributing

Issues and focused pull requests are welcome. Please include the Android
device/API level and relevant `flutter doctor` or Logcat output when reporting a
build or runtime problem. Do not attach proprietary BIOS files, commercial
games, private signing keys, or credentials.

## Legal note

PlayStation Go is an independent project. PlayStation and related names are
trademarks of their respective owners. This repository does not distribute
commercial games, BIOS images, or third-party emulator core binaries. Obtain
and use any such materials only where permitted by applicable law.
