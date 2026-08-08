#!/usr/bin/env python3
"""
simulate.py — Simulate a compiled program in Icarus Verilog and verify
register values against expected results.

Usage:
    python3 scripts/simulate.py <config.simulate.yml>

The YAML file must contain a top-level 'simulate' key.  All relative paths are
resolved against the directory containing the YAML file.

Accepted program formats:
  .asm   assembled automatically; if 'startup.entry' is set a startup stub is
         prepended that loads arguments and calls the named C function.
  .hex   loaded directly — no startup stub is added.

Minimal example (my_cpu.simulate.yml):
    simulate:
      output-dir: ./build/sim
      program: ./build/compile/program.asm
      rtl:
        base-dir: ./rtl
        files:
          - my_pkg.v
          - my_cpu.v
          - my_tb.v
      startup:
        entry: add
        args: [10, 20]
      verify:
        - register: r8
          value: 30

Alternatively, 'rtl.dir' compiles every *.v in a directory (package files first):
    simulate:
      rtl:
        dir: ./rtl
"""

import argparse, os, re, glob, shutil, subprocess, sys

try:
    import yaml
except ImportError:
    print("ERROR: PyYAML is required.  Run: pip3 install pyyaml", file=sys.stderr)
    sys.exit(1)

def log(msg):   print(f"[simulate] {msg}")
def die(msg):   print(f"[simulate] ERROR: {msg}", file=sys.stderr); sys.exit(1)

def run(cmd, *, cwd=None, verbose=False):
    if verbose:
        log("$ " + " ".join(str(c) for c in cmd))
    r = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, text=True)
    if verbose:
        print(r.stdout, end="")
    return r.returncode, r.stdout

def load_config(yaml_path):
    with open(yaml_path) as f:
        doc = yaml.safe_load(f)
    if not isinstance(doc, dict) or "simulate" not in doc:
        die(f"'{yaml_path}' must have a top-level 'simulate' key")
    cfg = doc["simulate"]
    base = os.path.dirname(os.path.abspath(yaml_path))
    def rel(path):
        if not path: return path
        return path if os.path.isabs(path) else os.path.normpath(os.path.join(base, path))

    output_dir = rel(cfg.get("output-dir", "./build/sim"))
    program    = rel(cfg.get("program", "")) or die("'program' is required")

    rtl = cfg.get("rtl", {}) or {}
    rtl_base = rel(rtl.get("base-dir", "")) or ""
    rtl_files_list = rtl.get("files", []) or []
    rtl_dir = rel(rtl.get("dir", "")) or ""
    if rtl_dir:
        # Directory mode: compile every *.v file found in the directory.
        # Package files (*_pkg.v) are ordered first so SystemVerilog imports
        # resolve; an include/ subdir is picked up automatically in compile_rtl.
        if not os.path.isdir(rtl_dir):
            die(f"'rtl.dir' not found: {rtl_dir}")
        found = sorted(glob.glob(os.path.join(rtl_dir, "*.v")))
        if not found:
            die(f"No .v files found in 'rtl.dir': {rtl_dir}")
        pkgs = [f for f in found if f.endswith("_pkg.v")]
        rest = [f for f in found if not f.endswith("_pkg.v")]
        rtl_files = pkgs + rest
    else:
        if not rtl_files_list:
            die("'rtl.files' (or 'rtl.dir') is required and must be non-empty")
        # Resolve each file relative to base-dir (or YAML dir if base-dir not set)
        rtl_files = []
        for f in rtl_files_list:
            if rtl_base and not os.path.isabs(f):
                fpath = os.path.normpath(os.path.join(rtl_base, f))
            else:
                fpath = rel(f) if not os.path.isabs(f) else f
            if not os.path.isfile(fpath):
                die(f"RTL file not found: {fpath}")
            rtl_files.append(fpath)

    startup = cfg.get("startup", {}) or {}
    entry   = startup.get("entry", "") or ""
    sargs   = [int(a) for a in (startup.get("args", []) or [])]

    verify_list = []
    for v in (cfg.get("verify", []) or []):
        reg = v.get("register", "")
        val = v.get("value", None)
        m = re.fullmatch(r"r(\d+)", reg, re.IGNORECASE)
        if not m or val is None:
            die(f"Invalid verify entry {v!r} — use {{register: rN, value: N}}")
        verify_list.append((int(m.group(1)), int(val)))

    options    = cfg.get("options", {}) or {}
    max_cycles = int(options.get("max-cycles", 100_000))
    trace      = bool(options.get("trace", False))
    assembler  = rel(cfg.get("assembler", "")) or ""

    return {
        "output_dir": output_dir, "program": program, "rtl_files": rtl_files,
        "entry": entry, "args": sargs, "verify": verify_list,
        "max_cycles": max_cycles, "trace": trace, "assembler": assembler,
    }

