// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 Analog Devices, Inc.
//===-- MYISAFrameLowering.h - MYISA Frame Lowering ----------*- C++ -*-===//
//
// WHAT THIS FILE IS:
//   Declares the MYISAFrameLowering class — responsible for generating
//   function prologue and epilogue code. The prologue runs at function entry
//   (saves registers, allocates stack frame) and the epilogue runs at function
//   exit (restores registers, deallocates stack frame).
//
// WHY IT MUST EXIST:
//   Every function that uses the stack (which is most functions) needs
//   prologue/epilogue code. Without this class, the compiler cannot:
//     - Save/restore callee-saved registers (corrupting the caller's state)
//     - Allocate space for local variables on the stack
//     - Maintain a valid stack pointer across function calls
//   LLVM calls emitPrologue/emitEpilogue after register allocation to insert
//   the necessary machine instructions.
//
// WHAT EACH METHOD DOES:
//   - Constructor: declares stack properties (grows down, 4-byte aligned)
//   - emitPrologue(): inserts frame setup code at function entry
//   - emitEpilogue(): inserts frame teardown code before each return
//   - hasFP(): determines if a frame pointer (r30) is needed
//   - eliminateCallFramePseudoInstr(): removes ADJCALLSTACKDOWN/UP pseudos
//
// WHAT COULD BE ADDED:
//   - determineCalleeSaves() override: custom logic for which regs to save
//   - getFrameIndexReference() override: custom frame offset calculation
//   - spillCalleeSavedRegisters() override: custom spill sequence (e.g.,
//     store-multiple instruction if hardware supports it)
//   - restoreCalleeSavedRegisters() override: custom restore sequence
//   - adjustForSegmentedStacks(): stack overflow detection/guard pages
//   - emitEHPrologue() for exception handling support
//   - processFunctionBeforeFrameFinalized(): last-minute frame adjustments
//   - orderFrameObjects(): optimize stack layout for cache performance
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIB_TARGET_MYISA_MYISAFRAMELOWERING_H
#define LLVM_LIB_TARGET_MYISA_MYISAFRAMELOWERING_H

#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {

class MYISAFrameLowering : public TargetFrameLowering {
public:
  // Constructor: configures basic stack properties.
  // StackGrowsDown: stack pointer decreases as items are pushed
  // StackAlignment=4: SP must always be 4-byte aligned
  // LocalAreaOffset=0: no gap between FP and first local variable
  MYISAFrameLowering()
      : TargetFrameLowering(TargetFrameLowering::StackGrowsDown,
                            /*StackAlignment=*/Align(4),
                            /*LocalAreaOffset=*/0) {}

  // emitPrologue: Insert frame setup code at the beginning of the function.
  // Sequence: PUSH LR (if non-leaf), PUSH callee-saved regs, set FP, SUB SP
  void emitPrologue(MachineFunction &MF,
                    MachineBasicBlock &MBB) const override;

  // emitEpilogue: Insert frame teardown code before the return instruction.
  // Sequence: ADD SP (or restore from FP), POP callee-saved regs, POP LR
  void emitEpilogue(MachineFunction &MF,
                    MachineBasicBlock &MBB) const override;

  // hasFP: Returns true if this function needs a dedicated frame pointer (r30).
  // Needed when: variable-sized objects (alloca), or frame address is taken.
  bool hasFP(const MachineFunction &MF) const override;

  // eliminateCallFramePseudoInstr: Removes ADJCALLSTACKDOWN/UP pseudo-instrs.
  // Since we allocate the full frame in the prologue (including outgoing arg
  // space), these pseudos are just erased — no actual SP adjustment needed.
  MachineBasicBlock::iterator
  eliminateCallFramePseudoInstr(MachineFunction &MF, MachineBasicBlock &MBB,
                                MachineBasicBlock::iterator MI) const override;
};

} // end namespace llvm

#endif
