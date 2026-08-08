// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 Analog Devices, Inc.
//===-- MYISARegisterInfo.h - MYISA Register Info ------------*- C++ -*-===//
//
// WHAT THIS FILE IS:
//   Declares the MYISARegisterInfo class — the runtime interface to the
//   register file. While MYISARegisterInfo.td describes registers declaratively
//   (for TableGen), this class provides the dynamic behavior that the register
//   allocator and frame lowering need at compile time.
//
// WHY IT MUST EXIST:
//   The register allocator needs to know:
//     1. Which registers are reserved (cannot be allocated) — getReservedRegs()
//     2. Which registers are callee-saved — getCalleeSavedRegs()
//     3. Which registers survive across calls — getCallPreservedMask()
//     4. How to convert frame indices to SP-relative addresses — eliminateFrameIndex()
//     5. What register serves as the frame base — getFrameRegister()
//   Without this class, register allocation would corrupt special registers
//   and frame-relative memory accesses would be unresolved.
//
// WHAT THE #include'd .inc FILES PROVIDE:
//   GET_REGINFO_ENUM: defines MYISA::R0, MYISA::R1, ..., MYISA::R31 enum
//   GET_REGINFO_HEADER: declares MYISAGenRegisterInfo base class with
//     getNumRegs(), getRegClass(), etc.
//
// WHAT EACH METHOD DOES:
//   - getCalleeSavedRegs(): returns the list from CSR_MYISA in CallingConv.td
//   - getCallPreservedMask(): returns a bitmask of regs that survive calls
//   - getReservedRegs(): marks r0–r7 as unavailable for allocation
//   - eliminateFrameIndex(): converts abstract frame indices to SP + offset
//   - getFrameRegister(): returns SP (r2) as the frame base register
//
// WHAT COULD BE ADDED:
//   - getRegPressureLimit(): tune register pressure heuristics per class
//   - getRegAllocationHints(): guide allocation toward specific registers
//   - shouldRewriteCopySrc(): optimize redundant register copies
//   - getPointerRegClass(): specify which class holds pointer values
//   - requiresRegisterScavenging(): enable emergency spill for out-of-regs
//   - requiresFrameIndexScavenging(): handle cases where frame offset is too large
//   - eliminateFrameIndex with large offsets: emit multi-instruction sequences
//     when SP+offset exceeds the 16-bit encoding limit
//   - trackLivenessAfterRegAlloc(): enable post-RA liveness tracking
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIB_TARGET_MYISA_MYISAREGISTERINFO_H
#define LLVM_LIB_TARGET_MYISA_MYISAREGISTERINFO_H

#include "llvm/CodeGen/TargetRegisterInfo.h"

// Define the MYISA register enum (MYISA::R0, MYISA::R2, MYISA::R30, …)
#define GET_REGINFO_ENUM
#include "MYISAGenRegisterInfo.inc"

// Declare the MYISAGenRegisterInfo base class (generated from TableGen)
#define GET_REGINFO_HEADER
#include "MYISAGenRegisterInfo.inc"

namespace llvm {

class MYISARegisterInfo : public MYISAGenRegisterInfo {
public:
  MYISARegisterInfo();

  // Returns the list of callee-saved registers for prologue/epilogue generation.
  const MCPhysReg *getCalleeSavedRegs(const MachineFunction *MF) const override;

  // Returns a bitmask of registers preserved across function calls.
  // The register allocator uses this to avoid spilling values that live
  // in callee-saved registers across call sites.
  const uint32_t *getCallPreservedMask(const MachineFunction &MF,
                                       CallingConv::ID) const override;

  // Returns a bitvector marking which registers are reserved (not allocatable).
  // Reserved registers: r0 (zero), r1 (PC), r2 (SP), r3 (LR), r4 (TP),
  // r5 (cond), r6 (ISR), r7 (reserved).
  BitVector getReservedRegs(const MachineFunction &MF) const override;

  // Converts frame index references in instructions to concrete SP+offset.
  // Called after register allocation, once the stack frame size is known.
  bool eliminateFrameIndex(MachineBasicBlock::iterator MI, int SPAdj,
                           unsigned FIOperandNum,
                           RegScavenger *RS = nullptr) const override;

  // Returns the register used as the frame base (SP = r2 for us).
  Register getFrameRegister(const MachineFunction &MF) const override;
};

} // end namespace llvm

#endif