def find_assembler(override):
    if override and os.path.isfile(override):
        return override
    default = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           "..", "template", "tools", "asm.py")
    default = os.path.normpath(default)
    if os.path.isfile(default):
        return default
    die("Could not find a default assembler — set 'assembler' in the YAML")

def _load_immediate(reg, val):
    """Materialize an integer value into a register for the startup stub."""
    val32 = val & 0xFFFFFFFF
    lo = val32 & 0xFFFF
    hi = (val32 >> 16) & 0xFFFF
    if hi == 0:
        return [f"LI   r{reg}, #{lo}"]
    # Values > 0xFFFF: load hi into reg, shift left 16, OR in lo.
    # Uses reg+1 as a temporary scratch register.
    tmp = reg + 1
    lines = [f"LI   r{reg}, #{hi}", f"LUI  r{reg}, r{reg}"]
    if lo != 0:
        lines += [f"LI   r{tmp}, #{lo}", f"OR   r{reg}, r{reg}, r{tmp}"]
    return lines

def make_startup(entry_func, args):
    lines = [
        "; === startup stub (auto-generated by simulate.py) ===",
        "_start:",
        "    LI   r2, #31744        ; SP = 0x7C00 (top of user memory)",
    ]
    for i, val in enumerate(args[:8]):
        for insn in _load_immediate(8 + i, val):
            lines.append(f"    {insn}")
    lines += [f"    CALL {entry_func}", "    HALT", ""]
    return lines

def assemble(asm_path, hex_path, assembler_py):
    rc, out = run([sys.executable, assembler_py, asm_path, "-o", hex_path], verbose=True)
    if rc != 0:
        die(f"Assembly failed:\n{out}")
    log(f"Assembled → {hex_path}")

def compile_rtl(rtl_files, build_dir, sim_bin, trace=False, verbose=False):
    if not shutil.which("iverilog"):
        die("iverilog not found — install: sudo apt install iverilog")
    if not rtl_files:
        die("RTL files list is empty")
    # Collect unique directories from RTL files for include search paths.
    inc_dirs = set()
    for f in rtl_files:
        inc_dirs.add(os.path.dirname(f))
    # Add include/ subdirectories if they exist.
    for d in list(inc_dirs):
        inc_subdir = os.path.join(d, "include")
        if os.path.isdir(inc_subdir):
            inc_dirs.add(inc_subdir)
    # Build iverilog command with include paths.
    inc_flags = []
    for d in sorted(inc_dirs):
        inc_flags.extend(["-I", d])
    cmd = ["iverilog", "-g2012"] + inc_flags + ["-o", sim_bin] + rtl_files
    if trace:
        cmd += ["-DTRACE"]
    log("Compiling RTL (iverilog)…")
    rc, out = run(cmd, cwd=build_dir, verbose=verbose)
    if rc != 0:
        die(f"iverilog compilation failed:\n{out}")
    log("RTL compiled OK")

