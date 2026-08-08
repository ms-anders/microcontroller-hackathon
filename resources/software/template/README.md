# LLVM Backend Template

A **fill-in-the-blanks LLVM backend** for the ADI MCU Hackathon. All of the LLVM
*plumbing* is done for you; your job is to describe **your** instruction set and
teach the compiler how to use it.

The template uses the placeholder target name **`MYISA`** (files are named
`MYISA*.cpp`, the namespace is `MYISA`, the `-march` name is `myisa`). You can
keep this name or rename it (see *Renaming* below).

> **What "template" means here:** the boilerplate files compile and work as-is.
> The nine ISA-specific files have had their bodies replaced with `// TODO:`
> markers and short guidance. The backend will **not build end-to-end until you
> complete the TODOs** — that is the exercise.

---

## What's in this template

- **`llvm-backend/`** — a fill-in-the-blanks LLVM backend (the main exercise).
- **`tools/`** — a ready-to-use Python assembler: a generic `asm.py` engine plus
  an editable `isa_config.py` that ships with a working example encoding. Copy
  both into your project and edit only `isa_config.py` for your ISA:

  ```bash
  python3 tools/asm.py program.asm -o program.hex
  ```

- **`my_cpu.*.yml`** — starter config files for the build/compile/simulate/
  synthesize tooling in `../scripts/`. Edit the paths and names for your ISA:

  ```bash
  python3 ../scripts/build_compiler.py my_cpu.build-compiler.yml
  python3 ../scripts/compile.py        my_cpu.compile.yml
  python3 ../scripts/simulate.py       my_cpu.simulate.yml
  python3 ../scripts/synthesize.py     my_cpu.synthesize.yml
  ```

---

## Where the step-by-step guidance lives

You are not expected to know LLVM already. Three resources walk you through it,
from most guided to most detailed:

1. **The `// TODO:` comments in each file** — the fastest reference. Every TODO
   says what to add and shows a short inline example.
2. **The ADI MCU Hackathon tutorials** — the guided path, with copy-pasteable code, in
   the *same order* as the files below (see the
   [Tutorial Workbook PDF](../../../docs/tutorial-workbook.pdf)):
   - **Stage 1 — Data Operations** — registers,
     instruction formats, ALU instructions, and the data-layout string
   - **Stage 2 — Control Flow** — compare
     and branch instructions, `LowerBR_CC`, branch selection
   - **Stage 3 — Function Calls** — calling
     convention, calls, and prologue/epilogue
3. **[llvm-backend/INSTALL.md](llvm-backend/INSTALL.md)** — a glossary of every
   LLVM concept, a file-by-file reference, and a complete *Porting Walkthrough*
   (Part 2) covering each file with examples, common mistakes, and debugging
   tips.

---

## Files you must complete (the TODOs live here)

Fill these in roughly in this order — it mirrors the tutorial stages
(data ops → control flow → functions):

| # | File | What you implement |
|---|------|--------------------|
| 1 | `llvm-backend/MYISARegisterInfo.td` | Your registers and register classes |
| 2 | `llvm-backend/MYISAInstrFormats.td` | Your instruction bit-field encodings |
| 3 | `llvm-backend/MYISAInstrInfo.td` | Instruction definitions + selection patterns |
| 4 | `llvm-backend/MYISACallingConv.td` | Argument / return / callee-saved registers |
| 5 | `llvm-backend/MYISATargetMachine.cpp` | The data-layout string |
| 6 | `llvm-backend/MYISAISelLowering.cpp` | Which operations are legal / expanded / custom; branch lowering |
| 7 | `llvm-backend/MYISAISelDAGToDAG.cpp` | Constant materialisation, call/compare/branch selection, addressing modes |
| 8 | `llvm-backend/MYISAFrameLowering.cpp` | Function prologue / epilogue |
| 9 | `llvm-backend/TargetInfo/MYISATargetInfo.cpp` | Your `-march` name and description |

When you get stuck, the ADI MCU Hackathon tutorials (Stages 1–3) and each file's
`// TODO:` comments walk you through exactly what to write.

Each redacted file keeps its original explanatory header comment describing
**what the file is for** and **why LLVM needs it** — read those first.

## Files you should NOT need to touch (boilerplate)

