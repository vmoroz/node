#!/usr/bin/env python3
import argparse
import os
import platform
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

ARCH_CONFIG = {
    "arm": {
        "dest_cpu": "arm",
        "triple": "armv7a-linux-androideabi",
        "gyp_arch": "arm",
    },
    "armeabi": "arm",
    "armeabi-v7a": "arm",
    "arm64": {
        "dest_cpu": "arm64",
        "triple": "aarch64-linux-android",
        "gyp_arch": "arm64",
    },
    "aarch64": "arm64",
    "x86": {
        "dest_cpu": "ia32",
        "triple": "i686-linux-android",
        "gyp_arch": "x86",
    },
    "x86_64": {
        "dest_cpu": "x64",
        "triple": "x86_64-linux-android",
        "gyp_arch": "x64",
    },
    "x64": "x86_64",
}

REPO_ROOT = Path(__file__).resolve().parent


class AndroidConfigureError(Exception):
    pass


def resolve_arch_config(arch):
    key = arch.lower()
    value = ARCH_CONFIG.get(key)
    if value is None:
        raise AndroidConfigureError(
            "Invalid target architecture, must be one of: arm, arm64, aarch64, x86, x86_64"
        )
    if isinstance(value, str):
        return resolve_arch_config(value)
    return value


def detect_host_os():
    system = platform.system()
    if system == "Linux":
        return "linux"
    if system == "Darwin":
        return "darwin"
    raise AndroidConfigureError(
        "android-configure is currently only supported on Linux and Darwin."
    )


def default_ndk_host_tag(ndk_path, host_os):
    if host_os == "linux":
        return "linux-x86_64"
    darwin_arm64 = ndk_path / "toolchains" / "llvm" / "prebuilt" / "darwin-arm64"
    if darwin_arm64.is_dir():
        return "darwin-arm64"
    return "darwin-x86_64"


def detect_cpu_count():
    nproc = shutil.which("nproc")
    if nproc:
        try:
            return int(subprocess.check_output([nproc]).strip())
        except (OSError, ValueError, subprocess.CalledProcessError):
            pass
    if platform.system() == "Darwin":
        sysctl = shutil.which("sysctl")
        if sysctl:
            try:
                return int(
                    subprocess.check_output([sysctl, "-n", "hw.ncpu"]).strip()
                )
            except (OSError, ValueError, subprocess.CalledProcessError):
                pass
    return 4


def resolve_executable_path(path):
    if not path:
        return None
    candidate = shutil.which(path) if not os.path.isabs(path) else path
    if not candidate:
        return None
    resolved = Path(candidate).resolve()
    if not resolved.exists() or not os.access(resolved, os.X_OK):
        return None
    return resolved


def is_path_inside(path, parent):
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def is_valid_host_tool(path, toolchain_path, ndk_parent):
    resolved = resolve_executable_path(path)
    if resolved is None:
        return False
    toolchain_root = toolchain_path.resolve()
    ndk_parent_root = ndk_parent.resolve()
    if is_path_inside(resolved, toolchain_root):
        return False
    if is_path_inside(resolved, ndk_parent_root):
        return False
    return True


def find_host_tool(candidates, toolchain_path, ndk_parent):
    for candidate in candidates:
        if is_valid_host_tool(candidate, toolchain_path, ndk_parent):
            resolved = resolve_executable_path(candidate)
            if resolved is not None:
                return str(resolved)
    return candidates[0]


def resolve_host_tool_env(var_name, candidates, toolchain_path, ndk_parent):
    current = os.environ.get(var_name)
    if current and is_valid_host_tool(current, toolchain_path, ndk_parent):
        resolved = resolve_executable_path(current)
        if resolved is not None:
            os.environ[var_name] = str(resolved)
            return str(resolved)
    resolved = find_host_tool(candidates, toolchain_path, ndk_parent)
    os.environ[var_name] = resolved
    return resolved


def print_info(message):
    print(f"\033[92mInfo:\033[0m {message}")


def print_error(message):
    print(f"\033[91mError:\033[0m {message}")


def run_command(cmd, *, env=None, verbose=False, cwd=None, stdin=None):
    cmd_list = [str(part) for part in cmd]
    if verbose:
        printable = " ".join(shlex.quote(part) for part in cmd_list)
        print(f"+ {printable}")
    subprocess.run(cmd_list, check=True, env=env, cwd=cwd, stdin=stdin)


def transform_legacy_args(argv):
    if len(argv) == 3 and all(not arg.startswith("-") for arg in argv):
        return ["--ndk", argv[0], "--api", argv[1], "--arch", argv[2]]
    return argv


def build_parser():
    parser = argparse.ArgumentParser(
        description="Configure and optionally build Node.js for Android",
        allow_abbrev=False,
    )
    parser.add_argument(
        "--ndk",
        help="Absolute path to the Android NDK root (e.g. $ANDROID_HOME/ndk/29.0.x)",
    )
    parser.add_argument(
        "--api", type=int, default=34, help="Android API level to target (default: 34)"
    )
    parser.add_argument(
        "--arch",
        default="arm64",
        help="Target architecture (arm, arm64, aarch64, x86, x86_64). Default: arm64",
    )
    parser.add_argument(
        "--ndk-host-tag",
        help="Override NDK host tag (e.g. linux-x86_64)",
    )
    parser.add_argument(
        "--shared", action="store_true", help="Build libnode as a shared library"
    )
    parser.add_argument(
        "--build",
        action="store_true",
        help="Also run make after configure",
    )
    parser.add_argument(
        "--jobs",
        type=int,
        help="Parallel make jobs when --build is used (default: host CPU count)",
    )
    parser.add_argument(
        "--make-target",
        default="node",
        help="Make target to build when --build is used (default: node)",
    )
    parser.add_argument(
        "--verbose", action="store_true", help="Print executed commands"
    )
    return parser


