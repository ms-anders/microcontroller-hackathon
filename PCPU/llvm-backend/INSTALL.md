# MYISA LLVM Backend — Template

This directory is a **fill-in-the-blanks LLVM backend skeleton** for building a
compiler for your own instruction set (placeholder target name **MYISA**). All
of the LLVM *infrastructure* (target registration, subtarget wiring, MC layer,
assembly printing, register-allocator glue) is already written for you. The
*ISA-specific* parts — your registers, instruction encodings, instruction
definitions, calling convention, operation legality, instruction selection and
prologue/epilogue — are stubbed out and marked with `// TODO:` comments for you
to complete.

It is modelled on a 32-bit, fixed-width, little-endian RISC architecture with a
16-bit address space. Once completed, the backend compiles C code (via Clang)
or LLVM IR (via LLC) down to your assembly text. See `../README.md` for the
recommended order in which to fill in the TODOs.

> ### How to use this document
>
> - **Look for `// TODO:` markers** in the nine ISA-specific files — each one
>   tells you what to implement and shows a worked example inline.
> - **Part 1 (below) is a glossary + reference.** The specific numbers it
>   quotes for MYISA (32 registers with fixed roles, the Type 1 / Type 2
>   encodings, the `e-p:…` data-layout string, the r8–r15 calling convention,
>   etc.) describe the **example ISA this template is modelled on** — they are
>   exactly the design decisions *you* replace. Read Part 1 to understand what
>   each file and concept is *for*, not as a spec you must copy.
> - **Part 2 is a step-by-step porting walkthrough** with examples for every
>   file.
> - **The ADI MCU Hackathon tutorials are the guided path.** Stages 1–3 build the MYISA
>   backend incrementally with copy-pasteable code, in the same order as the
>   TODOs (see the `docs/tutorial-workbook.pdf`):
>     - **Stage 1 — Data Operations** — registers, formats, ALU
>       instructions, data layout
>     - **Stage 2 — Control Flow** — compares and branches
>     - **Stage 3 — Function Calls** — calling convention, calls,
>       prologue/epilogue
> - **Stuck?** Each `// TODO:` includes an inline example, and the Stage 1–3
>   tutorials show copy-pasteable code for every file.

This document has two parts:
1. **Part 1 — Glossary & Architecture**: what every file is, what every concept means, and how the pieces fit together
2. **Part 2 — Porting Walkthrough**: step-by-step instructions for adapting this backend to your own ISA

---

# Part 1 — Glossary & Architecture

## LLVM Concepts

| Term | What It Means |
|------|---------------|
| **LLVM IR** | LLVM's Intermediate Representation — a typed, SSA-form language that all frontends (Clang, Rust, etc.) compile down to. The backend's job is to turn this into machine code. |
| **SelectionDAG** | A directed acyclic graph of operations (ADD, LOAD, STORE, etc.) representing one basic block. The backend pattern-matches this graph into machine instructions. |
| **Legalization** | The process of transforming a SelectionDAG until every node is something the target can actually execute. Operations the hardware doesn't support get expanded into sequences of operations it does support. |
| **ISel (Instruction Selection)** | Pattern-matching DAG nodes to concrete machine instructions. Mostly driven by patterns in `.td` files; complex cases use C++ in `ISelDAGToDAG.cpp`. |
| **MachineInstr** | A concrete machine instruction with operands that are either virtual registers (before allocation) or physical registers (after allocation). |
| **MCInst** | The lowest-level instruction representation — fully resolved, ready to be printed as assembly text or encoded as binary. |
| **TableGen** | LLVM's domain-specific language (`.td` files) for declaratively describing registers, instructions, and patterns. A tool called `llvm-tblgen` reads these and generates C++ `.inc` files that are `#include`d into the backend. |
| **Register Allocator** | A built-in LLVM pass that assigns virtual registers to physical registers and inserts spill/reload code when registers run out. You configure it; you don't implement it. |
| **Frame Lowering** | The pass that inserts function prologue (save registers, allocate stack) and epilogue (restore registers, deallocate stack) code. |
| **Glue** | A special DAG edge that forces two nodes to be scheduled immediately adjacent (used for flag-producing/consuming pairs like CMP→BR). |
| **Chain** | A special DAG edge that enforces ordering between side-effecting operations (loads, stores, calls). |
| **Calling Convention** | The rules for how function arguments are passed (which registers, what order, when to spill to stack) and how return values come back. |
| **Data Layout String** | A compact notation that tells LLVM your pointer width, alignment rules, endianness, and native integer sizes. |

