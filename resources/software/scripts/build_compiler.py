#!/usr/bin/env python3
"""
build_compiler.py — Build an LLVM backend for a custom ISA.

Usage:
    python3 scripts/build_compiler.py <config.build-compiler.yml>

The YAML file must contain a top-level 'build-compiler' key.  All relative
paths inside it are resolved against the directory that contains the YAML file.

Minimal example (my_cpu.build-compiler.yml):
    build-compiler:
      target:
        backend-dir: ./llvm-backend
"""

import argparse
import filecmp
import glob
import os
import re
import shutil
import subprocess
import sys

try:
    import yaml
except ImportError:
    print("ERROR: PyYAML is required.  Run: pip3 install pyyaml", file=sys.stderr)
    sys.exit(1)


# --------------------------------------------------------------------------- #
# Logging                                                                      #
# --------------------------------------------------------------------------- #

def log(msg):   print(f"[build-compiler] {msg}")
def die(msg):   print(f"[build-compiler] ERROR: {msg}", file=sys.stderr); sys.exit(1)


def run(cmd, *, cwd=None, verbose=False):
    if verbose:
        log("$ " + " ".join(str(c) for c in cmd))
    r = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, text=True)
    if verbose:
        print(r.stdout, end="")
    return r.returncode, r.stdout


# --------------------------------------------------------------------------- #
# System capacity -- pick a build parallelism that will not exhaust RAM        #
# --------------------------------------------------------------------------- #

def _detect_mem_gb():
    """Best-effort memory ceiling in GiB, honouring container cgroup limits."""
    # A cgroup limit (Docker/K8s) is the real ceiling, so it wins over host RAM.
    for path in ("/sys/fs/cgroup/memory.max",                     # cgroup v2
                 "/sys/fs/cgroup/memory/memory.limit_in_bytes"):  # cgroup v1
        try:
            with open(path) as f:
                raw = f.read().strip()
        except OSError:
            continue
        if raw.isdigit():
            b = int(raw)
            if 0 < b < (1 << 62):        # ignore "max"/unlimited sentinels
                return b / (1024 ** 3)
    try:
        return (os.sysconf("SC_PAGE_SIZE")
                * os.sysconf("SC_PHYS_PAGES")) / (1024 ** 3)
    except (ValueError, OSError, AttributeError):
        return None


