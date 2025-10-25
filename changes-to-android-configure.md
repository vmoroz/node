# Android Configure Refactor Plan

## Goal
Unify our Android build entry point by enhancing `android_configure.py` so it subsumes the functionality that currently lives in `tools/android-build.sh`. After this refactor we should be able to drop the shell script and rely solely on the Python tool for both patching and configuring/building Node.js for Android.

## Current State
- `android_configure.py` only supports the legacy workflow: minimal CLI, rudimentary validation, direct environment mutation, and a hard-coded `./configure` invocation.
- `tools/android-build.sh` carries the modern workflow: feature-rich CLI (NDK/API/arch selection, shared builds, job control, apply patches, configure-only mode, host-tool fallback detection, extra `./configure` flags passed via `--`), robust toolchain discovery, and optional `make` invocation.
- The two scripts diverge in capability, leading to duplicated logic and confusion about the canonical entry point for Android builds.

## Functional Requirements
To retire `tools/android-build.sh`, `android_configure.py` must provide the same surface area:
1. **Structured CLI** – Argument parsing equivalent to the shell script (with `--ndk`, `--api`, `--arch`, `--jobs`, `--shared`, `--configure-only`, `--apply-patch`, `--make-target`, `--ndk-host-tag`, `--verbose`, and passthrough `--` flags).
2. **Robust validation** – Helpful errors for missing/invalid NDK paths, unsupported architectures, or malformed flag combinations.
3. **Toolchain setup** – Detect host OS, resolve the correct NDK toolchain path (allow override), export target compiler/binutils, and populate `SYSROOT`. All environment mutations must match the shell script behaviour.
4. **Host tool discovery** – Locate usable host compilers/binutils outside the NDK tree, falling back to sane defaults (re-implement `find_host_tool`).
5. **Patch orchestration** – Surface the patch application sequence (trap-handler + zlib) with clear logging and failure handling.
6. **GYP define construction** – Mirror the shell script’s `GYP_DEFINES`, including `android_ndk_path`, host OS, and target architecture values.
7. **Configure invocation** – Allow optional `--shared` and passthrough flags, and reuse common argument construction for both configure-only and build flows.
8. **Optional `make` step** – Honour `--configure-only` and `--make-target`, with default job count detection (`nproc`/`sysctl`).
9. **Verbosity** – Respect `--verbose` to enable debug logging (akin to `set -x`).
10. **Backward compatibility** – Preserve the existing `./android-configure patch` entrypoint and the legacy positional arguments path for users relying on it today.

## Proposed Changes
1. **Rewrite CLI with `argparse`**
   - Support both the new flag set and the old positional invocation (emit deprecation warning).
   - Add a dedicated `patch` subcommand to keep the one-off patch workflow intact.

2. **Encapsulate environment logic**
   - Implement helper functions for host detection, NDK toolchain resolution, and environment export.
   - Mirror `tools/android-build.sh` semantics (including host-tag inference and validation).

3. **Host tool discovery helpers**
   - Port `find_host_tool` logic so host compilers do not point into the NDK tree.
   - Respect existing `*_host` environment overrides when valid.

4. **Patch runner**
   - Move patch execution to Python (`subprocess.run`) with error handling and explanatory messages.
   - Keep the patch list synchronized with `android-patches/` and make future additions easier (e.g., iterate a descriptor array).

5. **Configure wrapper**
   - Assemble the full `./configure` argument vector based on flags (`--dest-cpu`, `--dest-os=android`, `--openssl-no-asm`, etc.).
   - Append passthrough flags captured after `--`.

6. **Build step**
   - Invoke `make` when `--configure-only` is unset; honour custom `--make-target` and job count.
   - Detect job count via `nproc` (Linux) or `sysctl -n hw.ncpu` (macOS).

7. **Logging & UX**
   - Print a concise summary (NDK path, toolchain, target triple, host tool paths, configure flags).
   - Gate verbose command echoing on `--verbose`.

8. **Structure & testing**
   - Organise code into functions with a `main()` entrypoint for easier testing.
   - Add lightweight unit coverage where practical (e.g., CLI parsing, toolchain inference), or at minimum document manual verification steps.

## Migration Plan
1. Implement the refactored Python script behind the existing filename (`android_configure.py`).
2. Validate end-to-end builds for each supported architecture (arm, arm64, x86, x86_64) on Linux and macOS hosts.
3. Update project documentation (README / BUILDING.md / onboarding.md) to point to the unified Python workflow.
4. Deprecate `tools/android-build.sh` (leave stub or removal note) once the Python tool is proven in CI.

## Open Questions
- Should we add automated tests (CI job) that runs `android_configure.py --configure-only` against a small matrix to prevent regressions?
- Do we want to formalise the patch list in a JSON/YAML manifest (as hinted by the existing TODO) during this refactor?
- Are there downstream consumers of `android-build.sh` (external guides/scripts) that need a migration heads-up?

Resolving these questions will help finalise the implementation timeline, but the outlined changes provide a clear roadmap to consolidating our Android build tooling.