---

## File-by-File Reference

### Top-Level Build & Configuration

| File | What It Is |
|------|------------|
| `CMakeLists.txt` | CMake build script. Runs TableGen to generate `.inc` files, compiles C++ sources into the `LLVMMYISACodeGen` library, and descends into subdirectories. |
| `MYISA.td` | Root TableGen file. `#include`s all other `.td` files. Defines subtarget features (like `FeatureExtMul`) and processor models. This is the file you point `LLVM_TARGET_DEFINITIONS` at. |
| `MYISA.h` | Shared C++ header. Forward-declares factory functions (`createMYISAISelDag()`) and the target accessor (`getTheMYISATarget()`). Every `.cpp` in the backend includes this. |

### TableGen Descriptions (`.td` files)

| File | What It Describes |
|------|-------------------|
| `MYISARegisterInfo.td` | Every physical register (r0–r31), their hardware encoding (5-bit), and register classes (GPR for general-purpose, CREG for the condition register, SP for the stack pointer). The allocation order is defined here — caller-saved registers first so the allocator prefers them. |
| `MYISAInstrFormats.td` | The binary encoding formats. MYISA has two: Type 1 (ALU, bit 31=1, 14-bit opcode) and Type 2 (Memory/Control, bit 31=0, 9-bit opcode). Each format is a TableGen class that maps operands to bit positions in the 32-bit instruction word. |
| `MYISAInstrInfo.td` | Every instruction in the ISA: its format, opcode, operands, assembly syntax string, and (where possible) a SelectionDAG pattern for automatic instruction selection. Also defines operand types (immediates with range checks, memory operands, branch targets) and custom SDNode types. |
| `MYISACallingConv.td` | The calling convention rules: arguments go in r8–r15 then stack; return value in r8; r16–r31 are callee-saved. LLVM's TableGen generates `CC_MYISA()` and `RetCC_MYISA()` functions from this. |

### Code Generation (C++ files)

| File | What It Does |
|------|--------------|
| `MYISATargetMachine.cpp/.h` | The entry point. Declares the data layout string (`"e-p:16:16-i32:32-n32-S32"`), creates the pass pipeline, and instantiates the subtarget. When LLVM needs to compile for MYISA, it starts here. |
| `MYISASubtarget.cpp/.h` | The wiring hub. Owns instances of InstrInfo, RegInfo, FrameLowering, and TargetLowering. Parses feature strings (like `+ext-mul`). Every sub-component reaches its siblings through the Subtarget. |
| `MYISAISelLowering.cpp/.h` | Legalization and custom lowering. Tells LLVM which operations are Legal (hardware instruction exists), Expand (synthesize from other ops), or Custom (handled by C++ code in `LowerOperation()`). Implements lowering for calls, returns, branches, and global addresses. |
| `MYISAISelDAGToDAG.cpp` | The instruction selection pass. Most patterns come from TableGen, but this file handles cases that can't be expressed declaratively: constant materialization (choosing between 5-bit immediate, LI, negate, or LUI+OR), memory address decomposition, and call sequence pseudos. |
| `MYISAFrameLowering.cpp/.h` | Prologue/epilogue generation. Saves the link register and callee-saved registers with PUSH, allocates the stack frame by subtracting from SP (in chunks of 31 since the immediate is only 5 bits wide), and mirrors everything in the epilogue. |
| `MYISAInstrInfo.cpp/.h` | Physical register operations that the register allocator needs: `copyPhysReg()` (move between registers), `storeRegToStackSlot()` (spill), and `loadRegFromStackSlot()` (reload). |
| `MYISARegisterInfo.cpp/.h` | Tells LLVM which registers are reserved (r0–r7 cannot be allocated), provides the allocatable register class, and implements `eliminateFrameIndex()` which converts abstract stack slot references into SP+offset addressing. |
| `MYISAAsmPrinter.cpp` | The bridge between the code generator and the MC layer. Converts each `MachineInstr` into an `MCInst` by mapping operands (registers → `MCOperand::createReg()`, immediates → `MCOperand::createImm()`, etc.) and emitting it to the assembly streamer. |
| `MYISATargetObjectFile.h` | Configures ELF section selection (which section code/data/BSS goes into). Currently uses defaults. |

