#!/usr/bin/env python3
"""
compile.py — Compile C and/or assembly source files for a custom ISA.

Usage:
    python3 scripts/compile.py <config.compile.yml>

The YAML file must contain a top-level 'compile' key.  All relative paths are
resolved against the directory containing the YAML file.

Workflow for C sources:
  1.  Each .c file  →  LLVM IR (.ll)  via  clang -emit-llvm -S
  2.  All .ll files →  linked.ll       via  llvm-link  (single file: skip)
  3.  linked.ll     →  compiled.s      via  llc -march=<target>
  4.  compiled.s is converted to assembler-compatible .asm format

Workflow for .asm sources:
  Included as-is after the compiled C output.

Final outputs (in output-dir):
  <name>.asm        always produced
  <name>.hex        if output.hex is true (assembled by the ISA assembler)

Minimal example (my_cpu.compile.yml):
    compile:
      output-dir: ./build/compile
      compiler:
        target: myisa
      output:
        hex: true
      groups:
        - group: Sources
          files:
            - file: ./tests/test_sim.c
"""

import argparse
import os
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

def log(msg):   print(f"[compile] {msg}")
def die(msg):   print(f"[compile] ERROR: {msg}", file=sys.stderr); sys.exit(1)


def run(cmd, *, cwd=None, verbose=False):
    if verbose:
        log("$ " + " ".join(str(c) for c in cmd))
    r = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, text=True)
    if verbose:
        print(r.stdout, end="")
    return r.returncode, r.stdout


# --------------------------------------------------------------------------- #
# YAML loading                                                                 #
# --------------------------------------------------------------------------- #

def load_config(yaml_path):
    with open(yaml_path) as f:
        doc = yaml.safe_load(f)
    if not isinstance(doc, dict) or "compile" not in doc:
        die(f"'{yaml_path}' must have a top-level 'compile' key")
    cfg = doc["compile"]
    base = os.path.dirname(os.path.abspath(yaml_path))

    def rel(path):
        if not path:
            return path
        return path if os.path.isabs(path) else os.path.normpath(os.path.join(base, path))

    output_dir = rel(cfg.get("output-dir", "./build/compile"))

    compiler = cfg.get("compiler", {}) or {}
    llvm_install = rel(compiler.get("install-dir", "")) or os.environ.get("LLVM_INSTALL", "/opt/llvm")
    target       = compiler.get("target", "") or die("'compiler.target' is required")
    misc_flags   = compiler.get("misc", []) or []      # extra clang flags
    codegen_flags = compiler.get("codegen", []) or []  # extra llc flags
    add_paths    = [rel(p) for p in (compiler.get("add-path", []) or [])]

    output_cfg = cfg.get("output", {}) or {}
    emit_asm = bool(output_cfg.get("asm", True))
    emit_hex = bool(output_cfg.get("hex", False))
    out_name = output_cfg.get("name", "program") or "program"
    assembler = rel(output_cfg.get("assembler", "")) or ""

    # Collect source files from groups
    sources = []
    for group in (cfg.get("groups", []) or []):
        for fentry in (group.get("files", []) or []):
            fpath = fentry.get("file", "") if isinstance(fentry, dict) else str(fentry)
            if fpath:
                sources.append(rel(fpath))

    if not sources:
        die("No source files found — add at least one entry under 'groups'")

    startup = cfg.get("startup", {}) or {}
    startup_entry = startup.get("entry", "") or ""
    startup_args  = [int(a) for a in (startup.get("args", []) or [])]

    return {
        "output_dir":    output_dir,
        "llvm_install":  llvm_install,
        "target":        target,
        "misc_flags":    [str(f) for f in misc_flags],
        "codegen_flags": [str(f) for f in codegen_flags],
        "add_paths":     add_paths,
        "emit_asm":      emit_asm,
        "emit_hex":      emit_hex,
        "out_name":      out_name,
        "assembler":     assembler,
        "sources":       sources,
        "startup_entry": startup_entry,
        "startup_args":  startup_args,
    }


# --------------------------------------------------------------------------- #
# Tool discovery                                                               #
# --------------------------------------------------------------------------- #

# --------------------------------------------------------------------------- #
# Startup stub generation                                                      #
# --------------------------------------------------------------------------- #

