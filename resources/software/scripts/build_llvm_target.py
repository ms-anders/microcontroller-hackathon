#!/usr/bin/env python3
"""
build_llvm_target.py — Integrate and build any custom LLVM backend.

Copies a backend source directory into the LLVM source tree already present
in this container, registers the target, reconfigures CMake, then performs
an incremental Ninja rebuild (only the new target is recompiled).

The backend directory must contain:
  - CMakeLists.txt  (with add_llvm_target(...))
  - TargetInfo/     (registers the target triple)
  - MCTargetDesc/   (MC-layer descriptors)

Usage:
    python scripts/build_llvm_target.py <backend-dir> [options]

Options:
    --target-name NAME   Override LLVM target name (default: directory basename)
    --llvm-src    DIR    LLVM source root   (default: $LLVM_SRC  or /llvm-src)
    --llvm-build  DIR    LLVM build root    (default: $LLVM_BUILD or /llvm-build)
    --install-dir DIR    Install prefix     (default: $LLVM_INSTALL or /opt/llvm)
    -j N                 Parallel build jobs (default: all CPUs)
    --no-install         Skip install step after build
    --verify             Check target appears in `llc --version` after build
    -v, --verbose        Show full build output (cmake + ninja)

Examples:
    python scripts/build_llvm_target.py resources/software/template/llvm-backend/
    python scripts/build_llvm_target.py /workspace/my-backend/ --target-name MyISA -j8 --verify
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


# ---------------------------------------------------------------------------
# Logging helpers
# ---------------------------------------------------------------------------

def _c(code: str, msg: str) -> str:
    return f"\033[{code}m[build-llvm-target]\033[0m {msg}"

def info(msg):  print(_c("36", msg), flush=True)
def ok(msg):    print(_c("32", msg), flush=True)
def warn(msg):  print(_c("33", msg), flush=True)
def error(msg): sys.exit(_c("31;1", f"ERROR: {msg}"))


# ---------------------------------------------------------------------------
# Subprocess helper
# ---------------------------------------------------------------------------

def run(cmd: list, *, cwd=None, verbose: bool = False, check: bool = True):
    """Run *cmd*, streaming output when verbose, capturing it otherwise."""
    if verbose:
        print(f"  $ {' '.join(str(c) for c in cmd)}", flush=True)
    result = subprocess.run(
        [str(c) for c in cmd],
        cwd=str(cwd) if cwd else None,
        text=True,
        capture_output=not verbose,
    )
    if check and result.returncode != 0:
        if not verbose:
            if result.stdout:
                print(result.stdout, end="")
            if result.stderr:
                print(result.stderr, end="", file=sys.stderr)
        sys.exit(f"\nCommand failed (exit {result.returncode}): {' '.join(str(c) for c in cmd)}")
    return result


# ---------------------------------------------------------------------------
# Step implementations
# ---------------------------------------------------------------------------

def detect_target_name(backend_dir: Path) -> str | None:
    """
    Parse the backend CMakeLists.txt for `add_llvm_target(XxxCodeGen ...)` and
    return the bare target name (i.e. `Xxx`) without the CodeGen suffix.
    Returns None if no match is found.
    """
    cmake = backend_dir / "CMakeLists.txt"
    text = cmake.read_text()
    # add_llvm_target accepts the name as the first positional argument
    m = re.search(r"add_llvm_target\s*\(\s*(\w+?)CodeGen\b", text)
    if m:
        return m.group(1)
    # Fallback: look for any add_llvm_target call
    m = re.search(r"add_llvm_target\s*\(\s*(\w+)", text)
    if m:
        return m.group(1)
    return None


def copy_backend(src: Path, dest: Path):
    """Copy backend directory into the LLVM source tree."""
    if dest.exists():
        warn(f"Target already exists at {dest} — overwriting")
        shutil.rmtree(dest)
    shutil.copytree(src, dest)
    ok(f"Copied to {dest}")


def inject_component_groups(dest: Path):
    """
    LLVM's ADD_TO_COMPONENT directive requires each component to be pre-declared
    with add_llvm_component_group().  Scan every CMakeLists.txt in the backend
    tree for ADD_TO_COMPONENT references and inject any missing declarations at
    the top of the backend's root CMakeLists.txt.

    This is a generic fix that works for any backend that was written without
    the explicit component group declarations (a common omission).
    """
    component_names: set[str] = set()
    for cmake in sorted(dest.rglob("CMakeLists.txt")):
        for m in re.finditer(
            r"ADD_TO_COMPONENT\s*\n\s*(\w+)",
            cmake.read_text(),
        ):
            component_names.add(m.group(1))

    if not component_names:
        return

    top_cmake = dest / "CMakeLists.txt"
    text = top_cmake.read_text()

    to_inject = [
        name for name in sorted(component_names)
        if f"add_llvm_component_group({name})" not in text
    ]

    if not to_inject:
        return

    header = "\n".join(f"add_llvm_component_group({n})" for n in to_inject) + "\n\n"
    top_cmake.write_text(header + text)
    ok(f"Injected component group declarations: {', '.join(to_inject)}")


def register_subdirectory(target_name: str, dest: Path, lib_target_cmake: Path):
    """
    Ensure the LLVM target directory is reachable from
    llvm/lib/Target/CMakeLists.txt.

    LLVM's CMakeLists.txt iterates LLVM_TARGETS_TO_BUILD and calls
    add_subdirectory(${t}) — so the subdirectory name *must* match the
    target name exactly.  If the source backend directory has a different
    name (e.g. "llvm-backend" vs "MyISA") we already copied it under the
    correct name in copy_backend(), so no extra add_subdirectory() line is
    needed here.  We just validate the destination exists.
    """
    if not dest.is_dir():
        error(f"Expected backend to be present at {dest} — copy_backend() may have failed")
    ok(f"{target_name} directory present at {dest} — LLVM will pick it up via LLVM_TARGETS_TO_BUILD")


def register_in_all_targets(target_name: str, llvm_src: Path):
    """
    Add *target_name* to the LLVM_ALL_TARGETS list in llvm/CMakeLists.txt
    so it is a valid -DLLVM_TARGETS_TO_BUILD= value.
    """
    top_cmake = llvm_src / "llvm" / "CMakeLists.txt"
    if not top_cmake.exists():
        warn(f"{top_cmake} not found — skipping LLVM_ALL_TARGETS registration")
        return

    text = top_cmake.read_text()
    if re.search(rf"\b{re.escape(target_name)}\b", text):
        warn(f"{target_name} already in LLVM_ALL_TARGETS — skipping")
        return

    # Append inside the set(LLVM_ALL_TARGETS ...) block, before its closing )
    new_text, n = re.subn(
        r"(set\s*\(\s*LLVM_ALL_TARGETS\b[^)]*?)(\s*\))",
        rf"\1\n  {target_name}\2",
        text,
        count=1,
        flags=re.DOTALL,
    )
    if n == 0:
        warn(f"Could not locate set(LLVM_ALL_TARGETS ...) in {top_cmake} — skipping")
        return

    top_cmake.write_text(new_text)
    ok(f"Added {target_name} to LLVM_ALL_TARGETS in llvm/CMakeLists.txt")


def cmake_reconfigure(
    target_name: str,
    llvm_src: Path,
    llvm_build: Path,
    verbose: bool,
    force: bool = False,
):
    """Read the current LLVM_TARGETS_TO_BUILD, append the new target, reconfigure."""
    cache = llvm_build / "CMakeCache.txt"
    if not cache.exists():
        error(
            f"CMakeCache.txt not found at {cache}.\n"
            "       Is the LLVM build tree present in this container?"
        )

    current_targets = ""
    for line in cache.read_text().splitlines():
        if line.startswith("LLVM_TARGETS_TO_BUILD:"):
            current_targets = line.split("=", 1)[1].strip()
            break

    already_registered = target_name in current_targets.split(";")

    if already_registered and not force:
        # Still run cmake to pick up any changes in the backend source files
        # (e.g. newly injected component groups).  This is cheap when nothing
        # changed because CMake detects no modification.
        new_targets = current_targets
    else:
        new_targets = f"{current_targets};{target_name}" if current_targets else target_name

    info(f"Reconfiguring: LLVM_TARGETS_TO_BUILD={new_targets}")

    run(
        ["cmake", "-S", str(llvm_src / "llvm"),
         "-B", str(llvm_build),
         f"-DLLVM_TARGETS_TO_BUILD={new_targets}"],
        cwd=llvm_build,
        verbose=verbose,
    )
    ok("CMake reconfigured")


def get_ninja_targets(llvm_build: Path, target_name: str) -> list[str]:
    """
    Query ninja for all `LLVM<TargetName>*` phony targets that are actually
    present in the build graph.  Returns an empty list on any failure.
    """
    result = subprocess.run(
        ["ninja", "-C", str(llvm_build), "-t", "targets"],
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        return []
    prefix = f"LLVM{target_name}"
    targets = [
        line.split(":")[0].strip()
        for line in result.stdout.splitlines()
        if line.startswith(prefix) and ": phony" in line
    ]
    return targets


def build_target(target_name: str, llvm_build: Path, jobs: int, verbose: bool):
    """
    Try to build only the new target's components for a fast incremental build.
    Falls back to a full `ninja` if no per-target components are found.
    """
    components = get_ninja_targets(llvm_build, target_name)

    if components:
        info(f"Building {target_name} components: {', '.join(components)}  (-j{jobs})…")
        result = run(
            ["ninja", "-C", str(llvm_build), f"-j{jobs}"] + components,
            verbose=verbose,
            check=False,
        )
        if result.returncode == 0:
            ok("Build complete")
            return
        warn("Per-component build failed — falling back to full ninja build")
    else:
        warn(f"No LLVM{target_name}* ninja targets found — running full ninja build")

    run(["ninja", "-C", str(llvm_build), f"-j{jobs}"], verbose=verbose)
    ok("Build complete")


def install_llvm(llvm_build: Path, jobs: int, verbose: bool):
    info("Installing…")
    run(["ninja", "-C", str(llvm_build), f"-j{jobs}", "install"], verbose=verbose)
    ok("Installed")


def verify_target(target_name: str, install_dir: Path):
    llc = install_dir / "bin" / "llc"
    if not llc.exists():
        warn(f"llc not found at {llc} — cannot verify")
        return

    result = subprocess.run([str(llc), "--version"], capture_output=True, text=True)
    output = (result.stdout + result.stderr).lower()

    if target_name.lower() in output:
        ok(f"{target_name} confirmed in `llc --version`")
    else:
        warn(
            f"{target_name} not found in `llc --version` output.\n"
            f"       Check manually: {llc} --version"
        )


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(
        prog="build_llvm_target.py",
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument(
        "backend_dir",
        metavar="BACKEND_DIR",
        help="Path to the LLVM backend source directory",
    )
    ap.add_argument(
        "--target-name",
        metavar="NAME",
        help="Override LLVM target name (default: directory basename)",
    )
    ap.add_argument(
        "--llvm-src",
        metavar="DIR",
        default=os.environ.get("LLVM_SRC", "/llvm-src"),
        help="LLVM source root (default: $LLVM_SRC)",
    )
    ap.add_argument(
        "--llvm-build",
        metavar="DIR",
        default=os.environ.get("LLVM_BUILD", "/llvm-build"),
        help="LLVM build root (default: $LLVM_BUILD)",
    )
    ap.add_argument(
        "--install-dir",
        metavar="DIR",
        default=os.environ.get("LLVM_INSTALL", "/opt/llvm"),
        help="Install prefix (default: $LLVM_INSTALL)",
    )
    ap.add_argument(
        "-j", "--jobs",
        type=int,
        default=os.cpu_count(),
        metavar="N",
        help="Parallel build jobs (default: CPU count)",
    )
    ap.add_argument(
        "--no-install",
        action="store_true",
        help="Skip the install step",
    )
    ap.add_argument(
        "--verify",
        action="store_true",
        help="Verify the target appears in `llc --version` after build",
    )
    ap.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Show full cmake + ninja output",
    )
    return ap.parse_args()


def main():
    args = parse_args()

    backend_dir = Path(args.backend_dir).resolve()
    llvm_src    = Path(args.llvm_src).resolve()
    llvm_build  = Path(args.llvm_build).resolve()
    install_dir = Path(args.install_dir).resolve()

    # Determine target name: CLI > CMakeLists.txt detection > directory name
    if args.target_name:
        target_name = args.target_name
    else:
        detected = detect_target_name(backend_dir)
        if detected:
            target_name = detected
            info(f"Auto-detected target name from CMakeLists.txt: {target_name}")
        else:
            target_name = backend_dir.name
            warn(
                f"Could not auto-detect target name — falling back to directory name '{target_name}'.\n"
                "       If the build fails, pass --target-name <TargetName> explicitly."
            )

    # ---- validation --------------------------------------------------------
    if not backend_dir.is_dir():
        error(f"Backend directory not found: {backend_dir}")
    if not (backend_dir / "CMakeLists.txt").exists():
        error(f"No CMakeLists.txt in backend directory: {backend_dir}")
    if not llvm_src.is_dir():
        error(
            f"LLVM source not found at {llvm_src}.\n"
            "       Pass --llvm-src or set $LLVM_SRC. "
            "Are you running inside the ISA dev container?"
        )
    if not llvm_build.is_dir():
        error(
            f"LLVM build tree not found at {llvm_build}.\n"
            "       Pass --llvm-build or set $LLVM_BUILD."
        )

    # ---- summary -----------------------------------------------------------
    info(f"Target name  : {target_name}")
    info(f"Backend dir  : {backend_dir}")
    info(f"LLVM source  : {llvm_src}")
    info(f"LLVM build   : {llvm_build}")
    info(f"Install dir  : {install_dir}")
    info(f"Build jobs   : {args.jobs}")
    print()

    # ---- steps -------------------------------------------------------------
    dest = llvm_src / "llvm" / "lib" / "Target" / target_name
    copy_backend(backend_dir, dest)
    inject_component_groups(dest)

    lib_target_cmake = llvm_src / "llvm" / "lib" / "Target" / "CMakeLists.txt"
    if lib_target_cmake.exists():
        register_subdirectory(target_name, dest, lib_target_cmake)

    register_in_all_targets(target_name, llvm_src)
    cmake_reconfigure(target_name, llvm_src, llvm_build, args.verbose)
    build_target(target_name, llvm_build, args.jobs, args.verbose)

    if not args.no_install:
        install_llvm(llvm_build, args.jobs, args.verbose)

    if args.verify:
        verify_target(target_name, install_dir)

    # ---- usage hint --------------------------------------------------------
    print()
    target_lower = target_name.lower()
    ok("Done. Compile C to your target assembly:")
    print(f"  clang -target {target_lower}-unknown-elf -S -o out.s input.c")
    print(f"  llc   -march={target_lower} -o out.s input.ll")


if __name__ == "__main__":
    main()