### MC Layer (`MCTargetDesc/`)

| File | What It Does |
|------|--------------|
| `MYISAMCTargetDesc.cpp/.h` | Registers MC-layer components with LLVM: creates `MCInstrInfo`, `MCRegisterInfo`, `MCSubtargetInfo`, and the `InstPrinter`. Runs during target initialization. |
| `MYISAMCAsmInfo.h` | Assembly syntax configuration: comment character (`;`), label suffix (`:`), uses `.` as PC reference, alignment treated as bytes, has no dot-loc directives. |
| `MYISAInstPrinter.cpp/.h` | Renders `MCInst` as assembly text. Implements `printOperand()` (registers and immediates) and `printMemOperand()` (base+offset format). |

### Target Registration (`TargetInfo/`)

| File | What It Does |
|------|--------------|
| `MYISATargetInfo.cpp` | Registers the string `"myisa"` with LLVM's target registry so that `-march=myisa` and `--target=myisa` resolve to this backend. Contains the `Target` singleton. |

---

## How Data Flows Through the Backend

```
  C source (.c)
     │  Clang frontend
     ▼
  LLVM IR (.ll)
     │  SelectionDAG Builder (built into LLVM)
     ▼
  SelectionDAG (target-independent)
     │  MYISAISelLowering — legalization & custom lowering
     ▼
  SelectionDAG (legal for MYISA)
     │  MYISAISelDAGToDAG — pattern matching via TableGen + C++
     ▼
  MachineInstr (virtual registers)
     │  Register Allocator (built into LLVM, configured by MYISARegisterInfo)
     ▼
  MachineInstr (physical registers)
     │  MYISAFrameLowering — prologue/epilogue insertion
     ▼
  MachineInstr (frame indices resolved by MYISARegisterInfo)
     │  MYISAAsmPrinter — MachineInstr → MCInst conversion
     ▼
  MCInst
     │  MYISAInstPrinter — MCInst → assembly text
     ▼
  Assembly output (.s file)
```

---

## What TableGen Generates

The `CMakeLists.txt` invokes `llvm-tblgen` multiple times to produce `.inc` files from the `.td` descriptions:

| Generated File | Flag | Contents |
|---|---|---|
| `MYISAGenRegisterInfo.inc` | `-gen-register-info` | Register enum, class membership, encoding tables, sub-register relationships |
| `MYISAGenInstrInfo.inc` | `-gen-instr-info` | Instruction opcode enum, operand descriptors, flags, implicit defs/uses |
| `MYISAGenDAGISel.inc` | `-gen-dag-isel` | The `SelectCode()` function body — pattern-matching code for instruction selection |
| `MYISAGenCallingConv.inc` | `-gen-callingconv` | `CC_MYISA()` and `RetCC_MYISA()` functions |
| `MYISAGenSubtargetInfo.inc` | `-gen-subtarget` | Feature flag parsing, scheduling model tables |
| `MYISAGenAsmWriter.inc` | `-gen-asm-writer` | `printInstruction()` and `getRegisterName()` functions |