def run_sim(sim_bin, hex_path, build_dir, max_cycles, verbose):
    log("Running simulation…")
    rc, out = run(["vvp", sim_bin, f"+PROGRAM={hex_path}", f"+MAX_CYCLES={max_cycles}"],
                  cwd=build_dir, verbose=verbose)
    if rc != 0 and "ERROR" in out.upper():
        die(f"Simulation error:\n{out}")
    if not verbose:
        print(out, end="")
    return out

def parse_registers(sim_output):
    regs = {}
    for m in re.finditer(r"r(\d+)\s*(?:\(\w+\))?\s*=\s*0x([0-9a-fA-F]+)", sim_output):
        regs[int(m.group(1))] = int(m.group(2), 16)
    return regs

def verify(sim_output, verify_list):
    if not verify_list: return True
    regs = parse_registers(sim_output)
    passed = failed = 0
    print("\n--- Verification ---")
    for reg_num, expected in verify_list:
        actual = regs.get(reg_num)
        if actual is None:
            print(f"  r{reg_num:2d}  expected={expected:#010x}  actual=NOT FOUND  FAIL")
            failed += 1
        elif actual == expected:
            print(f"  r{reg_num:2d}  expected={expected:#010x}  actual={actual:#010x}  PASS")
            passed += 1
        else:
            print(f"  r{reg_num:2d}  expected={expected:#010x}  actual={actual:#010x}  FAIL"
                  f"  (delta={actual - expected:+d})")
            failed += 1
    print()
    if failed == 0:
        print(f"Result: ALL {passed} checks PASSED")
    else:
        print(f"Result: {failed} FAILED, {passed} passed")
    return failed == 0

def main():
    parser = argparse.ArgumentParser(
        description="Simulate a compiled program from a YAML config file.")
    parser.add_argument("config", help="Path to a .simulate.yml file")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()
    if not os.path.isfile(args.config):
        die(f"Config file not found: {args.config}")

    cfg = load_config(args.config)
    os.makedirs(cfg["output_dir"], exist_ok=True)
    program   = cfg["program"]
    rtl_files = cfg["rtl_files"]
    entry     = cfg["entry"]
    sargs     = cfg["args"]
    build_dir = cfg["output_dir"]
    assembler = find_assembler(cfg["assembler"])

    if not os.path.isfile(program):
        die(f"Program file not found: {program}")

    ext = os.path.splitext(program)[1].lower()
    if ext == ".hex":
        hex_path = program
    elif ext == ".asm":
        stem = os.path.splitext(os.path.basename(program))[0]
        if entry:
            with open(program) as f:
                func_lines = f.read().splitlines()
            defined = [l.rstrip(":") for l in func_lines if re.match(r"^\w+:$", l.strip())]
            if entry not in defined:
                avail = [l for l in defined if not l.startswith(".L")]
                die(f"Entry '{entry}' not found in {program}.\n"
                    f"  Available: {', '.join(avail) or '(none)'}")
            combined = os.path.join(build_dir, f"{stem}_{entry}.asm")
            with open(combined, "w") as f:
                f.write("\n".join(make_startup(entry, sargs)) + "\n")
                f.write("\n".join(func_lines) + "\n")
            log(f"Combined assembly → {combined}")
            hex_path = os.path.join(build_dir, f"{stem}_{entry}.hex")
            assemble(combined, hex_path, assembler)
        else:
            hex_path = os.path.join(build_dir, stem + ".hex")
            assemble(program, hex_path, assembler)
    else:
        die(f"Unsupported program extension '{ext}' — expected .asm or .hex")

    sim_bin = os.path.join(build_dir, "sim")
    compile_rtl(rtl_files, build_dir, sim_bin, trace=cfg["trace"], verbose=args.verbose)
    output = run_sim(sim_bin, hex_path, build_dir, cfg["max_cycles"], verbose=args.verbose)

    if cfg["verify"]:
        ok = verify(output, cfg["verify"])
        sys.exit(0 if ok else 1)

if __name__ == "__main__":
    main()
