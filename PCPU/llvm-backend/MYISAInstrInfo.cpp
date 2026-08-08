// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 Analog Devices, Inc.
//===-- MYISAInstrInfo.cpp - MYISA Instruction Info ----------------------===//
//
// WHAT THIS FILE IS:
//   Implements the MYISAInstrInfo class — provides the register allocator
//   with the ability to move values between registers and spill/reload them
//   to/from stack slots.
//
// WHY IT MUST EXIST:
//   The register allocator is target-independent — it doesn't know which
//   instruction to use for a register copy or a stack spill. This file
//   provides those target-specific implementations. Without it, the register
//   allocator would crash when it needs to resolve register conflicts.
//
// WHAT EACH FUNCTION DOES:
//   Constructor:
//     Passes the opcodes for ADJCALLSTACKDOWN and ADJCALLSTACKUP to the base
//     class. LLVM uses these to identify call frame pseudo-instructions.
//
//   copyPhysReg():
//     Emits "MOV DstReg, SrcReg" (MYISA opcode 0x000F). Used when:
//       - The RA needs to move a value between physical registers
//       - A phi node resolution requires register copies
//       - Calling convention requires values in specific registers
//
//   storeRegToStackSlot():
//     Emits "STORE_reg SrcReg, [FrameIndex + 0]". Used when:
//       - More values are live simultaneously than physical registers exist
//       - The RA "spills" a register value to a stack slot temporarily
//
//   loadRegFromStackSlot():
//     Emits "LOAD_reg DestReg, [FrameIndex + 0]". Used when:
//       - A previously-spilled value is needed again
//       - The RA "reloads" the value from its stack slot into a register
//
// WHAT COULD BE ADDED:
//   - analyzeBranch(): identify branch targets and conditions in a basic block
//     (enables branch optimization, block placement, if-conversion)
//   - insertBranch() / removeBranch(): modify control flow
//   - reverseBranchCondition(): flip a conditional branch (JZ ↔ JNZ)
//   - insertNoop(): insert NOP for alignment or pipeline hazard avoidance
//   - isLoadFromStackSlot() / isStoreToStackSlot(): help the coalescer
//     identify and eliminate redundant spill/reload pairs
//   - getMemOperandWithOffset(): enable load/store combining
//   - expandPostRAPseudo(): expand pseudo-instructions after RA
//   - foldMemoryOperand(): fold stack loads directly into ALU instructions
//
//===----------------------------------------------------------------------===//

#include "MYISAInstrInfo.h"
#include "MYISA.h"
#include "MYISASubtarget.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineFrameInfo.h"

// Include the auto-generated constructor and destructor for MYISAGenInstrInfo.
// This provides the instruction descriptor tables, operand info, etc.
#define GET_INSTRINFO_CTOR_DTOR
#include "MYISAGenInstrInfo.inc"

using namespace llvm;

// Constructor: tells the base class which opcodes are the call frame pseudos.
// These are used by LLVM to identify ADJCALLSTACKDOWN/UP during frame lowering.
MYISAInstrInfo::MYISAInstrInfo()
    : MYISAGenInstrInfo(MYISA::ADJCALLSTACKDOWN, MYISA::ADJCALLSTACKUP) {}

// copyPhysReg — Emit a register-to-register move instruction.
// MYISA uses "MOV rd, rs" (Type 1, opcode 0x000F) for all register copies.
// Parameters:
//   DstReg: destination physical register
//   SrcReg: source physical register
//   KillSrc: if true, the source register is dead after this copy
void MYISAInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator MI,
                                  const DebugLoc &DL, MCRegister DstReg,
                                  MCRegister SrcReg, bool KillSrc) const {
  BuildMI(MBB, MI, DL, get(MYISA::MOV_rr), DstReg)
      .addReg(SrcReg, getKillRegState(KillSrc));
}

// storeRegToStackSlot — Emit a store instruction to spill a register.
// Uses STORE_reg which encodes as: STORE reg, [FrameIndex + 0]
// The FrameIndex will be replaced with an SP-relative offset during
// frame index elimination (MYISARegisterInfo::eliminateFrameIndex).
void MYISAInstrInfo::storeRegToStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register SrcReg,
    bool isKill, int FrameIndex, const TargetRegisterClass *RC,
    const TargetRegisterInfo *TRI, Register VReg) const {
  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();

  // Emit: STORE_reg SrcReg, [FrameIndex + 0]
  // The two operands after SrcReg are the memsrc composite: (base, offset)
  // FrameIndex becomes the base, 0 is the offset.
  BuildMI(MBB, MI, DL, get(MYISA::STORE_reg))
      .addReg(SrcReg, getKillRegState(isKill))
      .addFrameIndex(FrameIndex)
      .addImm(0);
}

// loadRegFromStackSlot — Emit a load instruction to reload a spilled register.
// Uses LOAD_reg which encodes as: LOAD rd, [FrameIndex + 0]
void MYISAInstrInfo::loadRegFromStackSlot(
    MachineBasicBlock &MBB, MachineBasicBlock::iterator MI, Register DestReg,
    int FrameIndex, const TargetRegisterClass *RC,
    const TargetRegisterInfo *TRI, Register VReg) const {
  DebugLoc DL;
  if (MI != MBB.end())
    DL = MI->getDebugLoc();

  // Emit: LOAD_reg DestReg, [FrameIndex + 0]
  BuildMI(MBB, MI, DL, get(MYISA::LOAD_reg), DestReg)
      .addFrameIndex(FrameIndex)
      .addImm(0);
}
