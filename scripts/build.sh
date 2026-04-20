#!/bin/bash
set -e

SDK_ROOT="${SDK_ROOT:-/opt/fslc-xwayland/5.15-scarthgap}"
SDK_ENV="$SDK_ROOT/environment-setup-cortexa53-fslc-linux"
SDK_CMAKE="$SDK_ROOT/sysroots/x86_64-fslcsdk-linux/usr/bin/cmake"
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

usage() {
    cat <<'EOF'
Usage: build.sh <target> [--x509] [--speed]

Targets:
  debug       Cross-compile Debug build (default)
  release     Cross-compile Release build (-Os, LTO)
  sanitize    Cross-compile Debug build with ASan + UBSan
  test        Native build + run unit tests
  clean       Remove all build directories

Options:
  --x509      Enable X.509 certificate store (Azure DU)
  --speed     Optimize for speed (-O2) instead of size (-Os)
EOF
    exit 1
}

# X.509 certificate store CMake arguments
X509_CMAKE_ARGS=(
    -DBUILD_X509_CERTIFICATE_STORE_MOUNT=ON
    -DTARGET_ARCHIV_DIR_PATH=/etc/adu/x509_c
    -DFUS_AZURE_CONFIGURATION=/etc/adu/du-config.json
    -DPART_NAME_MTD_CERT=Secure
    -DEMMC_SECURE_PART_BLK_NR=22528
)

TARGET=""
EXTRA_ARGS=()

for arg in "$@"; do
    case "$arg" in
    --x509) EXTRA_ARGS+=("${X509_CMAKE_ARGS[@]}") ;;
    --speed) EXTRA_ARGS+=("-DOPTIMIZE_FOR=SPEED") ;;
    debug | release | sanitize | test | clean)
        if [ -n "$TARGET" ]; then
            echo "Multiple targets specified: $TARGET and $arg"
            usage
        fi
        TARGET="$arg"
        ;;
    *)
        echo "Unknown option: $arg"
        usage
        ;;
    esac
done

TARGET="${TARGET:-debug}"

build_cross() {
    local build_dir="$PROJECT_ROOT/build"
    local cmake_args=("$@")

    unset LD_LIBRARY_PATH
    source "$SDK_ENV"

    mkdir -p "$build_dir" && cd "$build_dir"
    "$SDK_CMAKE" "${cmake_args[@]}" "$PROJECT_ROOT"
    make -j"$(nproc)"
}

build_test() {
    local build_dir="$PROJECT_ROOT/build_test"
    local sdk_bin="$SDK_ROOT/sysroots/x86_64-fslcsdk-linux/usr/bin"

    mkdir -p "$build_dir" && cd "$build_dir"
    "$sdk_bin/cmake" -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_C_COMPILER=gcc \
        -DCMAKE_CXX_COMPILER=g++ \
        -DENABLE_LOGGING=ON \
        -DBUILD_TESTING=ON \
        -DBUILD_MAIN_TARGET=OFF \
        "$PROJECT_ROOT"
    make -j"$(nproc)"
    "$sdk_bin/ctest" --output-on-failure
}

case "$TARGET" in
debug)
    build_cross -DCMAKE_BUILD_TYPE=Debug -DENABLE_LOGGING=ON "${EXTRA_ARGS[@]}"
    ;;
release)
    build_cross -DCMAKE_BUILD_TYPE=Release -DENABLE_LOGGING=ON "${EXTRA_ARGS[@]}"
    ;;
sanitize)
    build_cross -DCMAKE_BUILD_TYPE=Debug -DENABLE_LOGGING=ON -DENABLE_SANITIZERS=ON "${EXTRA_ARGS[@]}"
    ;;
test)
    build_test
    ;;
clean)
    rm -rf "$PROJECT_ROOT"/build "$PROJECT_ROOT"/build_test
    echo "Build directories removed."
    ;;
*)
    usage
    ;;
esac
