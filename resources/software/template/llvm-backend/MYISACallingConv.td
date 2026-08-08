// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 Analog Devices, Inc.
//===-- MYISACallingConv.td - Calling Convention ----------*- tablegen -*-===//
//
// WHAT THIS FILE IS:
//   Defines the calling convention for MYISA — the rules that govern how
//   function arguments are passed to callees and how return values come back.
//   Also declares which registers are callee-saved (must be preserved by
//   the called function).
//
// WHY IT MUST EXIST:
//   LLVM's call lowering (LowerCall, LowerFormalArguments, LowerReturn in
//   MYISAISelLowering.cpp) uses the generated CC_MYISA() and RetCC_MYISA()
//   functions to decide where each argument/return value goes. Without this
//   file, the backend cannot compile functions that take arguments or return
//   values — which is essentially all real code.
//
// WHAT TABLEGEN GENERATES FROM THIS:
//   MYISAGenCallingConv.inc contains:
//     - bool CC_MYISA(unsigned ValNo, MVT ValVT, ...) — argument assignment
//     - bool RetCC_MYISA(unsigned ValNo, MVT ValVT, ...) — return value assignment
//     - const MCPhysReg CSR_MYISA_SaveList[] — callee-saved register list
//     - const uint32_t CSR_MYISA_RegMask[] — bitmask of call-preserved registers
//
// WHAT EACH SECTION DOES:
//   - RetCC_MYISA: Return value convention. i32 returns go in r8.
//   - CC_MYISA: Argument passing. First 8 i32 args go in r8–r15,
//     subsequent args spill to the stack at 4-byte-aligned offsets.
//   - CSR_MYISA: Callee-saved register list. These registers must be
//     saved in the function prologue and restored in the epilogue if used.
//
// HOW THE CALLING CONVENTION WORKS AT RUNTIME:
//   Caller side (LowerCall):
//     1. Assigns args to r8–r15 or stack slots via CC_MYISA
//     2. Emits CopyToReg to move values into physical arg registers
//     3. Emits CALL instruction
//     4. Reads return value from r8 via RetCC_MYISA
//   Callee side (LowerFormalArguments):
//     1. Reads arg locations via CC_MYISA
//     2. Emits CopyFromReg to move physical regs into virtual regs
//     3. For stack args, emits loads from fixed stack objects
//
// WHAT COULD BE ADDED:
//   - Support for multiple return values (e.g., return i32 in r8, r9)
//   - Support for 64-bit arguments (passed in register pairs r8:r9, etc.)
//   - Varargs support (marking where the variable argument area starts)
//   - Alternate calling conventions (e.g., "fast" with all regs caller-saved,
//     or "interrupt" that saves all registers)
//   - Float/double argument passing rules (if FPU hardware is added)
//   - Byval/sret struct passing conventions
//
// *** ADAPT FOR YOUR ISA: change the register lists to match your ABI.    ***
// *** If you have fewer argument registers, shorten the CCAssignToReg     ***
// *** list.  Callee-saved = registers the CALLED function must preserve.  ***
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Return value convention
//
// CCIfType<[i32], ...> means "if the return value is i32, apply this rule."
// CCAssignToReg<[R8]> means "assign it to physical register r8."
// If there were multiple return values, you'd add more registers here.
//===----------------------------------------------------------------------===//
def RetCC_MYISA : CallingConv<[
  // TODO: assign return values to your ABI's return register(s).
  //       Example: return a 32-bit value in r8.
  //   CCIfType<[i32], CCAssignToReg<[R8]>>
]>;

//===----------------------------------------------------------------------===//
// Argument passing convention
//
// Rules are evaluated in order for each argument. The first matching rule wins.
//   1. If the arg is i32 and a register from [R8..R15] is still free, assign it.
//   2. Otherwise (all 8 arg regs used), spill to the stack with 4-byte size
//      and 4-byte alignment.
//
// The register list order matters: R8 is assigned first, then R9, etc.
// This matches the assembler's convention and ABI documentation.
//===----------------------------------------------------------------------===//
def CC_MYISA : CallingConv<[
  // TODO: describe how arguments are passed in your ABI. Rules are tried in
  //       order; the first that matches an argument wins. A typical scheme is
  //       "first N integer args in registers, the rest spilled to the stack":
  //   CCIfType<[i32], CCAssignToReg<[R8, R9, R10, R11, R12, R13, R14, R15]>>,
  //   CCIfType<[i32], CCAssignToStack<4, 4>>
]>;

//===----------------------------------------------------------------------===//
// Callee-saved registers
//
// CalleeSavedRegs defines which physical registers a called function must
// preserve. If a function uses any of these, the prologue must PUSH them
// and the epilogue must POP them (handled by MYISAFrameLowering.cpp).
//
// TableGen generates two things from this:
//   - CSR_MYISA_SaveList: array of MCPhysReg for the prologue/epilogue
//   - CSR_MYISA_RegMask: bitmask used by the register allocator to know
//     which registers survive across a CALL instruction (so it doesn't need
//     to spill values that live in callee-saved registers across calls).
//===----------------------------------------------------------------------===//
// TODO: list the registers a called function must preserve (callee-saved).
//       These are PUSHed in the prologue and POPped in the epilogue if used.
//       Must not overlap the caller-saved / argument registers above.
//       Example:
//   def CSR_MYISA : CalleeSavedRegs<(add R16, R17, R18, R19, R20, R21, R22, R23,
//                                        R24, R25, R26, R27, R28, R29, R30, R31)>;
def CSR_MYISA : CalleeSavedRegs<(add)>;
