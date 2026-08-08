// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 Analog Devices, Inc.
//===-- MYISAInstrInfo.td - MYISA Instruction defs -------*- tablegen -*-===//
//
// WHAT THIS FILE IS:
//   Defines every machine instruction in the MYISA instruction set, along
//   with the SelectionDAG patterns that allow LLVM's instruction selector
//   to automatically map high-level operations (add, sub, load, store,
//   branch, call) to concrete machine instructions.
//
// WHY IT MUST EXIST:
//   This is the core of the backend's instruction selection capability.
//   Without instruction definitions and patterns, the backend cannot
//   transform LLVM IR into machine code. TableGen uses these definitions
//   to generate:
//     - MYISAGenInstrInfo.inc (instruction descriptors, opcode enum)
//     - MYISAGenDAGISel.inc (pattern-matching code for SelectCode())
//     - MYISAGenAsmWriter.inc (assembly printing from AsmString fields)
//
// WHAT EACH SECTION DOES:
//   1. Operand types: define how immediates and addresses are represented
//      in the DAG and how they map to instruction bit fields.
//   2. SDNode definitions: declare custom DAG nodes (retflag, callseq)
//      that don't exist in LLVM's built-in node vocabulary.
//   3. Type 1 ALU instructions: arithmetic, logic, shifts, comparison.
//   4. Type 2 memory instructions: load, store (word and byte).
//   5. Type 2 branch/jump instructions: conditional and unconditional.
//   6. Type 2 call/return instructions: function call mechanism.
//   7. Type 2 stack operations: push/pop for callee-save and frame management.
//   8. Pseudo-instructions: bookkeeping nodes that don't produce machine code.
//
// HOW PATTERNS WORK:
//   Each instruction definition can include a Pattern list like:
//     [(set GPR:$rd, (add GPR:$rs1, GPR:$rs2))]
//   This tells the ISel: "if you see an 'add' node with two GPR inputs
//   producing a GPR output, emit an ADD_rrr instruction." TableGen compiles
//   these patterns into the SelectCode() function in MYISAGenDAGISel.inc.
//
// WHAT COULD BE ADDED:
//   - Multiply-accumulate (MAC) instruction
//   - Hardware divide instructions (SDIV, UDIV, SREM, UREM)
//   - Atomic load/store for multi-core support
//   - SIMD/vector instructions
//   - Floating-point instructions (FADD, FSUB, FMUL, FDIV)
//   - Load/store multiple (for faster context save/restore)
//   - Compare-and-branch fused instructions
//   - Bit manipulation instructions (CLZ, CTZ, POPCNT, BSWAP)
//   - Conditional move (CMOV) to avoid branches
//   - PC-relative load (for position-independent code)
//   - 16-bit halfword load/store (LOADH, STOREH)
//   - Wider immediate instructions (if encoding space allows)
//
//===----------------------------------------------------------------------===//

include "MYISAInstrFormats.td"

//===----------------------------------------------------------------------===//
// Operand types
//
// Operand definitions describe how instruction operands appear in the DAG
// and how they map to assembly text and binary encoding. They serve as the
// bridge between the abstract SelectionDAG and concrete instruction fields.
//===----------------------------------------------------------------------===//

// imm5: 5-bit unsigned immediate (range 0–31).
// Fits in the Type 1 arg field (5 value bits). Used by ALU register-immediate
// variants (ADD_rri, SUB_rri, etc.). The ImmLeaf predicate tells the ISel
// when a DAG constant qualifies for this operand type.
def imm5 : Operand<i32>, ImmLeaf<i32, [{return Imm >= 0 && Imm <= 31;}]>;

// imm16: 16-bit address or offset.
// Fits in the Type 2 addr field (16 bits). Used for memory addresses and
// branch offsets. No predicate — validation happens elsewhere.
def imm16 : Operand<i32>;

// imm16u: 16-bit unsigned immediate (range 0–65535).
// Used by the LI (Load Immediate) instruction for materializing constants
// that don't fit in 5 bits. ImmLeaf predicate restricts ISel matching.
def imm16u : Operand<i32>, ImmLeaf<i32, [{return Imm >= 0 && Imm <= 65535;}]>;

// brtarget16: 16-bit signed PC-relative branch offset.
// Used by branch instructions (JMP, JZ, JNZ, JLT, JGT). The EncoderMethod
// specifies a C++ function to call when encoding the offset for binary output.
// OPERAND_PCREL tells LLVM this is PC-relative (needed for fixups/relocations).
def brtarget16 : Operand<OtherVT> {
  let EncoderMethod = "encodeBranchTarget";
  let OperandType = "OPERAND_PCREL";
}