def _load_immediate(reg, val):
    """Materialise an integer into a register for the startup stub."""
    val32 = val & 0xFFFFFFFF
    lo = val32 & 0xFFFF
    hi = (val32 >> 16) & 0xFFFF
    if hi == 0:
        return [f"LI   r{reg}, #{lo}"]
    tmp = reg + 1
    lines = [f"LI   r{reg}, #{hi}", f"LUI  r{reg}, r{reg}"]
    if lo != 0:
        lines += [f"LI   r{tmp}, #{lo}", f"OR   r{reg}, r{reg}, r{tmp}"]
    return lines


def make_startup_lines(entry_func, args):
    """Return lines for a _start stub that calls entry_func with args."""
    lines = [
        "; === _start stub (auto-generated by compile.py) ===",
        "_start:",
        "    LI   r2, #31744        ; SP = 0x7C00 (top of user memory)",
    ]
    for i, val in enumerate(args[:8]):
        for insn in _load_immediate(8 + i, val):
            lines.append(f"    {insn}")
    lines += [f"    CALL {entry_func}", "    HALT", ""]
    return lines


def find_llvm_tool(name, llvm_install):
    candidate = os.path.join(llvm_install, "bin", name)
    if os.path.isfile(candidate):
        return candidate
    found = shutil.which(name)
    if found:
        return found
    die(f"'{name}' not found — set LLVM_INSTALL or add it to PATH")


def find_assembler(assembler_override):
    if assembler_override and os.path.isfile(assembler_override):
        return assembler_override
    script_dir = os.path.dirname(os.path.abspath(__file__))
    default    = os.path.normpath(os.path.join(
        script_dir, "..", "template", "tools", "asm.py"))
    if os.path.isfile(default):
        return default
    die("Could not find a default assembler — set 'output.assembler' in the YAML")


# --------------------------------------------------------------------------- #
# Compilation                                                                  #
# --------------------------------------------------------------------------- #

def compile_c_to_ir(c_path, ll_path, clang, misc_flags, add_paths, verbose):
    """clang -emit-llvm -S → LLVM IR."""
    inc_flags = [f for p in add_paths for f in ["-I", p]]
    cmd = [clang, "-emit-llvm", "-S"] + misc_flags + inc_flags + ["-o", ll_path, c_path]
    log(f"  clang: {os.path.basename(c_path)} → {os.path.basename(ll_path)}")
    rc, out = run(cmd, verbose=verbose)
    if rc != 0:
        die(f"clang failed on {c_path}:\n{out}")


def link_ir_files(ll_files, linked_ll, llvm_link, verbose):
    """llvm-link all IR files into one."""
    cmd = [llvm_link, "-S"] + ll_files + ["-o", linked_ll]
    log(f"  llvm-link: {len(ll_files)} files → {os.path.basename(linked_ll)}")
    rc, out = run(cmd, verbose=verbose)
    if rc != 0:
        die(f"llvm-link failed:\n{out}")


def compile_ir_to_asm(ll_path, s_path, llc, target, codegen_flags, verbose):
    """llc -march=<target> → target assembly."""
    cmd = [llc, f"-march={target}"] + codegen_flags + ["-o", s_path, ll_path]
    log(f"  llc: {os.path.basename(ll_path)} → {os.path.basename(s_path)} "
        f"(-march={target})")
    rc, out = run(cmd, verbose=verbose)
    if rc != 0:
        die(f"llc failed:\n{out}")


# --------------------------------------------------------------------------- #
# Assembly format conversion                                                   #
# --------------------------------------------------------------------------- #

def parse_llvm_asm(asm_path):
    """
    Convert LLVM-generated .s output to lines compatible with the assembler.

    Keeps: label definitions (ending in ':') and instruction lines.
    Drops: assembler directives (starting with '.') and pure-comment lines.

    Register-indirect memory operands like [r30 + -8] are passed through
    directly — the assembler handles this syntax natively.
    """
    result = []
    with open(asm_path) as f:
        for raw in f:
            stripped = raw.strip()
            if not stripped or stripped.startswith(";"):
                continue
            code = stripped.split(";")[0].strip()
            if not code:
                continue
            if code.endswith(":"):
                result.append(code)
                continue
            if code.startswith("."):
                continue
            result.append(code)
    return result


def read_asm_file(asm_path):
    """Read a hand-written .asm file, stripping blank lines."""
    with open(asm_path) as f:
        lines = [l.rstrip("\n") for l in f]
    return lines


