// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 Analog Devices, Inc.
//===-- MYISAInstrInfo.h - MYISA Instruction Info ------------*- C++ -*-===//
//
// WHAT THIS FILE IS:
//   Declares the MYISAInstrInfo class — the interface for instruction-level
//   operations that LLVM's code generator needs beyond what TableGen provides.
//   While TableGen generates instruction descriptors and patterns, this class
//   provides dynamic operations: copying between physical registers, spilling
//   registers to the stack, and reloading them.
//
// WHY IT MUST EXIST:
//   The register allocator frequently needs to:
//     1. Move a value from one physical register to another (copyPhysReg)
//     2. Spill a register to a stack slot when no registers are available
//        (storeRegToStackSlot)
//     3. Reload a register from a stack slot (loadRegFromStackSlot)
//   These operations are target-specific (every ISA uses different instructions
//   for reg-to-reg moves and memory access), so the backend must provide them.
//
// WHAT THE #include'd .inc FILES PROVIDE:
//   GET_INSTRINFO_ENUM: defines the MYISA::ADD_rrr, MYISA::PUSH, etc. enum
//   GET_INSTRINFO_HEADER: declares the MYISAGenInstrInfo base class with
//     getInstructionName(), getInstSizeInBytes(), etc.
//
// WHAT EACH METHOD DOES:
//   - MYISAInstrInfo(): constructor — passes ADJCALLSTACKDOWN/UP opcodes to
//     the base class (tells LLVM which pseudos mark call frame boundaries)
//   - copyPhysReg(): emits MOV rd, rs for register-to-register transfers
//   - storeRegToStackSlot(): emits STORE_reg to save a register to memory
//   - loadRegFromStackSlot(): emits LOAD_reg to restore a register from memory
//
// WHAT COULD BE ADDED:
//   - analyzeBranch() / insertBranch() / removeBranch(): enable branch
//     optimization passes (if-conversion, block reordering, tail merging)
//   - insertNoop(): emit a NOP instruction (for alignment or pipeline hazards)
//   - isLoadFromStackSlot() / isStoreToStackSlot(): help the register coalescer
//     identify and eliminate redundant spill/reload pairs
//   - foldMemoryOperand(): fold a spill/reload into an instruction's memory
//     operand (e.g., ADD from stack slot instead of LOAD + ADD)
//   - expandPostRAPseudo(): expand pseudo-instructions after register allocation
//   - getInstSizeInBytes(): return instruction size for branch relaxation
//   - isReallyTriviallyReMaterializable(): mark cheap-to-recompute instructions
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIB_TARGET_MYISA_MYISAINSTRINFO_H
#define LLVM_LIB_TARGET_MYISA_MYISAINSTRINFO_H

#include "llvm/CodeGen/TargetInstrInfo.h"

// Define the MYISA instruction opcode enum (MYISA::ADD_rrr, MYISA::PUSH, …)
// This enum is used throughout the backend to reference specific instructions.
#define GET_INSTRINFO_ENUM
#include "MYISAGenInstrInfo.inc"

// Declare the MYISAGenInstrInfo base class (auto-generated from TableGen).
// Provides: instruction name lookup, operand info, scheduling info, etc.
#define GET_INSTRINFO_HEADER
#include "MYISAGenInstrInfo.inc"

namespace llvm {

class MYISAInstrInfo : public MYISAGenInstrInfo {
public:
  MYISAInstrInfo();

  // copyPhysReg — Emit a MOV instruction to copy between physical registers.
  // Called by the register allocator when it needs to resolve register
  // conflicts (e.g., two values need to be in the same physical register).
  void copyPhysReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
                   const DebugLoc &DL, MCRegister DstReg, MCRegister SrcReg,
                   bool KillSrc) const override;

  // storeRegToStackSlot — Emit a STORE to save a register to a stack slot.
  // Called when the register allocator runs out of physical registers and
  // needs to "spill" a value to memory temporarily.
  void storeRegToStackSlot(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator MI, Register SrcReg,
                           bool isKill, int FrameIndex,
                           const TargetRegisterClass *RC,
                           const TargetRegisterInfo *TRI,
                           Register VReg) const override;

  // loadRegFromStackSlot — Emit a LOAD to restore a register from a stack slot.
  // Called when a previously-spilled value is needed again.
  void loadRegFromStackSlot(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator MI, Register DestReg,
                            int FrameIndex, const TargetRegisterClass *RC,
                            const TargetRegisterInfo *TRI,
                            Register VReg) const override;
};

} // end namespace llvm

#endif