These are `#include`d directly into C++ files. You never edit them.

---

## MYISA Architecture Summary

| Parameter | Value |
|---|---|
| Word size | 32-bit |
| Address space | 16-bit (64 KB) |
| Registers | 32 general-purpose (r0–r31) |
| Instruction width | Fixed 32-bit |
| Endianness | Little-endian |
| Instruction types | Type 1 (ALU), Type 2 (Memory/Control) |

### Registers

| Register | Role | Allocatable? |
|---|---|---|
| r0 | Hard-wired zero | No |
| r1 | Program counter | No |
| r2 | Stack pointer (SP) | No |
| r3 | Link register (return address) | No |
| r4 | Thread pointer (reserved) | No |
| r5 | Condition register (CMP result) | No |
| r6 | ISR status (reserved) | No |
| r7 | Reserved | No |
| r8–r15 | Argument/return (caller-saved) | Yes |
| r16–r31 | Callee-saved general purpose | Yes |

### Calling Convention

- **Arguments:** r8–r15 (up to 8), then stack (4-byte aligned)
- **Return value:** r8
- **Caller-saved:** r8–r15
- **Callee-saved:** r16–r31
- **Stack growth:** Downward
- **Stack alignment:** 4 bytes

### Instruction Encoding

**Type 1** — ALU (bit 31 = 1):
```
[31]    = 1 (type bit)
[30:17] = opcode (14 bits)
[16:11] = arg1 (6 bits: 1 flag + 5 value)
[10:5]  = arg2 (6 bits: 1 flag + 5 value)
[4:0]   = result register (5 bits)
```

**Type 2** — Memory / Control (bit 31 = 0):
```
[31]    = 0 (type bit)
[30:22] = opcode (9 bits)
[21]    = reg/imm flag
[20:16] = register (5 bits)
[15:0]  = address/offset (16 bits)
```

---

## Build Prerequisites

| Requirement | Version | Notes |
|---|---|---|
| LLVM source tree | 17.x or 18.x | Tested with LLVM 17 |
| CMake | ≥ 3.20 | Required by LLVM |
| Ninja | any | Recommended over Make |
| C++ compiler | C++17 | GCC ≥ 9, Clang ≥ 10, MSVC ≥ 2019 |
| Python | ≥ 3.6 | Used by LLVM's lit test framework |
| Disk space | ~30 GB | Debug build; ~10 GB for Release |
| RAM | ~16 GB | Use `-DLLVM_USE_LINKER=lld` to reduce |

---

## Build & Test

```bash
# 1. Copy into LLVM source tree
cp -r llvm-backend/ <llvm-project>/llvm/lib/Target/MYISA/

# 2. Register with LLVM (add "MYISA" to LLVM_ALL_TARGETS in llvm/CMakeLists.txt)

# 3. Configure
cd <llvm-project> && mkdir build && cd build
cmake -G Ninja ../llvm \
  -DLLVM_TARGETS_TO_BUILD="MYISA" \
  -DLLVM_ENABLE_PROJECTS="clang" \
  -DCMAKE_BUILD_TYPE=Debug

# 4. Build (~15-30 minutes)
ninja

# 5. Test
./bin/clang --target=myisa -S -O1 -o test.s test.c
./bin/llc -march=myisa -O1 -o test.s test.ll
```

---

## What This Backend Provides

- C → MYISA assembly via Clang + LLC
- All basic integer operations (add, sub, mul, and, or, xor, shifts)
- Comparisons and conditional branches
- Function calls with full calling convention
- Register allocation with spill/reload
- Automatic prologue/epilogue generation
- Multi-strategy constant materialization (5-bit imm, LI, negate, LUI+OR)

## What It Does NOT Provide (Yet)