// calltarget: 16-bit call target address.
// Similar to brtarget16 but used specifically by CALL instructions.
// May be absolute or PC-relative depending on the ri flag.
def calltarget : Operand<i32> {
  let EncoderMethod = "encodeBranchTarget";
}

// memsrc: composite memory operand (base register + 16-bit offset).
// MIOperandInfo declares that this operand expands to TWO sub-operands
// at the MachineInstr level: a GPR (base register) and an imm16 (offset).
// PrintMethod specifies how to render it in assembly (e.g., "[r8 + 4]").
def memsrc : Operand<i32> {
  let MIOperandInfo = (ops GPR, imm16);
  let PrintMethod = "printMemOperand";
}

// addr: ComplexPattern for register-indirect memory addressing.
// ComplexPatterns delegate operand matching to a C++ function (SelectAddr
// in MYISAISelDAGToDAG.cpp). This function decomposes a generic address
// DAG node into a (Base, Offset) pair. The [frameindex, add, imm] list
// tells the ISel which DAG node types to try matching against this pattern.
def addr : ComplexPattern<iPTR, 2, "SelectAddr", [frameindex, add, imm]>;

//===----------------------------------------------------------------------===//
// Custom SDNode definitions
//
// These define DAG node types that don't exist in LLVM's standard vocabulary.
// Custom nodes are emitted by the ISelLowering code (MYISAISelLowering.cpp)
// and consumed by the ISel patterns or Select() function.
//===----------------------------------------------------------------------===//

