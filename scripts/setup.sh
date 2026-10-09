#!/usr/bin/env bash
set -Eeuo pipefail

readonly FLUTTER_VERSION="3.19.0"
readonly FLUTTER_DIR="${HOME}/flutter"
readonly ANDROID_HOME="${HOME}/android-sdk"
readonly COMMAND_LINE_TOOLS_REVISION="11076708"
readonly COMMAND_LINE_TOOLS_URL="https://dl.google.com/android/repository/commandlinetools-linux-${COMMAND_LINE_TOOLS_REVISION}_latest.zip"
readonly FLUTTER_ARCHIVE_URL="https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_${FLUTTER_VERSION}-stable.tar.xz"
readonly PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly ANDROID_REPOSITORY="${ANDROID_HOME}/cmdline-tools"
readonly SDKMANAGER="${ANDROID_REPOSITORY}/latest/bin/sdkmanager"

log() {
    printf '[PSG setup] %s\n' "$*"
}

fail() {
    printf '[PSG setup] ERROR: %s\n' "$*" >&2
    exit 1
}

command -v java >/dev/null 2>&1 || fail "Java 17 is required. Rebuild the Codespace with the configured Java feature."
command -v sudo >/dev/null 2>&1 || fail "sudo is required to install wget, unzip, and xz-utils."

java_version="$(java -version 2>&1)"
if [[ "${java_version}" != *'"17.'* ]]; then
    fail "Expected Java 17, found: ${java_version%%$'\n'*}"
fi

sudo apt-get update
sudo apt-get install -y wget unzip xz-utils

mkdir -p "${HOME}/.cache/psg-setup" "${ANDROID_HOME}" "${ANDROID_REPOSITORY}"
temporary_directory="$(mktemp -d "${HOME}/.cache/psg-setup/run.XXXXXX")"
cleanup() {
    rm -rf -- "${temporary_directory}"
}
trap cleanup EXIT

if [[ ! -x "${FLUTTER_DIR}/bin/flutter" ]]; then
    log "Downloading Flutter ${FLUTTER_VERSION} stable."
    wget --https-only --show-progress \
        --output-document="${temporary_directory}/flutter.tar.xz" \
        "${FLUTTER_ARCHIVE_URL}"
    tar -xJf "${temporary_directory}/flutter.tar.xz" -C "${HOME}"
fi

[[ -x "${FLUTTER_DIR}/bin/flutter" ]] ||
    fail "Flutter archive extraction did not create ${FLUTTER_DIR}/bin/flutter."

installed_flutter_version="$("${FLUTTER_DIR}/bin/flutter" --version --machine)"
if ! grep -Eq \
    "\"frameworkVersion\"[[:space:]]*:[[:space:]]*\"${FLUTTER_VERSION}\"" \
    <<< "${installed_flutter_version}"; then
    fail "Expected Flutter ${FLUTTER_VERSION}, but found: ${installed_flutter_version}"
fi

log "Downloading Android command-line tools."
wget --https-only --show-progress \
    --output-document="${temporary_directory}/command-line-tools.zip" \
    "${COMMAND_LINE_TOOLS_URL}"
mkdir -p "${temporary_directory}/command-line-tools"
unzip -q "${temporary_directory}/command-line-tools.zip" \
    -d "${temporary_directory}/command-line-tools"
[[ -x "${temporary_directory}/command-line-tools/cmdline-tools/bin/sdkmanager" ]] ||
    fail "Android command-line tools archive did not contain sdkmanager."
mkdir -p "${ANDROID_REPOSITORY}/latest"
cp -a "${temporary_directory}/command-line-tools/cmdline-tools/." \
    "${ANDROID_REPOSITORY}/latest/"
[[ -x "${SDKMANAGER}" ]] || fail "sdkmanager was not installed at ${SDKMAN}."

add_shell_line() {
    local file="$1"
    local line="$2"
    touch "${file}"
    if ! grep -Fqx -- "${line}" "${file}"; then
        printf '%s\n' "${line}" >> "${file}"
    fi
}

add_shell_line "${HOME}/.bashrc" 'export PATH="$HOME/flutter/bin:$PATH"'
add_shell_line "${HOME}/.bashrc" 'export ANDROID_HOME="$HOME/android-sdk"'
add_shell_line "${HOME}/.bashrc" 'export ANDROID_SDK_ROOT="$ANDROID_HOME"'
add_shell_line "${HOME}/.profile" 'export PATH="$HOME/flutter/bin:$PATH"'
add_shell_line "${HOME}/.profile" 'export ANDROID_HOME="$HOME/android-sdk"'
add_shell_line "${HOME}/.profile" 'export ANDROID_SDK_ROOT="$ANDROID_HOME"'

export PATH="${FLUTTER_DIR}/bin:${ANDROID_REPOSITORY}/latest/bin:${PATH}"
export ANDROID_HOME
export ANDROID_SDK_ROOT="${ANDROID_HOME}"

log "Accepting Android SDK licenses."
set +e
yes | "${SDKMANAGER}" --sdk_root="${ANDROID_HOME}" --licenses
license_status="${PIPESTATUS[1]}"
set -e
if [[ "${license_status}" -ne 0 ]]; then
    fail "sdkmanager could not accept Android SDK licenses (exit ${license_status})."
fi

log "Installing Android platform tools, API 34, NDK r25c, and CMake."
"${SDKMANAGER}" --sdk_root="${ANDROID_HOME}" \
    "platform-tools" \
    "platforms;android-34" \
    "build-tools;34.0.0" \
    "ndk;25.2.9519653" \
    "cmake;3.22.1"

printf 'sdk.dir=%s\nflutter.sdk=%s\n' \
    "${ANDROID_HOME}" \
    "${FLUTTER_DIR}" \
    > "${PROJECT_ROOT}/android/local.properties"

log "Running Flutter doctor."
"${FLUTTER_DIR}/bin/flutter" doctor

log "Resolving Flutter dependencies."
cd "${PROJECT_ROOT}"
"${FLUTTER_DIR}/bin/flutter" pub get

printf 'PSG Dev Environment Ready\n'
