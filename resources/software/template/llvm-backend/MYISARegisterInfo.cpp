// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 Analog Devices, Inc.
//===-- MYISARegisterInfo.cpp - MYISA Register Info ----------------------===//
//
// WHAT THIS FILE IS:
//   Implements the MYISARegisterInfo class — provides runtime register file
//   information to the register allocator and frame lowering passes.
//
// WHY IT MUST EXIST:
//   Contains the implementation of methods that the register allocator and
//   post-RA passes call to:
//     - Know which registers are off-limits (reserved)
//     - Know which registers survive function calls (callee-saved)
//     - Convert abstract "frame index" references into concrete SP+offset
//       addressing in the final machine code
//
// WHAT EACH FUNCTION DOES:
//   Constructor:
//     Calls MYISAGenRegisterInfo with the return address register (R3).
//     This tells LLVM which register holds the return address for unwinding.
//
//   getCalleeSavedRegs():
//     Returns CSR_MYISA_SaveList — the ordered list of registers that must
//     be saved in the prologue if used. Generated from MYISACallingConv.td.
//
//   getCallPreservedMask():
//     Returns CSR_MYISA_RegMask — a bitmask where 1 = preserved across calls.
//     The RA uses this at call sites to know which live values are "safe"
//     (in callee-saved regs) vs. which need spilling (in caller-saved regs).
//
//   getReservedRegs():
//     Returns a BitVector marking all special-purpose registers as reserved.
//     The RA will never assign virtual registers to reserved physical regs.
//     r0–r7 are all reserved (zero, PC, SP, LR, TP, cond, ISR, reserved).
//
//   eliminateFrameIndex():
//     After register allocation, instructions still reference abstract "frame
//     indices" (e.g., STORE_reg $src, %stack.0). This function replaces them
//     with concrete SP-relative addressing (e.g., STORE_reg $src, [r2 + 12]).
//     The offset calculation: ObjectOffset + StackSize + SPAdj.
//
//   getFrameRegister():
//     Returns r2 (SP) as the frame base register. Used by debug info and
//     other passes that need to reference the frame.
//
// WHAT COULD BE ADDED:
//   - requiresRegisterScavenging(): return true if frame offsets might not
//     fit in the 16-bit addr field (would need a scavenger register)
//   - eliminateFrameIndex with large offset handling: if SP+offset > 16 bits,
//     emit a multi-instruction sequence to compute the address
//   - getRegPressureLimit(): tell the scheduler how many GPRs are available
//   - Per-calling-convention reserved registers (e.g., interrupt handlers
//     might reserve additional registers)
//
//===----------------------------------------------------------------------===//

#include "MYISARegisterInfo.h"
#include "MYISA.h"
#include "MYISASubtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/RegisterScavenging.h"

// Include the auto-generated register info implementation tables.
// Contains: register encoding arrays, register class membership, sub-reg info.
#define GET_REGINFO_TARGET_DESC
#include "MYISAGenRegisterInfo.inc"

using namespace llvm;

// Constructor: pass the return address register to the base class.
// MYISAGenRegisterInfo needs this for DWARF unwinding (identifies which
// register holds the return address for backtracing).
MYISARegisterInfo::MYISARegisterInfo()
    : MYISAGenRegisterInfo(MYISA::R3 /* return address register */) {}

// getCalleeSavedRegs — Returns the callee-saved register list.
// CSR_MYISA_SaveList is auto-generated from CalleeSavedRegs in CallingConv.td.
// Returns: {R16, R17, R18, ..., R31, 0} (null-terminated array).
const MCPhysReg *
MYISARegisterInfo::getCalleeSavedRegs(const MachineFunction *MF) const {
  return CSR_MYISA_SaveList;
}

// getCallPreservedMask — Returns the call-preserved register bitmask.
// CSR_MYISA_RegMask is generated alongside SaveList. Each bit corresponds
// to a physical register; set bits indicate the register is preserved across
// calls (i.e., the callee guarantees it won't modify the value).
const uint32_t *
MYISARegisterInfo::getCallPreservedMask(const MachineFunction &MF,
                                         CallingConv::ID) const {
  return CSR_MYISA_RegMask;
}

// getReservedRegs — Mark special-purpose registers as unavailable.
// The register allocator will NEVER assign a virtual register to any
// physical register in this set. All 8 special registers are reserved:
//   r0 = zero (writes ignored, reads always 0)
//   r1 = PC (program counter — not directly writable)
//   r2 = SP (stack pointer — managed by frame lowering)
//   r3 = LR (link register — managed by call/return)
//   r4 = TP (thread pointer — reserved for OS)
//   r5 = cond (condition register — managed by CMP/branch)
//   r6 = ISR (interrupt status — reserved for hardware)
//   r7 = reserved (future use)
BitVector MYISARegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());

  Reserved.set(MYISA::R0);
  Reserved.set(MYISA::R1);
  Reserved.set(MYISA::R2);
  Reserved.set(MYISA::R3);
  Reserved.set(MYISA::R4);
  Reserved.set(MYISA::R5);
  Reserved.set(MYISA::R6);
  Reserved.set(MYISA::R7);

  // NOTE: R30 is NOT reserved here. Even though r30 is used as a frame
  // pointer when needed (hasFP() returns true), it's still allocatable
  // in functions that don't need a frame pointer. This gives the RA an
  // extra register to work with in most functions.

  return Reserved;
}

// eliminateFrameIndex — Convert frame index operands to SP+offset.
//
// During code generation, memory instructions reference stack objects by
// abstract "frame index" (e.g., %stack.0, %stack.1). After the frame size
// is finalized, this function replaces each frame index with:
//   Base register = R2 (SP)
//   Offset = object's offset from SP
//
// The offset calculation:
//   SP-relative offset = ObjectOffset + StackSize + SPAdj
// Where:
//   ObjectOffset = MFI.getObjectOffset(FI) — frame-pointer-relative (negative)
//   StackSize = MFI.getStackSize() — total frame size (positive)
//   SPAdj = dynamic adjustment (usually 0)
//
// After this, the instruction looks like: LOAD rd, [r2 + 12]
bool MYISARegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator MI,
                                             int SPAdj,
                                             unsigned FIOperandNum,
                                             RegScavenger *RS) const {
  MachineInstr &Instr = *MI;
  MachineFunction &MF = *Instr.getParent()->getParent();
  const MachineFrameInfo &MFI = MF.getFrameInfo();

  // Get the frame index from the operand
  int FrameIndex = Instr.getOperand(FIOperandNum).getIndex();

  // Calculate the SP-relative offset
  int Offset = MFI.getObjectOffset(FrameIndex) + (int)MFI.getStackSize() + SPAdj;

  // Replace the frame index operand with the base register (SP = R2)
  Register BaseReg = MYISA::R2;
  Instr.getOperand(FIOperandNum).ChangeToRegister(BaseReg, false);

  // Set the offset in the next operand (the immediate field of memsrc)
  if (FIOperandNum + 1 < Instr.getNumOperands() &&
      Instr.getOperand(FIOperandNum + 1).isImm()) {
    Offset += Instr.getOperand(FIOperandNum + 1).getImm();
    Instr.getOperand(FIOperandNum + 1).setImm(Offset);
  }

  return false;  // false = no additional scavenging needed
}

// getFrameRegister — Returns the register used as the frame base.
// All frame-relative accesses use SP (R2) in MYISA (no separate FP by default).
Register MYISARegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  return MYISA::R2;
}