| Missing | Impact | Effort |
|---|---|---|
| MCCodeEmitter | No `.o` object output | Medium |
| AsmParser | No `.s` → `.o` assembly | Medium |
| Disassembler | No objdump support | Low |
| ELF Relocations | No multi-file linking | Medium |
| LLD support | No final executables | High |
| Debug info (DWARF) | No source-level debugging | Medium |
| Tail calls | All calls use full frame | Low |
| Scheduling model | No latency-based reordering | Low |

---
---

# Part 2 — Porting This Backend to Your Own ISA

This section walks you through adapting the MYISA backend to compile for your own custom instruction set. It assumes no prior LLVM backend experience.

---

## Before You Start

You need a complete written specification for your ISA:

1. **Register file** — How many registers? Which are special-purpose? What width?
2. **Instruction encoding** — Fixed-width or variable? What are the formats?
3. **Instruction set** — What ALU ops exist? What memory access modes?
4. **Calling convention** — How are arguments passed? Which regs are callee-saved?
5. **Address space** — How wide are pointers? How large is addressable memory?
6. **Endianness** — Little or big?

---

## Files to Modify (In Order)

Every file containing `// TODO:` markers (a few also carry
`*** ADAPT FOR YOUR ISA ***` notes) needs changes. Work through them in this
order — it matches the tutorial stages (data ops → control flow → functions):

| Step | File | What You'll Change |
|------|------|--------------------|
| 1 | `MYISARegisterInfo.td` | Your registers and register classes |
| 2 | `MYISAInstrFormats.td` | Your instruction binary encoding formats |
| 3 | `MYISAInstrInfo.td` | Your instructions and ISel patterns |
| 4 | `MYISACallingConv.td` | Argument passing and callee-saved regs |
| 5 | `MYISA.td` | Subtarget features for your CPU variants |
| 6 | `MYISATargetMachine.cpp` | Your data layout string |
| 7 | `MYISAISelLowering.cpp` | Legal/illegal operations for your ISA |
| 8 | `MYISAISelDAGToDAG.cpp` | Constant materialization and custom selection |
| 9 | `MYISAFrameLowering.cpp` | Prologue/epilogue using your stack instructions |
| 10 | `MYISARegisterInfo.cpp` | Reserve your special-purpose registers |
| 11 | `MYISAInstrInfo.cpp` | `copyPhysReg` with your MOV instruction |
| 12 | `TargetInfo/MYISATargetInfo.cpp` | Rename the target |
| 13 | `MCTargetDesc/MYISAMCAsmInfo.h` | Assembly syntax (comment char, directives) |
| 14 | All files | Rename `MYISA` → `YourTarget` everywhere |

---

## Step 1: Define Your Register File

**File:** `MYISARegisterInfo.td`

This is first because everything else depends on register definitions.

**What to do:**
1. Change the encoding width to match your register address bits
2. Define one `def` per physical register
3. Group into `RegisterClass` definitions by role
4. Set allocation order (caller-saved first, callee-saved second)

**Key decisions:**
- Which registers are special-purpose (zero, PC, SP, link)?
- Which can the compiler freely allocate?
- Separate register files (int vs float vs vector)?

**Example** — 16 registers with 4-bit encoding:
```tablegen
class MyReg<bits<4> Enc, string n> : Register<n> {
  let HWEncoding{3-0} = Enc;
  let Namespace = "MyTarget";
}
def R0 : MyReg<0, "r0">;  // Zero register
def R1 : MyReg<1, "r1">;  // ...
```

---

## Step 2: Define Instruction Encoding Formats

**File:** `MYISAInstrFormats.td`

**What to do:**
1. Map your encoding to bit fields in `field bits<N> Inst`
2. Create a base `Instruction` subclass per format
3. Set `Size` to your instruction width in bytes
4. Use a bit as format discriminator if you have multiple formats

**Key decisions:**
- Fixed or variable-length instructions?
- How many formats (R-type, I-type, B-type, etc.)?
- Where does the opcode sit in each format?

