#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: tools/android-build.sh --ndk <path> [options] [-- <extra configure flags>]

Required arguments:
  --ndk <path>          Absolute path to the Android NDK root.

Options:
  --api <level>         Android API level to target (default: 34).
  --arch <arch>         Target architecture: arm, arm64, aarch64, x86, x86_64 (default: arm64).
  --jobs <n>            Parallel build jobs for make (default: host CPU count).
  --shared              Build libnode as a shared library.
  --configure-only      Run configure step but skip make.
  --apply-patch         Apply android patches before configuring.
  --make-target <name>  Make target to build (default: node).
  --ndk-host-tag <tag>  Override NDK host tag (e.g. linux-x86_64).
  --verbose             Print executed commands (set -x).
  -h, --help            Show this help and exit.

All arguments following "--" are passed directly to ./configure.
EOF
}

if [[ $# -eq 0 ]]; then
  usage
  exit 1
fi

API_LEVEL=34
ARCH="arm64"
JOBS=""
RUN_BUILD=1
BUILD_SHARED=0
APPLY_PATCH=0
MAKE_TARGET="node"
NDK_HOST_TAG=""
VERBOSE=0
USER_CONFIG_FLAGS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --ndk)
      NDK_PATH="$2"
      shift 2
      ;;
    --api)
      API_LEVEL="$2"
      shift 2
      ;;
    --arch)
      ARCH="$2"
      shift 2
      ;;
    --jobs)
      JOBS="$2"
      shift 2
      ;;
    --shared)
      BUILD_SHARED=1
      shift
      ;;
    --configure-only)
      RUN_BUILD=0
      shift
      ;;
    --apply-patch)
      APPLY_PATCH=1
      shift
      ;;
    --make-target)
      MAKE_TARGET="$2"
      shift 2
      ;;
    --ndk-host-tag)
      NDK_HOST_TAG="$2"
      shift 2
      ;;
    --verbose)
      VERBOSE=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --)
      shift
      while [[ $# -gt 0 ]]; do
        USER_CONFIG_FLAGS+=("$1")
        shift
      done
      break
      ;;
    *)
      printf 'Error: unknown option "%s".\n' "$1" >&2
      usage
      exit 1
      ;;
  esac
done
if [[ -z ${NDK_PATH:-} ]]; then
  printf 'Error: --ndk is required.\n' >&2
  usage
  exit 1
fi

if [[ ! -d "$NDK_PATH" ]]; then
  printf 'Error: NDK path "%s" does not exist.\n' "$NDK_PATH" >&2
  exit 1
fi

if ! [[ "$API_LEVEL" =~ ^[0-9]+$ ]]; then
  printf 'Error: --api must be an integer.\n' >&2
  exit 1
fi

if (( API_LEVEL < 24 )); then
  printf 'Error: Android API level must be at least 24.\n' >&2
  exit 1
fi

if [[ -z "$JOBS" ]]; then
  if command -v nproc >/dev/null 2>&1; then
    JOBS="$(nproc)"
  elif [[ "$(uname -s)" == "Darwin" ]]; then
    JOBS="$(sysctl -n hw.ncpu)"
  else
    JOBS=4
  fi
fi

if (( VERBOSE )); then
  set -x
fi

case "$ARCH" in
  arm|armeabi|armeabi-v7a)
    DEST_CPU="arm"
    TOOLCHAIN_TRIPLE="armv7a-linux-androideabi"
    GYP_ARCH="arm"
    ;;
  arm64|aarch64)
    DEST_CPU="arm64"
    TOOLCHAIN_TRIPLE="aarch64-linux-android"
    GYP_ARCH="arm64"
    ;;
  x86)
    DEST_CPU="ia32"
    TOOLCHAIN_TRIPLE="i686-linux-android"
    GYP_ARCH="x86"
    ;;
  x86_64|x64)
    DEST_CPU="x64"
    TOOLCHAIN_TRIPLE="x86_64-linux-android"
    GYP_ARCH="x64"
    ;;
  *)
    printf 'Error: unsupported --arch value "%s".\n' "$ARCH" >&2
    exit 1
    ;;
esac

case "$(uname -s)" in
  Linux)
    HOST_OS="linux"
    DEFAULT_HOST_TAG="linux-x86_64"
    ;;
  Darwin)
    HOST_OS="darwin"
    if [[ -d "$NDK_PATH/toolchains/llvm/prebuilt/darwin-arm64" ]]; then
      DEFAULT_HOST_TAG="darwin-arm64"
    else
      DEFAULT_HOST_TAG="darwin-x86_64"
    fi
    ;;
  *)
    printf 'Error: host OS "%s" is not supported.\n' "$(uname -s)" >&2
    exit 1
    ;;
esac

if [[ -z "$NDK_HOST_TAG" ]]; then
  NDK_HOST_TAG="$DEFAULT_HOST_TAG"
fi

TOOLCHAIN_PATH="$NDK_PATH/toolchains/llvm/prebuilt/$NDK_HOST_TAG"
if [[ ! -d "$TOOLCHAIN_PATH" ]]; then
  printf 'Error: toolchain directory "%s" not found.\n' "$TOOLCHAIN_PATH" >&2
  exit 1
fi

NDK_PARENT_DIR=$(cd "$NDK_PATH"/.. && pwd -P)

ANDROID_TRIPLE_BIN="$TOOLCHAIN_PATH/bin/${TOOLCHAIN_TRIPLE}${API_LEVEL}"
if [[ ! -x "${ANDROID_TRIPLE_BIN}-clang" ]]; then
  printf 'Error: compiler "%s-clang" not found.\n' "$ANDROID_TRIPLE_BIN" >&2
  exit 1