def compute_build_parallelism(requested_jobs):
    """Return (compile_jobs, link_jobs) sized to fit the current machine.

    LLVM C++ compiles peak around 2 GiB each and an ``llc``-scale link can
    spike to ~6-8 GiB, so one job per core OOMs low-memory machines (and can
    take down the Docker engine).  When 'jobs' is 0 (auto) we size from
    detected RAM so this is safe on any machine; a non-zero 'jobs' is honoured
    as a deliberate override.
    """
    cpu = os.cpu_count() or 4
    mem_gb = _detect_mem_gb()
    if mem_gb:
        link_jobs = 2 if mem_gb >= 24 else 1
        # Model peak RSS: link_jobs links (~4 GiB each) run alongside the
        # remaining compiles (~2 GiB each), plus ~2 GiB OS/daemon headroom.
        reserve, link_cost, compile_cost = 2.0, 4.0, 2.0
        budget = mem_gb - reserve - link_jobs * link_cost
        mem_jobs = max(1, link_jobs + max(0, int(budget // compile_cost)))
    else:
        link_jobs = 1
        mem_jobs = 2          # unknown memory: stay conservative

    if requested_jobs and requested_jobs > 0:
        jobs = requested_jobs
    else:
        jobs = max(1, min(cpu, mem_jobs))

    mem_txt = f"{mem_gb:.1f} GiB" if mem_gb else "unknown"
    log(f"Build parallelism: -j{jobs}, link jobs {link_jobs} "
        f"(cpus={cpu}, mem={mem_txt})")
    return jobs, link_jobs


# --------------------------------------------------------------------------- #
# YAML loading                                                                 #
# --------------------------------------------------------------------------- #

def load_config(yaml_path):
    with open(yaml_path) as f:
        doc = yaml.safe_load(f)
    if not isinstance(doc, dict) or "build-compiler" not in doc:
        die(f"'{yaml_path}' must have a top-level 'build-compiler' key")
    cfg = doc["build-compiler"]
    base = os.path.dirname(os.path.abspath(yaml_path))

    def rel(path):
        if not path:
            return path
        return path if os.path.isabs(path) else os.path.normpath(os.path.join(base, path))

    # LLVM tree locations — fall back to environment variables then sensible defaults
    llvm = cfg.get("llvm", {}) or {}
    llvm_src     = rel(llvm.get("src-dir",     "")) or os.environ.get("LLVM_SRC",     "/llvm-src")
    llvm_build   = rel(llvm.get("build-dir",   "")) or os.environ.get("LLVM_BUILD",   "/llvm-build")
    llvm_install = rel(llvm.get("install-dir", "")) or os.environ.get("LLVM_INSTALL", "/opt/llvm")
    jobs         = int(llvm.get("jobs", 0))   # 0 = auto (memory-aware)
    verify       = bool(llvm.get("verify", True))

    target = cfg.get("target", {}) or {}
    backend_dir = rel(target.get("backend-dir", ""))
    if not backend_dir:
        die("'target.backend-dir' is required")
    target_name_override = target.get("name", "") or ""

    cmake_cfg = cfg.get("cmake") or None

    return {
        "llvm_src":    llvm_src,
        "llvm_build":  llvm_build,
        "llvm_install": llvm_install,
        "jobs":        jobs,
        "verify":      verify,
        "backend_dir": backend_dir,
        "target_name": target_name_override,
        "cmake_cfg":   cmake_cfg,
    }


# --------------------------------------------------------------------------- #
# Target detection                                                             #
# --------------------------------------------------------------------------- #

def detect_target_name(backend_dir):
    cmake = os.path.join(backend_dir, "CMakeLists.txt")
    if os.path.isfile(cmake):
        with open(cmake) as f:
            content = f.read()
        m = re.search(r"add_llvm_target\(\s*(\w+)CodeGen", content)
        if m:
            return m.group(1)
    name = os.path.basename(backend_dir.rstrip("/"))
    log(f"WARNING: could not detect target name from CMakeLists.txt; "
        f"using directory name '{name}'")
    return name


# --------------------------------------------------------------------------- #
# CMakeLists.txt generation                                                    #
# --------------------------------------------------------------------------- #

def generate_cmake(backend_dir, target_name, cmake_cfg):
    """
    Write (or overwrite) CMakeLists.txt in *backend_dir* from the 'cmake'
    section of the build-compiler YAML.  The file is the single source of
    truth for the backend's build rules; edit the YAML, not the generated file.
    """
    tg_root  = cmake_cfg.get("tablegen-root", f"{target_name}.td")
    tablegens = cmake_cfg.get("tablegen", [])
    sources   = cmake_cfg.get("sources", [])
    link_comps = cmake_cfg.get("link-components", [])
    subdirs   = cmake_cfg.get("subdirectories", [])

    lines = [
        "# Auto-generated by build_compiler.py — do not edit directly.",
        "# Edit the 'cmake' section in the .build-compiler.yml and re-run.",
        "",
        f"add_llvm_component_group({target_name})",
        "",
        f"set(LLVM_TARGET_DEFINITIONS {tg_root})",
        "",
    ]

    for entry in tablegens:
        output = entry["output"]
        flag   = entry["flag"]
        lines.append(f"tablegen(LLVM {output}  {flag})")
    lines.append("")

    lines.append(f"add_public_tablegen_target({target_name}CommonTableGen)")
    lines.append("")

    lines.append(f"add_llvm_target({target_name}CodeGen")
    for src in sources:
        lines.append(f"  {src}")
    lines.append("")
    lines.append("  LINK_COMPONENTS")
    for comp in link_comps:
        lines.append(f"  {comp}")
    lines.append(")")
    lines.append("")

    for subdir in subdirs:
        lines.append(f"add_subdirectory({subdir})")
    lines.append("")

    cmake_path = os.path.join(backend_dir, "CMakeLists.txt")
    with open(cmake_path, "w") as f:
        f.write("\n".join(lines))
    log(f"Generated {cmake_path}")




def sync_backend(src, dest):
    """Incrementally mirror *src* into *dest*, touching only changed files.

    A wipe-and-recopy gives every file a fresh mtime, so Ninja rebuilds the
    whole backend on every run.  This compares file contents and only copies
    what actually changed (preserving mtimes), then prunes files removed from
    *src* -- so editing one .td/.cpp rebuilds just that file, not everything.
    """
    src = os.path.abspath(src)
    os.makedirs(dest, exist_ok=True)

    copied = 0
    for root, _dirs, files in os.walk(src):
        rel = os.path.relpath(root, src)
        dest_root = dest if rel == "." else os.path.join(dest, rel)
        os.makedirs(dest_root, exist_ok=True)
        for name in files:
            s = os.path.join(root, name)
            d = os.path.join(dest_root, name)
            if not os.path.exists(d) or not filecmp.cmp(s, d, shallow=False):
                shutil.copy2(s, d)   # copy2 preserves the source mtime
                copied += 1

    pruned = 0
    for root, dirs, files in os.walk(dest, topdown=False):
        rel = os.path.relpath(root, dest)
        src_root = src if rel == "." else os.path.join(src, rel)
        for name in files:
            if not os.path.exists(os.path.join(src_root, name)):
                os.remove(os.path.join(root, name))
                pruned += 1
        for name in dirs:
            if not os.path.isdir(os.path.join(src_root, name)):
                shutil.rmtree(os.path.join(root, name), ignore_errors=True)
                pruned += 1

    log(f"Synced backend -> {dest} ({copied} changed, {pruned} removed)")


def inject_component_groups(dest):
    """Ensure add_llvm_component_group(<Name>) is declared before ADD_TO_COMPONENT."""
    groups = set()
    for cmake in glob.glob(os.path.join(dest, "**", "CMakeLists.txt"), recursive=True):
        with open(cmake) as f:
            for line in f:
                m = re.search(r"ADD_TO_COMPONENT\s+(\w+)", line)
                if m:
                    groups.add(m.group(1))
    if not groups:
        return
    root_cmake = os.path.join(dest, "CMakeLists.txt")
    with open(root_cmake) as f:
        content = f.read()
    injected = []
    for g in sorted(groups):
        decl = f"add_llvm_component_group({g})"
        if decl not in content:
            injected.append(decl)
    if injected:
        content = "\n".join(injected) + "\n\n" + content
        with open(root_cmake, "w") as f:
            f.write(content)
    log(f"Injected component group declarations: {', '.join(sorted(groups))}")


def register_in_all_targets(target_name, llvm_src):
    all_targets_cmake = os.path.join(llvm_src, "llvm", "CMakeLists.txt")
    if not os.path.isfile(all_targets_cmake):
        die(f"Cannot find {all_targets_cmake}")
    with open(all_targets_cmake) as f:
        content = f.read()
    pattern = r"(set\(LLVM_ALL_TARGETS\b[^)]*?)(\))"
    m = re.search(pattern, content, re.DOTALL)
    if not m:
        die("Cannot find LLVM_ALL_TARGETS in llvm/CMakeLists.txt")
    block = m.group(1)
    if target_name in block:
        log(f"{target_name} already in LLVM_ALL_TARGETS — skipping")
        return
    new_block = block + f"  {target_name}\n"
    new_content = content[:m.start()] + new_block + m.group(2) + content[m.end():]
    with open(all_targets_cmake, "w") as f:
        f.write(new_content)
    log(f"Registered {target_name} in LLVM_ALL_TARGETS")


# --------------------------------------------------------------------------- #
# CMake / Ninja                                                                #
# --------------------------------------------------------------------------- #

def cmake_reconfigure(target_name, llvm_src, llvm_build, llvm_install, verbose,
                      link_jobs=1):
    cmake_exe = shutil.which("cmake") or die("cmake not found on PATH")
    targets = f"X86;{target_name}"
    cmd = [
        cmake_exe,
        f"-DLLVM_TARGETS_TO_BUILD={targets}",
        "-DLLVM_ENABLE_LIBXML2=OFF",
        "-DLLVM_ENABLE_ZSTD=OFF",
        "-DLIBXML2_INCLUDE_DIR=/usr/include",
        "-DLIBXML2_INCLUDE_DIRS=/usr/include",
        "-DZSTD_LIBRARY=/usr/lib/x86_64-linux-gnu/libzstd.so.1",
        "-DZSTD_LIBRARIES=/usr/lib/x86_64-linux-gnu/libzstd.so.1",
        f"-DCMAKE_INSTALL_PREFIX={llvm_install}",
        f"-DLLVM_PARALLEL_LINK_JOBS={link_jobs}",   # links are the RAM spike
    ]
    # Portability-guarded speed-ups: only enable a tool when it is present, so
    # the script still works on any participant setup that lacks it.
    if shutil.which("ld.lld") or shutil.which("lld"):
        cmd.append("-DLLVM_USE_LINKER=lld")       # much faster llc relinks
    if shutil.which("ccache"):
        cmd.append("-DLLVM_CCACHE_BUILD=ON")       # cache object compiles
    cmd += ["-G", "Ninja", os.path.join(llvm_src, "llvm")]
    log(f"Reconfiguring: LLVM_TARGETS_TO_BUILD={targets}")
    rc, out = run(cmd, cwd=llvm_build, verbose=verbose)
    if rc != 0:
        die(f"CMake reconfigure failed:\n{out}")
    log("CMake reconfigured")


def target_already_configured(llvm_build, target_name):
    """True if *target_name* is already in the build's LLVM_TARGETS_TO_BUILD.

    When it is, an explicit CMake reconfigure is redundant -- Ninja will
    re-run CMake by itself if any CMakeLists.txt actually changed.
    """
    cache = os.path.join(llvm_build, "CMakeCache.txt")
    if not os.path.isfile(cache):
        return False
    with open(cache) as f:
        for line in f:
            if line.startswith("LLVM_TARGETS_TO_BUILD:"):
                _, _, val = line.partition("=")
                return target_name in val.strip().split(";")
    return False


def get_ninja_targets(llvm_build, target_name):
    rc, out = run(["ninja", "-C", llvm_build, "-t", "targets", "all"])
    if rc != 0:
        return []
    prefix = f"LLVM{target_name}"
    return [
        line.split(":")[0].strip()
        for line in out.splitlines()
        if line.startswith(prefix) and "phony" in line
    ]


def patch_ninja_zstd_path(llvm_build):
    """
    Some container images only ship libzstd.so.1 (no unversioned libzstd.so).
    If the existing build graph references the missing unversioned path, rewrite
    it to the versioned .so.1 path so ninja can resolve dependencies.
    """
    old = "/usr/lib/x86_64-linux-gnu/libzstd.so"
    new = "/usr/lib/x86_64-linux-gnu/libzstd.so.1"
    if os.path.exists(old) or not os.path.exists(new):
        return
    ninja_file = os.path.join(llvm_build, "build.ninja")
    if not os.path.isfile(ninja_file):
        return
    with open(ninja_file) as f:
        content = f.read()
    if old not in content:
        return
    with open(ninja_file, "w") as f:
        f.write(content.replace(old, new))
    log("Patched build.ninja to use libzstd.so.1")


def build_target(target_name, llvm_build, jobs, verbose):
    ninja = shutil.which("ninja") or die("ninja not found on PATH")
    patch_ninja_zstd_path(llvm_build)
    targets = get_ninja_targets(llvm_build, target_name)
    if targets:
        cmd = [ninja, "-C", llvm_build, f"-j{jobs}"] + targets
        log(f"Building {target_name} components: {', '.join(targets)}  (-j{jobs})…")
    else:
        cmd = [ninja, "-C", llvm_build, f"-j{jobs}"]
        log(f"Building full LLVM tree (-j{jobs})… (no phony targets found)")
    rc, out = run(cmd, verbose=verbose)
    if rc != 0:
        die(f"Build failed:\n{out}")
    log("Build complete")


def install_llvm(llvm_build, jobs, verbose):
    ninja = shutil.which("ninja") or die("ninja not found on PATH")
    log(f"Installing… (-j{jobs})")
    rc, out = run([ninja, "-C", llvm_build, f"-j{jobs}", "install"], verbose=verbose)
    if rc != 0:
        log(f"WARNING: Install failed (continuing with build-tree tools):\n{out}")
        return False
    log("Installed")
    return True


def verify_target(target_name, llvm_install, llvm_build):
    name_lower = target_name.lower()

    candidates = [
        os.path.join(llvm_install, "bin", "llc"),
        os.path.join(llvm_build, "bin", "llc"),
        shutil.which("llc") or "",
    ]

    def has_target(llc_path):
        if not llc_path or not os.path.isfile(llc_path):
            return False
        rc, out = run([llc_path, "--version"])
        if rc != 0:
            return False
        return any(name_lower in line.lower() for line in out.splitlines())

    for llc in candidates:
        if has_target(llc):
            log(f"{target_name} confirmed in `llc --version` ({llc})")
            return True

    # In incremental builds, llc may need explicit relink to pick up new target.
    ninja = shutil.which("ninja")
    if ninja and os.path.isdir(llvm_build):
        log("Relinking llc for verification…")
        rc, out = run([ninja, "-C", llvm_build, "llc"])
        if rc != 0:
            log(f"WARNING: failed to relink llc:\n{out}")
        elif has_target(os.path.join(llvm_build, "bin", "llc")):
            log(f"{target_name} confirmed after llc relink ({os.path.join(llvm_build, 'bin', 'llc')})")
            return True

    log(f"WARNING: '{target_name}' not found in any llc --version output")
    return False


# --------------------------------------------------------------------------- #
# Main                                                                         #
# --------------------------------------------------------------------------- #

def main():
    parser = argparse.ArgumentParser(
        description="Build a custom LLVM backend from a YAML config file.")
    parser.add_argument("config", help="Path to a .build-compiler.yml file")
    args = parser.parse_args()

    if not os.path.isfile(args.config):
        die(f"Config file not found: {args.config}")

    cfg = load_config(args.config)

    llvm_src     = cfg["llvm_src"]
    llvm_build   = cfg["llvm_build"]
    llvm_install = cfg["llvm_install"]
    jobs, link_jobs = compute_build_parallelism(cfg["jobs"])
    backend_dir  = cfg["backend_dir"]
    target_name  = cfg["target_name"] or detect_target_name(backend_dir)

    if not os.path.isdir(backend_dir):
        die(f"backend-dir not found: {backend_dir}")
    if not os.path.isdir(llvm_src):
        die(f"llvm.src-dir not found: {llvm_src}")
    if not os.path.isdir(llvm_build):
        die(f"llvm.build-dir not found: {llvm_build}")

    if cfg.get("cmake_cfg"):
        cmake_cfg = cfg["cmake_cfg"]
        # Infer target name: explicit > tablegen-root stem > directory basename
        if cfg["target_name"]:
            cmake_target = cfg["target_name"]
        else:
            tg_root = cmake_cfg.get("tablegen-root", "")
            stem = os.path.splitext(tg_root)[0] if tg_root else ""
            if stem and re.match(r'^\w+$', stem):
                cmake_target = stem
            else:
                cmake_target = re.sub(r'[^A-Za-z0-9_]', '_', os.path.basename(backend_dir.rstrip("/\\")))
        generate_cmake(backend_dir, cmake_target, cmake_cfg)
        if not cfg["target_name"]:
            target_name = detect_target_name(backend_dir)

    dest = os.path.join(llvm_src, "llvm", "lib", "Target", target_name)
    sync_backend(backend_dir, dest)
    inject_component_groups(dest)

    log(f"{target_name} directory present at {dest} — LLVM will pick it up via "
        f"LLVM_TARGETS_TO_BUILD")
    register_in_all_targets(target_name, llvm_src)
    if target_already_configured(llvm_build, target_name):
        log(f"{target_name} already configured -- skipping CMake reconfigure "
            f"(Ninja self-reconfigures if CMakeLists changed)")
    else:
        cmake_reconfigure(target_name, llvm_src, llvm_build, llvm_install,
                          verbose=False, link_jobs=link_jobs)
    build_target(target_name, llvm_build, jobs, verbose=False)
    install_llvm(llvm_build, jobs, verbose=False)

    if cfg["verify"]:
        verify_target(target_name, llvm_install, llvm_build)

    print()
    log("Done.  Compile C to your target assembly:")
    print(f"  python3 scripts/compile.py <config.compile.yml>")


if __name__ == "__main__":
    main()