**Common patterns:**
- RISC-V: 4 formats (R, I, S, U) all 32-bit
- ARM Thumb: 16-bit and 32-bit mixed
- x86: variable 1–15 bytes (very complex — avoid for custom ISAs)

---

## Step 3: Define Instructions and Patterns

**File:** `MYISAInstrInfo.td`

This is the largest file and the core of instruction selection.

**What to do:**
1. Define operand types (immediates, branch targets, memory operands)
2. Define each instruction with: format class, opcode, operand list, assembly string, and SelectionDAG pattern
3. Use patterns like `[(set GPR:$rd, (add GPR:$rs1, GPR:$rs2))]` for automatic ISel

**Pattern tips:**
- `(set dst, (op src1, src2))` = assign result of `op` to `dst`
- `ImmLeaf<i32, [{return Imm >= 0 && Imm <= 15;}]>` for range-checked immediates
- Empty `[]` for instructions matched by custom C++ code
- `ComplexPattern` for addressing modes needing C++ decomposition

**If your ISA lacks an operation:** don't define it here. Mark it `Expand` in step 7 and LLVM will synthesize it from other instructions or library calls.

---

## Step 4: Define Calling Convention

**File:** `MYISACallingConv.td`

**What to do:**
1. Choose argument registers and order (`CCAssignToReg<[...]>`)
2. Choose return value register(s)
3. Define stack argument rules (size, alignment)
4. List callee-saved registers

**Key decisions:**
- How many register arguments? (Common: 4–8)
- What about structs/floats? (Can start integer-only)
- Which registers does the callee preserve?

---

## Step 5: Set Data Layout

**File:** `MYISATargetMachine.cpp`

Change the data layout string to match your architecture:

```
"e-p:32:32-i32:32-n32-S32"
 │  │       │      │   └─ Stack alignment (bits)
 │  │       │      └───── Native integer width
 │  │       └──────────── i32 alignment
 │  └──────────────────── Pointer width:alignment
 └─────────────────────── Endianness (e=little, E=big)
```

**Common layouts:**
- 32-bit RISC: `"e-p:32:32-i32:32-n32-S32"`
- 16-bit MCU: `"e-p:16:16-i16:16-n16-S16"`
- 64-bit RISC: `"e-p:64:64-i64:64-n64-S64"`

---

## Step 6: Declare Legal Operations

**File:** `MYISAISelLowering.cpp`

In the constructor, mark operations your ISA does NOT have:

```cpp
setOperationAction(ISD::SDIV, MVT::i32, Expand);  // No hardware divide
setOperationAction(ISD::BR_CC, MVT::i32, Custom);  // Custom branch handling
```

**Three options:**
- **Legal** (default): hardware instruction exists (defined in step 3)
- **Custom**: you handle it in `LowerOperation()` with C++
- **Expand**: LLVM replaces it with legal ops or a libcall

**Start conservative:** mark everything you're unsure about as `Expand`. Change to Legal/Custom later as you add instruction support.

---

## Step 7: Custom Instruction Selection

**File:** `MYISAISelDAGToDAG.cpp`