def run_pipeline(args, extra_flags):
    ndk_path = Path(args.ndk).expanduser().resolve()
    if not ndk_path.is_dir() or not any(ndk_path.iterdir()):
        raise AndroidConfigureError("Invalid path to the Android NDK")

    api_level = args.api
    if api_level < 24:
        raise AndroidConfigureError(
            "Android API level must be at least 24 (Android 7.0)"
        )

    arch_cfg = resolve_arch_config(args.arch)
    host_os = detect_host_os()
    host_tag = args.ndk_host_tag or default_ndk_host_tag(ndk_path, host_os)

    toolchain_path = ndk_path / "toolchains" / "llvm" / "prebuilt" / host_tag
    if not toolchain_path.is_dir():
        raise AndroidConfigureError(
            f'Toolchain directory "{toolchain_path}" not found.'
        )

    triple = arch_cfg["triple"]
    triple_prefix = toolchain_path / "bin" / f"{triple}{api_level}"
    clang_path = Path(f"{triple_prefix}-clang")
    if not clang_path.exists():
        raise AndroidConfigureError(f'Compiler "{clang_path}" not found.')

    toolchain_bin = toolchain_path / "bin"
    os.environ["ANDROID_NDK_HOME"] = str(ndk_path)
    os.environ["TOOLCHAIN"] = str(toolchain_path)
    os.environ["PATH"] = f"{toolchain_bin}{os.pathsep}{os.environ.get('PATH', '')}"
    os.environ["CC"] = f"{triple_prefix}-clang"
    os.environ["CXX"] = f"{triple_prefix}-clang++"
    os.environ["AR"] = str(toolchain_bin / "llvm-ar")
    os.environ["RANLIB"] = str(toolchain_bin / "llvm-ranlib")
    os.environ["STRIP"] = str(toolchain_bin / "llvm-strip")
    os.environ["NM"] = str(toolchain_bin / "llvm-nm")
    os.environ["LD"] = str(toolchain_bin / "ld.lld")
    os.environ["SYSROOT"] = str(toolchain_path / "sysroot")

    ndk_parent = ndk_path.parent
    host_cc = resolve_host_tool_env(
        "CC_host", ["clang", "cc", "gcc"], toolchain_path, ndk_parent
    )
    host_cxx = resolve_host_tool_env(
        "CXX_host", ["clang++", "c++", "g++"], toolchain_path, ndk_parent
    )
    resolve_host_tool_env("AR_host", ["ar", "llvm-ar"], toolchain_path, ndk_parent)
    resolve_host_tool_env(
        "RANLIB_host", ["ranlib", "llvm-ranlib"], toolchain_path, ndk_parent
    )
    resolve_host_tool_env(
        "STRIP_host", ["strip", "llvm-strip"], toolchain_path, ndk_parent
    )

    gyp_arch = arch_cfg["gyp_arch"]
    gyp_defines = [
        f"target_arch={gyp_arch}",
        f"v8_target_arch={gyp_arch}",
        f"android_target_arch={gyp_arch}",
        f"host_os={host_os}",
        "OS=android",
        f"android_ndk_path={ndk_path}",
    ]
    os.environ["GYP_DEFINES"] = " ".join(gyp_defines)

    print_info(f"Using NDK: {ndk_path}")
    print_info(f"Toolchain: {toolchain_path}")
    print_info(f"Target: {triple} (API {api_level})")
    print_info(f"Host compilers: CC_host={host_cc} CXX_host={host_cxx}")

    configure_args = [
        f"--dest-cpu={arch_cfg['dest_cpu']}",
        "--dest-os=android",
        "--cross-compiling",
        "--openssl-no-asm",
    ]
    if args.shared:
        configure_args.append("--shared")
    configure_args.extend(extra_flags)

    env = os.environ.copy()
    print_info(f"Running configure with flags: {' '.join(configure_args)}")
    run_command(["./configure", *configure_args], env=env, cwd=REPO_ROOT, verbose=args.verbose)
    print_info("Configure complete.")

    if args.build:
        jobs = args.jobs if args.jobs else detect_cpu_count()
        if jobs < 1:
            raise AndroidConfigureError("--jobs must be at least 1")
        print_info(f"Building target '{args.make_target}' with {jobs} jobs...")
        run_command(
            ["make", f"-j{jobs}", args.make_target],
            env=env,
            cwd=REPO_ROOT,
            verbose=args.verbose,
        )
        print_info("Build complete.")


def main():
    os.chdir(REPO_ROOT)
    argv = sys.argv[1:]

    argv = transform_legacy_args(argv)
    parser = build_parser()
    if not argv:
        print_info("No arguments provided. Showing help.")
        parser.print_help()
        return

    args, extra = parser.parse_known_args(argv)
    if not args.ndk:
        parser.error("--ndk is required.")

    try:
        run_pipeline(args, extra)
    except AndroidConfigureError as exc:
        print_error(str(exc))
        sys.exit(1)
    except subprocess.CalledProcessError as exc:
        print_error(f"Command failed with exit code {exc.returncode}")
        sys.exit(exc.returncode)


if __name__ == "__main__":
    main()