# --------------------------------------------------------------------------- #
# Assembly                                                                     #
# --------------------------------------------------------------------------- #

def assemble(asm_path, hex_path, assembler_py, verbose):
    cmd = [sys.executable, assembler_py, asm_path, "-o", hex_path]
    log(f"  assemble: {os.path.basename(asm_path)} → {os.path.basename(hex_path)}")
    rc, out = run(cmd, verbose=verbose)
    if rc != 0:
        die(f"Assembler failed:\n{out}")
    if verbose:
        print(out, end="")


# --------------------------------------------------------------------------- #
# Main                                                                         #
# --------------------------------------------------------------------------- #

def main():
    parser = argparse.ArgumentParser(
        description="Compile C/ASM sources to target assembly from a YAML config.")
    parser.add_argument("config", help="Path to a .compile.yml file")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    if not os.path.isfile(args.config):
        die(f"Config file not found: {args.config}")

    cfg = load_config(args.config)
    os.makedirs(cfg["output_dir"], exist_ok=True)

    llvm_install  = cfg["llvm_install"]
    target        = cfg["target"]
    misc_flags    = cfg["misc_flags"]
    codegen_flags = cfg["codegen_flags"]
    add_paths     = cfg["add_paths"]
    out_name      = cfg["out_name"]
    sources       = cfg["sources"]
    verbose       = args.verbose

    clang      = find_llvm_tool("clang",      llvm_install)
    llc        = find_llvm_tool("llc",        llvm_install)
    llvm_link  = find_llvm_tool("llvm-link",  llvm_install)
    assembler  = find_assembler(cfg["assembler"])

    # Separate C and ASM sources
    c_sources   = [s for s in sources if s.lower().endswith(".c")]
    asm_sources = [s for s in sources if s.lower().endswith(".asm")]
    unknown     = [s for s in sources if s not in c_sources + asm_sources]
    if unknown:
        die(f"Unsupported source file type(s): {unknown}\n"
            f"Supported: .c, .asm")

    # Validate all source files exist
    for s in sources:
        if not os.path.isfile(s):
            die(f"Source file not found: {s}")

    build = cfg["output_dir"]
    combined_lines = []

    # ---- Compile C sources ----
    if c_sources:
        log(f"Compiling {len(c_sources)} C file(s)…")
        ll_files = []
        for c in c_sources:
            stem = os.path.splitext(os.path.basename(c))[0]
            ll   = os.path.join(build, stem + ".ll")
            compile_c_to_ir(c, ll, clang, misc_flags, add_paths, verbose)
            ll_files.append(ll)

        if len(ll_files) == 1:
            linked_ll = ll_files[0]
        else:
            linked_ll = os.path.join(build, "linked.ll")
            link_ir_files(ll_files, linked_ll, llvm_link, verbose)

        compiled_s = os.path.join(build, out_name + ".s")
        compile_ir_to_asm(linked_ll, compiled_s, llc, target, codegen_flags, verbose)
        combined_lines += parse_llvm_asm(compiled_s)

    # ---- Include .asm sources ----
    if asm_sources:
        log(f"Including {len(asm_sources)} .asm file(s)…")
        for asm in asm_sources:
            log(f"  {os.path.basename(asm)}")
            combined_lines += [""] + read_asm_file(asm)

    # ---- Prepend startup stub (if requested) ----
    startup_entry = cfg["startup_entry"]
    if startup_entry:
        stub = make_startup_lines(startup_entry, cfg["startup_args"])
        combined_lines = stub + combined_lines
        log(f"  startup stub: _start → CALL {startup_entry} → HALT")

    # ---- Write combined .asm ----
    asm_out = os.path.join(build, out_name + ".asm")
    with open(asm_out, "w") as f:
        f.write("\n".join(combined_lines) + "\n")
    log(f"Wrote {asm_out}")

    # ---- Assemble to hex ----
    if cfg["emit_hex"]:
        hex_out = os.path.join(build, out_name + ".hex")
        assemble(asm_out, hex_out, assembler, verbose)
        log(f"Wrote {hex_out}")

    print()
    log("Done.")
    print(f"  Assembly:  {asm_out}")
    if cfg["emit_hex"]:
        print(f"  Hex image: {os.path.join(build, out_name + '.hex')}")


if __name__ == "__main__":
    main()