Everything else is target-independent wiring and can be left as-is:
`MYISA.td`, `MYISA.h`, `MYISASubtarget.*`, `MYISATargetObjectFile.h`,
`MYISAInstrInfo.cpp`, `MYISARegisterInfo.cpp`, `MYISAAsmPrinter.cpp`,
`MYISAInstrInfo.h`, `MYISARegisterInfo.h`, `MYISAFrameLowering.h`,
`MYISAISelLowering.h`, `MYISATargetMachine.h`, the whole `MCTargetDesc/`
directory, `TargetInfo/CMakeLists.txt`, and the top-level `CMakeLists.txt`.

### Important: names the boilerplate expects

The boilerplate `.cpp` files that stay untouched still refer to a handful of
instructions and registers **by name**. When you write your `.td` files, either
reuse these names or update the reference in the C++ file listed:

| Name used by boilerplate | Where | What it must be |
|--------------------------|-------|-----------------|
| `MOV_rr` | `MYISAInstrInfo.cpp` (`copyPhysReg`) | register-to-register move |
| `LOAD_reg` / `STORE_reg` | `MYISAInstrInfo.cpp` (spill / reload) | register-indirect load / store |
| `PUSH` / `POP` | `MYISAFrameLowering.cpp` (your TODO) | stack push / pop |
| `LI`, `ADD_rri`, `SUB_rri`, `NEG_rr` | `MYISAISelDAGToDAG.cpp` / `MYISAFrameLowering.cpp` | immediate / arithmetic ops |
| `CMP_rr` / `CMP_ri`, `JZ`/`JNZ`/`JLT`/`JGT`, `CALL` | `MYISAISelDAGToDAG.cpp` (your TODO) | compare, branches, call |
| `R0`–`R7`, `R2` (SP), `R3` (LR), `R30` (FP) | `MYISARegisterInfo.cpp`, `MYISAFrameLowering.cpp` | reserved / special registers |

## Renaming the target (optional)

To use your own name instead of `MYISA`, find-and-replace across this template
directory, keeping the casing variants consistent:

- `MYISA` → `YOURNAME` (file names and contents)
- `myisa` → `yourname` (the `-march` target-name strings)
- `MYISAISD` → `YOURNAMEISD` (the custom SDNode namespace)

## Building / integrating the backend

Integrate the backend into the container's pre-built LLVM tree and do an
incremental rebuild:

```bash
python3 ../scripts/build_llvm_target.py llvm-backend/ --target-name MYISA --verify
```

`--verify` checks that your target shows up in `llc --version`. Once it builds,
you can lower LLVM IR or C with the toolchain in `../scripts/` (see
`compile.py` and the `*.compile.yml` configs).

## A worked example: one instruction, end to end

`MYISAInstrInfo.td` ships with one instruction already filled in — integer add.
It shows the shape every instruction definition follows:

```tablegen
def ADD_rrr : Type1RRR<0x0000, "ADD",                    // record name, format, opcode, mnemonic
  (outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2),              // outputs / inputs
  "ADD\t$rd, $rs1, $rs2",                                // assembly text
  [(set GPR:$rd, (add GPR:$rs1, GPR:$rs2))]>;            // SelectionDAG pattern
```

The last line is the important one: it tells the compiler *"whenever you see an
LLVM `add` of two registers, emit this instruction."* That single pattern is
what lets C's `a + b` compile to `ADD`. To add subtraction you copy this, change
the opcode and mnemonic, and change `(add …)` to `(sub …)`. Most ALU
instructions follow this exact template — see the **Stage 1 — Data Operations**
section of the [Tutorial Workbook PDF](../../../docs/tutorial-workbook.pdf) for the full set.

## Recommended workflow: build a little, test a little

Don't try to complete every TODO before building. Work in small increments and
rebuild often:

1. Fill in your registers, one instruction format, and the `ADD` instruction.
2. Build with `build_llvm_target.py … --verify`.
3. Compile the simplest possible program and read the assembly:

   ```c
   int add(int a, int b) { return a + b; }
   ```

   ```bash
   /opt/llvm/bin/clang --target=myisa -S -O1 -o test.s test.c
   # or, starting from LLVM IR:
   /opt/llvm/bin/llc -march=myisa -O1 -o test.s test.ll
   ```

4. Add the next group of instructions and repeat, growing complexity in this
   order (each step exercises more of the backend):
   **arithmetic → local variables (stack) → function calls → conditionals →
   loops → arrays → recursion.**

If a build or compile fails, the *Common Mistakes* and *Debugging* sections in
[llvm-backend/INSTALL.md](llvm-backend/INSTALL.md) list the usual causes
(unreserved registers, pattern type mismatches, missing `Defs`/`Uses`, stack
offsets that exceed your immediate width, …).
