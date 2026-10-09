# PlayStation Go

**An Android-first Flutter frontend and native integration prototype for the PSG
emulation project.**

PlayStation Go combines a Flutter application shell with Android/Kotlin platform
integration and a C++ native layer. The current home dashboard presents device
and controller status plus setup guidance. The project is under active
development and this repository does not include the console core libraries or
BIOS files needed to run games.

## Download the Android test APK

### [⬇️ Download PSG APK](https://github.com/Clone80-c9/Playstation-go/releases/download/android-test-build/app-debug.apk)
| [View release details](https://github.com/Clone80-c9/Playstation-go/releases/tag/android-test-build)

Open the download link in Chrome on your Android phone, allow Chrome to install
unknown apps if Android prompts you, then open the downloaded APK and tap
**Install**. The APK is a debug-signed test build, not a production release.
If Android reports a signature conflict while updating, uninstall the previous
test build first; uninstalling removes its app data.

The download is published by GitHub Actions for pushes to `main` and
`fix/android-release-build`. If the link has no APK yet, the workflow has not
completed its first successful run; check the repository's
[Actions](https://github.com/Clone80-c9/Playstation-go/actions) page.

> [!NOTE]
> The app targets `arm64-v8a` and its native bridge is compiled for ARMv8.2-A,
> which is suitable for Snapdragon 855 (Kryo 485) and newer ARM64 devices.
> This is a CPU/ABI target, not a guarantee of game compatibility or speed.
> The APK does not contain an emulator core: actual gameplay still requires a
> core built for Android ARM64 and the PSG ABI documented below.

> [!IMPORTANT]
> This is a development preview, not a game-ready emulator or a production
> release. The downloadable debug APK is for testing only. The release build
> produced locally is **unsigned**; production distribution still requires a
> maintainer-controlled signing key and real-device validation. No compatible
> emulator cores are bundled.

## Project profile

| | |
| --- | --- |
| **Platform** | Android |
| **Application ID** | `com.psg.app` |
| **Flutter / Dart** | Flutter 3.19.x / Dart 3.3.x |
| **Android minimum** | Android 8.0 (API 26) |
| **Android target / compile SDK** | API 34 |
| **Native ABI** | `arm64-v8a` only |
| **Native CPU target** | ARMv8.2-A (Snapdragon 855 / Kryo 485 class and newer) |
| **Application version** | `1.0.0+1` |
| **Status** | Early development / preview |

## What is in the repository

- Flutter app entry point, splash screen, and a preview dashboard with Android
  controller discovery, private game/core/BIOS import, and a native game view.
- Kotlin platform code for game/core/BIOS imports, native surface lifecycle,
  controller input, and thermal monitoring.
- C++ sources for the native bridge, input, renderer, audio, and thermal
  components.
- Flutter method-channel interfaces connecting Dart UI code to Android.
- Android Bluetooth/controller permissions and ARM64 native build configuration.

The native bridge loads versioned PSG-compatible cores, launches a game into an
Android native surface, passes controller snapshots, and exposes audio host
callbacks. The player offers touch controls and supports physical gamepads. The
repository does not ship a compatible console core, so this integration alone
is not a playable emulator.

### Important runtime requirements

- Core libraries are not included. The setup notes refer to libraries expected
  under `android/app/src/main/jniLibs/arm64-v8a/` at build time and under the
  app-private `files/cores/` directory at runtime.
- BIOS files are not included. Use only core libraries and BIOS files you are
  legally entitled to use.
- The app can import a game file, core `.so`, and BIOS file into its private
  storage using Android's file picker. It cannot make an incompatible core
  compatible with the project's native ABI.
- The dashboard can list imported games and request a launch. Actual emulation
  depends on a compatible native core being installed for that system.
- The core must implement the PSG ABI defined in
  [`android/app/src/main/cpp/include/psg_core_api.h`](android/app/src/main/cpp/include/psg_core_api.h).
  Renaming a standard emulator plugin library does not make its ABI compatible.
- BIOS import and a `Skip BIOS` preference are available. Some cores may still
  have additional BIOS or firmware requirements.

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

### Install the Android test APK

Use the **[Download PSG APK](https://github.com/Clone80-c9/Playstation-go/releases/download/android-test-build/app-debug.apk)**
link above on the Android device. If the direct download is unavailable, open
the [GitHub Releases](https://github.com/Clone80-c9/Playstation-go/releases)
page and confirm that the Android test build has completed.

When prompted, allow Chrome to **Install unknown apps**, open the downloaded
APK, review Android's installer prompt, and choose **Install**. This debug APK
is intended for device testing; production updates require a stable,
maintainer-controlled signing key. Android may require uninstalling an earlier
build if its signing key differs, which removes that app's local data.

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
ARM64 Android device. Game import, controller, thermal, renderer, and core
loading behavior require device testing; a successful APK build alone does not
verify those runtime features.

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