Override `Select()` for cases that can't be TableGen patterns:
- Constant materialization (loading immediates that don't fit one instruction)
- Call sequence pseudos (CALLSEQ_START/END always need manual handling)
- Custom DAG nodes from your ISelLowering

**Tip:** Start minimal. Let TableGen handle as much as possible. Only add C++ for complex cases.

---

## Step 8: Frame Lowering

**File:** `MYISAFrameLowering.cpp`

Implement `emitPrologue()` and `emitEpilogue()`:
1. Save link register (if CALL clobbers it)
2. Save callee-saved registers (PUSH/STORE)
3. Allocate stack frame (subtract from SP)
4. Mirror in epilogue (reverse order)

**Key question:** How does your ISA adjust the stack pointer? A single wide immediate is ideal. If you only have small immediates, you'll need a loop (MYISA subtracts 31 at a time because its immediate field is only 5 bits).

---

## Step 9: Reserve Special Registers

**File:** `MYISARegisterInfo.cpp`

In `getReservedRegs()`, mark every register the compiler must never allocate: zero register, PC, SP, link register, etc.

In `eliminateFrameIndex()`, convert abstract stack slot references into your ISA's addressing mode (typically SP + offset).

---

## Step 10: Register Copies and Spills

**File:** `MYISAInstrInfo.cpp`

Implement:
- `copyPhysReg()` — emit your MOV/ADD-zero instruction to copy between registers
- `storeRegToStackSlot()` — emit a STORE to spill a register to the stack
- `loadRegFromStackSlot()` — emit a LOAD to reload a register from the stack

---

## Step 11: Rename Everything

Once your backend works with MYISA names, do a global search-and-replace:
1. `MYISA` → `YourTarget` (filenames and contents)
2. `myisa` → `yourtarget` (target name strings)
3. `MYISAISD` → `YourTargetISD`
4. Update this file with your target's specifics

---

## Common Mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| Forgot to reserve a special register | Allocator assigns values to PC/SP | Add to `getReservedRegs()` |
| Operand type mismatch in pattern | TableGen: "type mismatch in pattern" | Ensure both sides are same type (e.g., i32) |
| Missing `Defs`/`Uses` on instruction | Allocator doesn't see implicit writes | Add `let Defs = [REG]` to the def |
| Expanded op has no libcall | Link error: undefined `__divsi3` | Provide compiler-rt or custom implementation |
| Stack offset exceeds immediate range | Crash in `eliminateFrameIndex` | Add multi-instruction offset computation |
| Branch offset exceeds encoding width | Assertion in branch relaxation | Add a branch relaxation pass |

---

## Debugging

```bash
# Full ISel trace (which patterns match)
llc -debug -march=yourtarget test.ll

# IR dump after each pass
llc -print-after-all -march=yourtarget test.ll

# Inspect TableGen records
llvm-tblgen -print-records -I /path/to/llvm/include YourTarget.td

# Build with assertions (catches bugs early)
cmake ... -DCMAKE_BUILD_TYPE=Debug
```

---

## Minimal Test Program

Start with the simplest possible program:

```c
int add(int a, int b) {
    return a + b;
}
```

```bash
./bin/clang --target=yourtarget -S -O1 -o test.s test.c
```

Then gradually increase complexity:
1. **Local variables** — tests stack frame
2. **Function calls** — tests calling convention
3. **Conditionals** — tests branches
4. **Loops** — tests back-edges
5. **Arrays** — tests memory addressing
6. **Recursion** — tests everything together

---

## Recommended Reading

1. **This codebase** — read this file, then the `.td` files, then the `.cpp` files
2. [Writing an LLVM Backend](https://llvm.org/docs/WritingAnLLVMBackend.html) — official guide
3. [TableGen Language Introduction](https://llvm.org/docs/TableGen/index.html) — the `.td` DSL
4. [LLVM Data Layout Reference](https://llvm.org/docs/LangRef.html#data-layout) — the layout string format
5. **RISC-V or Lanai backends** in LLVM source — simple real-world backends for comparison

---

## Troubleshooting

| Error | Cause | Fix |
|---|---|---|
| `No such target "MYISA"` | Not in `LLVM_ALL_TARGETS` | Add to `llvm/CMakeLists.txt` |
| `MYISAGenRegisterInfo.inc: No such file` | TableGen didn't run | Check `LLVM_TARGET_DEFINITIONS` points to `MYISA.td` |
| `Undefined symbol: getTheMYISATarget` | TargetInfo not linked | Check `LINK_COMPONENTS` includes `MYISAInfo` |
| `no matching function for call to MYISAGenInstrInfo` | LLVM API changed | Check constructor signature in your LLVM version |
| Register allocator crash | Not enough reserved | Reserve all special regs (r0–r7) in `getReservedRegs()` |
