# PSG Core Engine setup

## Open the project in Android Studio

1. Install Android Studio and the Flutter plugin from **Settings/Preferences > Plugins**.
2. Open the repository root (`Playstation-go`) in Android Studio.
3. If Android Studio asks for an SDK location, use **Tools > SDK Manager** to install Android SDK Platform 34 and Android SDK Build-Tools 34.
4. Sync the Gradle project, then choose the `app` run configuration and an Android device.

This Android app uses `FlutterActivity` and the Flutter Gradle plugin. Run `flutter pub get` from the repository root before the first build so Flutter generates its local SDK configuration. The Android Gradle settings and plugin-management files are part of the scaffold.

## Install Android NDK r25c

NDK r25c is version `25.2.9519653`.

1. In Android Studio, open **Tools > SDK Manager > SDK Tools**.
2. Enable **Show Package Details**.
3. Expand **NDK (Side by side)** and select version **25.2.9519653**.
4. Apply the changes. The app Gradle configuration pins this NDK version.
5. In **SDK Tools**, install **CMake 3.22.1** if it is not already installed.

## Install core libraries and BIOS files

Build-time ARM64 core libraries belong in:

```text
android/app/src/main/jniLibs/arm64-v8a/
```

Use these exact filenames:

| Console | Core library |
| --- | --- |
| PS1 | `libpcsx_rearmed.so` |
| PS2 | `libplay.so` |
| PS3 | `librpcs3_lite.so` |
| PS4 | `libspine_lite.so` |

The current `loadCore` bridge looks for the selected core under the app's private files directory, not directly in `jniLibs`. At runtime, the core must therefore be installed/copied to:

```text
/data/user/0/com.psg.app/files/cores/
```

Expected runtime paths are `/data/user/0/com.psg.app/files/cores/libpcsx_rearmed.so`, `libplay.so`, `librpcs3_lite.so`, and `libspine_lite.so`. Install them through an app-side core installer or another mechanism that can write to the app-private directory; the Android shell normally cannot write there directly.

Place BIOS files in the app-private directory:

```text
/data/user/0/com.psg.app/files/bios/scph1001.bin
/data/user/0/com.psg.app/files/bios/ps2-0230e-20080220.bin
```

The PS1 and PS2 BIOS files are required when **Skip BIOS** is disabled. The bridge currently looks for `ps3.bin` and `ps4.bin` for those consoles if BIOS use is enabled. Only use BIOS files and core libraries you are legally entitled to use.

## Enable USB debugging on a Samsung Galaxy S10+

1. On the phone, open **Settings > About phone > Software information**.
2. Tap **Build number** seven times; enter the device PIN if prompted.
3. Return to **Settings**, open **Developer options**, and enable **USB debugging**.
4. Connect the phone to the development computer with a data-capable USB cable and approve the RSA authorization prompt on the phone.
5. Confirm the device is visible with `adb devices`. Select it in Android Studio and run the `app` configuration.

The app targets Android 14 (API 34) and packages only the `arm64-v8a` ABI.

## Check logs

In Android Studio, open **View > Tool Windows > Logcat**, select the S10+, and filter by `PSG_CORE`, `PSGEmulatorBridge`, or `PSGMainActivity`. From a terminal, use:

```sh
adb logcat -s PSG_CORE:I PSGEmulatorBridge:I PSGMainActivity:I PSGThermalManager:W
```

After the app starts, expect:

```text
PSGEmulatorBridge: Loaded native library psg_bridge
```

When a core is loaded successfully, native code logs:

```text
PSG_CORE: Core loaded: /data/user/0/com.psg.app/files/cores/libplay.so
```

The path suffix identifies the library that was loaded. A failed load emits `Unable to load core ...` or a missing-core-entry-point error instead.

## Confirm thermal monitoring

Call `startMonitoring` on the `com.psg.emulator/thermal` channel. On the first reading and whenever the tier changes, expect a message similar to:

```text
PSGMainActivity: Thermal tier changed to NORMAL at 35.0 C
```

The manager samples every three seconds but only emits the callback/log when the thermal tier changes. If hardware and sysfs readings both fail, it logs a warning and uses the safe `35.0 C` default.

## Common build errors

| Error | Fix |
| --- | --- |
| Android SDK platform 34 not found | Install **Android SDK Platform 34** from SDK Manager and sync Gradle again. |
| NDK `25.2.9519653` or CMake `3.22.1` not found | Install those exact versions from **SDK Manager > SDK Tools > Show Package Details**. |
| Minimum supported Gradle version is 8.0 | Keep the Gradle wrapper at 8.0 or newer to match Android Gradle Plugin 8.1.0. |
| Removing unused resources requires code shrinking | Enable `minifyEnabled` for release builds, or set `shrinkResources false` when code shrinking is intentionally disabled. |
| `flutter_blue_plus_android` cannot resolve `flutter.compileSdkVersion` | Use the Flutter Blue Plus 1.34.0 version pinned in `pubspec.yaml`, which is compatible with this Flutter SDK. |
| `No matching variant` or Flutter Gradle plugin cannot be resolved | Open the Flutter project root, install/configure the Flutter SDK, run `flutter pub get`, and sync Gradle so the Flutter Gradle plugin is resolved. |
| `psg_input.cpp` or another native source missing | Verify the source exists at `android/app/src/main/cpp/` and that the CMake source path matches its filename. |
| `dlopen failed` or `wrong ELF class` | Use a 64-bit ARM core (`arm64-v8a`) and verify the library filename and required `psg_core_*` exports. |
| Core is not found after packaging | Ensure the app-side installer has copied the core into `/data/user/0/com.psg.app/files/cores/`; the native loader does not load directly from `jniLibs`. |
| `UnsatisfiedLinkError: psg_bridge` | Build/install the app for `arm64-v8a`, confirm CMake succeeded, and check Logcat for the underlying native library load error. |
| Controller does not connect over Bluetooth | Grant nearby-device Bluetooth permissions when prompted and pair the controller in Android Bluetooth settings. |
