cmake_minimum_required(VERSION 3.22.1)
project(psg_core VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(ANDROID AND NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "PSG Core supports arm64-v8a only")
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=armv8.2-a -mtune=cortex-a76 -ffast-math -funroll-loops -fomit-frame-pointer")
set(CMAKE_C_FLAGS_RELEASE "-O3 -march=armv8.2-a -mtune=cortex-a76 -ffast-math -funroll-loops -fomit-frame-pointer")

add_library(psg_bridge SHARED
    src/main/cpp/psg_bridge.cpp
    src/main/cpp/psg_renderer.cpp
    src/main/cpp/psg_audio.cpp
    src/main/cpp/psg_input.cpp
    src/main/cpp/thermal_monitor.cpp
)

find_library(log-lib log REQUIRED)
find_library(android-lib android REQUIRED)
find_library(egl-lib EGL REQUIRED)
find_library(glesv3-lib GLESv3 REQUIRED)
find_library(vulkan-lib vulkan REQUIRED)
find_library(opensles-lib OpenSLES REQUIRED)
find_library(aaudio-lib aaudio REQUIRED)
find_library(mediandk-lib mediandk REQUIRED)

target_link_libraries(psg_bridge
    ${log-lib}
    ${android-lib}
    ${egl-lib}
    ${glesv3-lib}
    ${vulkan-lib}
    ${opensles-lib}
    ${aaudio-lib}
    ${mediandk-lib}
)

# External core libraries — loaded dynamically at runtime, not linked at build time
# Drop compiled .so files into: android/app/src/main/jniLibs/arm64-v8a/
# PS1: libpcsx_rearmed.so
# PS2: libplay.so
# PS3: librpcs3_lite.so (experimental)
# PS4: libspine_lite.so (experimental)