fi

export ANDROID_NDK_HOME="$NDK_PATH"
find_host_tool() {
  local candidate resolved
  for candidate in "$@"; do
    resolved=$(command -v "$candidate" 2>/dev/null || true)
    if [[ -z "$resolved" ]]; then
      continue
    fi
    if [[ -n "$TOOLCHAIN_PATH" && "$resolved" == "$TOOLCHAIN_PATH"/* ]]; then
      continue
    fi
    if [[ -n "$NDK_PARENT_DIR" && "$resolved" == "$NDK_PARENT_DIR"/* ]]; then
      continue
    fi
    printf '%s\n' "$resolved"
    return 0
  done
  printf '%s\n' "$1"
}

HOST_CC="${CC_host:-}"
if [[ -n "$HOST_CC" ]]; then
  if [[ "$HOST_CC" == "$TOOLCHAIN_PATH"/* || "$HOST_CC" == "$NDK_PARENT_DIR"/* || ! -x "$HOST_CC" ]]; then
    HOST_CC=""
  fi
fi
HOST_CXX="${CXX_host:-}"
if [[ -n "$HOST_CXX" ]]; then
  if [[ "$HOST_CXX" == "$TOOLCHAIN_PATH"/* || "$HOST_CXX" == "$NDK_PARENT_DIR"/* || ! -x "$HOST_CXX" ]]; then
    HOST_CXX=""
  fi
fi
if [[ -z "$HOST_CC" ]]; then
  HOST_CC=$(find_host_tool clang cc gcc)
fi
if [[ -z "$HOST_CXX" ]]; then
  HOST_CXX=$(find_host_tool clang++ c++ g++)
fi

export TOOLCHAIN="$TOOLCHAIN_PATH"
export PATH="$TOOLCHAIN_PATH/bin:$PATH"
export CC="${ANDROID_TRIPLE_BIN}-clang"
export CXX="${ANDROID_TRIPLE_BIN}-clang++"
export AR="$TOOLCHAIN_PATH/bin/llvm-ar"
export RANLIB="$TOOLCHAIN_PATH/bin/llvm-ranlib"
export STRIP="$TOOLCHAIN_PATH/bin/llvm-strip"
export NM="$TOOLCHAIN_PATH/bin/llvm-nm"
export LD="$TOOLCHAIN_PATH/bin/ld.lld"
export SYSROOT="$TOOLCHAIN_PATH/sysroot"

HOST_AR="${AR_host:-}"
if [[ -n "$HOST_AR" ]]; then
  if [[ "$HOST_AR" == "$TOOLCHAIN_PATH"/* || "$HOST_AR" == "$NDK_PARENT_DIR"/* || ! -x "$HOST_AR" ]]; then
    HOST_AR=""
  fi
fi
HOST_RANLIB="${RANLIB_host:-}"
if [[ -n "$HOST_RANLIB" ]]; then
  if [[ "$HOST_RANLIB" == "$TOOLCHAIN_PATH"/* || "$HOST_RANLIB" == "$NDK_PARENT_DIR"/* || ! -x "$HOST_RANLIB" ]]; then
    HOST_RANLIB=""
  fi
fi
HOST_STRIP="${STRIP_host:-}"
if [[ -n "$HOST_STRIP" ]]; then
  if [[ "$HOST_STRIP" == "$TOOLCHAIN_PATH"/* || "$HOST_STRIP" == "$NDK_PARENT_DIR"/* || ! -x "$HOST_STRIP" ]]; then
    HOST_STRIP=""
  fi
fi
if [[ -z "$HOST_AR" ]]; then
  HOST_AR=$(find_host_tool ar llvm-ar)
fi
if [[ -z "$HOST_RANLIB" ]]; then
  HOST_RANLIB=$(find_host_tool ranlib llvm-ranlib)
fi
if [[ -z "$HOST_STRIP" ]]; then
  HOST_STRIP=$(find_host_tool strip llvm-strip)
fi

export CC_host="$HOST_CC"
export CXX_host="$HOST_CXX"
export AR_host="$HOST_AR"
export RANLIB_host="$HOST_RANLIB"
export STRIP_host="$HOST_STRIP"

export GYP_DEFINES="target_arch=${GYP_ARCH} v8_target_arch=${GYP_ARCH} android_target_arch=${GYP_ARCH} host_os=${HOST_OS} OS=android android_ndk_path=${NDK_PATH}"

printf 'Using NDK: %s\n' "$NDK_PATH"
printf 'Toolchain: %s\n' "$TOOLCHAIN_PATH"
printf 'Target: %s (API %s)\n' "$TOOLCHAIN_TRIPLE" "$API_LEVEL"
printf 'Host compilers: CC_host=%s CXX_host=%s\n' "$HOST_CC" "$HOST_CXX"

if (( APPLY_PATCH )); then
  ./android-configure patch
  if [[ $? -ne 0 ]]; then
    printf 'Error: failed to apply Android patches.\n' >&2
    exit 1
  fi
fi

CONFIG_ARGS=(
  "--dest-cpu=${DEST_CPU}"
  "--dest-os=android"
  "--cross-compiling"
  "--openssl-no-asm"
)

if (( BUILD_SHARED )); then
  CONFIG_ARGS+=("--shared")
fi

CONFIG_ARGS+=("${USER_CONFIG_FLAGS[@]}")

printf 'Running configure with flags: %s\n' "${CONFIG_ARGS[*]}"
./configure "${CONFIG_ARGS[@]}"

if (( RUN_BUILD )); then
  printf 'Building target "%s" with %s jobs...\n' "$MAKE_TARGET" "$JOBS"
  make -j"$JOBS" "$MAKE_TARGET"
fi