// retflag: Represents a function return. Emitted by LowerReturn() as the
// terminal node of a function. Matched by the RET instruction pattern.
// SDNPHasChain: this node has a chain (ordering) input/output
// SDNPOptInGlue: may optionally have a glue (data dependency) input
// SDNPVariadic: can have a variable number of operands (for reg copies)
def retflag : SDNode<"MYISAISD::RET_FLAG", SDTNone,
                     [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;

// Call-sequence markers: Required by LLVM's call frame infrastructure.
// CALLSEQ_START marks the beginning of outgoing argument setup (before the
// call), and CALLSEQ_END marks the cleanup (after the call returns).
// These allow the frame lowering to adjust the stack around calls.
def SDT_MYISACallSeqStart : SDCallSeqStart<[SDTCisVT<0, i32>,
                                             SDTCisVT<1, i32>]>;
def SDT_MYISACallSeqEnd   : SDCallSeqEnd<[SDTCisVT<0, i32>,
                                           SDTCisVT<1, i32>]>;
def callseq_start : SDNode<"ISD::CALLSEQ_START", SDT_MYISACallSeqStart,
                           [SDNPHasChain, SDNPOutGlue]>;
def callseq_end   : SDNode<"ISD::CALLSEQ_END",   SDT_MYISACallSeqEnd,
                           [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;

//===----------------------------------------------------------------------===//
// Type 1 — ALU Instructions
//
// These instructions perform arithmetic, logic, and data manipulation
// operations on register values. Each one maps directly to a C operator
// or built-in operation via the SelectionDAG pattern.
//
// Naming convention:
//   <OP>_rrr = register, register → register
//   <OP>_rri = register, immediate → register
//   <OP>_rr  = register → register (unary)
//
// *** ADAPT FOR YOUR ISA: replace these with your ALU opcodes.            ***
// *** Each def maps one instruction mnemonic to an LLVM SelectionDAG      ***
// *** node (add, sub, and, or, …).  Remove ops your ISA lacks; add new    ***
// *** ones your ISA has.  Keep the pattern [(set ...)] lines — they are   ***
// *** what allows C operators to be compiled to your instructions.         ***
//===----------------------------------------------------------------------===//

// --- Three-operand (reg, reg/imm, reg result) ---

// ADD rd, rs1, rs2 — Integer addition (register + register)
// C equivalent: rd = rs1 + rs2
// Pattern: matches the 'add' SelectionDAG node with two register operands.
def ADD_rrr : Type1RRR<0x0000, "ADD",
  (outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2),
  "ADD\t$rd, $rs1, $rs2",
  [(set GPR:$rd, (add GPR:$rs1, GPR:$rs2))]>;

// ADD rd, rs1, #imm5 — Integer addition (register + 5-bit immediate)
// C equivalent: rd = rs1 + imm (where 0 ≤ imm ≤ 31)
// The arg2 field encodes {flag=1, value=imm} to signal "literal" mode.
def ADD_rri : Type1RRR<0x0000, "ADD",
  (outs GPR:$rd), (ins GPR:$rs1, imm5:$imm),
  "ADD\t$rd, $rs1, #$imm",
  [(set GPR:$rd, (add GPR:$rs1, imm5:$imm))]> {
  bits<5> imm;
  let arg2 = {1, imm};   // flag=1 (literal), value=imm
}

// ADD_rrr and ADD_rri above are a complete WORKED EXAMPLE showing the two
// most important parts of an instruction definition:
//   1. The operand lists (outs)/(ins) and the assembly template string.
//   2. The `[(set GPR:$rd, (<dagnode> ...))]` pattern that tells the
//      instruction selector how to compile a C operator into this instruction.
//
// TODO: define the rest of your ALU instructions using the same pattern.
//       Map each LLVM SelectionDAG node to one of your opcodes, for example:
//         (sub  ...) -> your SUB     (and  ...) -> your AND
//         (or   ...) -> your OR      (xor  ...) -> your XOR
//         (shl  ...) -> your SHL     (srl  ...) -> your SHR (logical)
//         (sra  ...) -> your SAR     (mul  ...) -> your MUL
//         (not  ...) -> your NOT     (sub 0, x) -> your NEG    (abs x) -> ABS
//       Give patternless defs (empty []) for instructions the compiler emits
//       itself rather than pattern-matches. The provided .cpp files expect a
//       few such instructions to exist BY NAME (rename them or update the .cpp):
//         MOV_rr   - register-to-register copy      (MYISAInstrInfo.cpp)
//         LUI      - load-upper-immediate           (constant materialisation)
//         CMP_rr / CMP_ri - compare, writing the condition reg (Defs = [R5])
//       See the Stage 1 tutorial for a worked example.

//===----------------------------------------------------------------------===//
// Type 2 — Memory, Branch, Call and Stack Instructions
//
// TODO: define your memory-access, control-flow, call/return and stack
//       instructions here. Follow the same style as the ADD example above:
//       give each a format base class, an assembly string, and (where the
//       compiler should pattern-match it) a `[(set ...)]` / `[(...)]` pattern.
//
//       Nodes/patterns the compiler expects to be matchable:
//         (load addr:$src)              -> a register-indirect word LOAD
//         (store GPR:$rs, addr:$dst)    -> a register-indirect word STORE
//         (zextloadi8 addr:$src)        -> a byte load (zero-extended)
//         (truncstorei8 GPR:$rs, addr)  -> a byte store
//         (set GPR:$rd, imm16u:$imm)    -> a load-immediate (LI)
//         (br bb:$target)               -> an unconditional branch (JMP)
//         (retflag)                     -> the return instruction (RET)
//
//       Instruction defs the provided .cpp files reference BY NAME (either
//       keep these names or update the C++):
//         LOAD_reg / STORE_reg  - spill & reload          (MYISAInstrInfo.cpp)
//         LI                    - constant materialisation (MYISAISelDAGToDAG.cpp)
//         PUSH / POP            - prologue / epilogue       (MYISAFrameLowering.cpp)
//         CALL                  - call selection            (MYISAISelDAGToDAG.cpp)
//         JZ / JNZ / JLT / JGT  - conditional branches      (MYISAISelDAGToDAG.cpp)
//
//       Remember to set the machine-level flags on control-flow instructions
//       (isBranch/isTerminator on branches, isCall on calls, isReturn/isBarrier
//       on returns, Defs/Uses of the link and stack-pointer registers, etc.).
//       See the Stage 2–3 tutorials for a worked example.
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Pseudo-instructions (expanded during lowering, not encoded)
//
// These instructions exist only in the MachineInstr representation during
// code generation. They are never emitted to assembly or binary — the
// AsmPrinter skips them (isPseudo=1).
//
// ADJCALLSTACKDOWN / ADJCALLSTACKUP: Call frame bookkeeping.
// Emitted around every function call to mark stack space allocated for
// outgoing arguments. The frame lowering pass (eliminateCallFramePseudoInstr)
// either converts them to SP adjustments or erases them.
//
// Both take two immediates: amt1 (stack adjustment size) and amt2 (alignment
// or teardown amount). These are required by LLVM 17's CALLSEQ infrastructure.
//
// Patterns are intentionally empty: ISD::CALLSEQ_START and ISD::CALLSEQ_END
// are matched in MYISAISelDAGToDAG::Select() manually to avoid a result-type
// mismatch (Defs=[R2] causes TableGen to emit MorphNodeTo1 with an extra i32
// output, but callseq nodes produce {chain, glue} which is incompatible).
//===----------------------------------------------------------------------===//

let Defs = [R2], Uses = [R2] in {
  def ADJCALLSTACKDOWN : Pseudo<(outs), (ins i32imm:$amt1, i32imm:$amt2),
                                "# ADJCALLSTACKDOWN $amt1, $amt2", []>;
  def ADJCALLSTACKUP   : Pseudo<(outs), (ins i32imm:$amt1, i32imm:$amt2),
                                "# ADJCALLSTACKUP $amt1, $amt2", []>;
}
