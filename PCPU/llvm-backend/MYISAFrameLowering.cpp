// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 Analog Devices, Inc.
//===-- MYISAFrameLowering.cpp - MYISA Frame Lowering --------------------===//
//
// WHAT THIS FILE IS:
//   Implements function prologue and epilogue generation for MYISA. This is
//   the code that runs at the very beginning and end of every compiled function
//   to set up and tear down the stack frame.
//
// WHY IT MUST EXIST:
//   C functions need a stack frame for:
//     - Saving the link register (return address) for non-leaf functions
//     - Saving callee-saved registers that the function uses
//     - Allocating space for local variables (arrays, structs, spill slots)
//   Without proper prologue/epilogue, function calls would corrupt registers
//   and return addresses, causing immediate crashes.
//
// WHAT EACH FUNCTION DOES:
//   hasFP():
//     Decides whether a dedicated frame pointer (r30) is needed. Most
//     functions can use SP-relative addressing, but variable-sized alloca()
//     calls require a fixed reference point (FP).
//
//   emitPrologue():
//     Generates the function entry sequence:
//       1. PUSH r3 (save return address — only for non-leaf functions)
//       2. PUSH r16, PUSH r17, ... (save any callee-saved regs used)
//       3. MOV r30, r2 (set up frame pointer — only if hasFP())
//       4. SUB r2, r2, #framesize (allocate local variable space)
//     The SUB must loop in chunks of 31 because the 5-bit immediate can
//     only encode 0–31 per instruction.
//
//   emitEpilogue():
//     Generates the function exit sequence (mirrors prologue in reverse):
//       1. MOV r2, r30 (restore SP from FP) — OR —
//          ADD r2, r2, #framesize (deallocate locals)
//       2. POP r17, POP r16, ... (restore callee-saved regs, reverse order)
//       3. POP r3 (restore return address)
//     After this, the RET instruction transfers control back to the caller.
//
//   eliminateCallFramePseudoInstr():
//     Removes ADJCALLSTACKDOWN/ADJCALLSTACKUP pseudos. These exist to
//     inform the frame lowering about outgoing call argument space, but
//     since we pre-allocate all needed stack space in the prologue, the
//     pseudos are simply erased (no run-time SP adjustment needed around calls).
//
// WHAT COULD BE ADDED:
//   - Dynamic frame size support for alloca() (adjust SP at runtime)
//   - Stack probes / guard pages for stack overflow detection
//   - CFI (Call Frame Information) directives for DWARF debug info
//   - Shrink-wrapping optimization (defer prologue past early-exit branches)
//   - Tail call support (skip epilogue when tail-calling)
//   - Store-multiple / load-multiple instructions for faster save/restore
//   - Red zone optimization (small leaf functions skip SP adjustment)
//   - Split-stack support for goroutine-style lightweight threads
//
//===----------------------------------------------------------------------===//

#include "MYISAFrameLowering.h"
#include "MYISA.h"
#include "MYISAInstrInfo.h"
#include "MYISASubtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"

using namespace llvm;

//===----------------------------------------------------------------------===//
// hasFP — Determine if a frame pointer is needed
//
// A frame pointer provides a fixed reference point in the stack frame that
// doesn't change as the SP is adjusted. It's needed when:
//   - The function uses variable-sized objects (alloca / VLAs)
//   - The frame address is taken (__builtin_frame_address)
// For all other functions, SP-relative addressing is sufficient and avoids
// dedicating r30 as the frame pointer (leaving it free for the allocator).
//===----------------------------------------------------------------------===//

bool MYISAFrameLowering::hasFP(const MachineFunction &MF) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();
  return MFI.hasVarSizedObjects() || MFI.isFrameAddressTaken();
}

//===----------------------------------------------------------------------===//
// emitPrologue — Generate function entry code
//===----------------------------------------------------------------------===//

void MYISAFrameLowering::emitPrologue(MachineFunction &MF,
                                       MachineBasicBlock &MBB) const {
  MachineBasicBlock::iterator MBBI = MBB.begin();
  const MYISASubtarget &STI = MF.getSubtarget<MYISASubtarget>();
  const MYISAInstrInfo &TII = *STI.getInstrInfo();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  DebugLoc DL;

  if (MBBI != MBB.end())
    DL = MBBI->getDebugLoc();

  // TODO: emit your function prologue. Use BuildMI(MBB, MBBI, DL, TII.get(...))
  //       with the FrameSetup flag. A typical prologue does, in order:
  //         1. Save the link/return-address register if this function calls
  //            others: if (MF.getFrameInfo().hasCalls()) PUSH the LR.
  //         2. Save each callee-saved register the function uses:
  //            for (auto &E : MFI.getCalleeSavedInfo()) PUSH E.getReg().
  //         3. If hasFP(MF), set the frame pointer to the current SP.
  //         4. Allocate locals by subtracting MFI.getStackSize() from SP
  //            (loop in chunks if your immediate field is narrow).
  //       See the Stage 3 tutorial for a worked example.
  (void)TII;
  (void)MFI;
}

//===----------------------------------------------------------------------===//
// emitEpilogue — Generate function exit code (mirrors prologue in reverse)
//===----------------------------------------------------------------------===//

void MYISAFrameLowering::emitEpilogue(MachineFunction &MF,
                                       MachineBasicBlock &MBB) const {
  MachineBasicBlock::iterator MBBI = MBB.getLastNonDebugInstr();
  const MYISASubtarget &STI = MF.getSubtarget<MYISASubtarget>();
  const MYISAInstrInfo &TII = *STI.getInstrInfo();
  MachineFrameInfo &MFI = MF.getFrameInfo();
  DebugLoc DL;

  if (MBBI != MBB.end())
    DL = MBBI->getDebugLoc();

  // TODO: emit your function epilogue — the mirror image of the prologue, in
  //       reverse order, using the FrameDestroy flag. A typical epilogue does:
  //         1. Deallocate locals: restore SP from the frame pointer (if
  //            hasFP(MF)) or add MFI.getStackSize() back to SP.
  //         2. Restore callee-saved registers in REVERSE order (iterate
  //            MFI.getCalleeSavedInfo() backwards and POP each).
  //         3. Restore the link/return-address register if it was saved.
  //       The RET instruction that follows returns control to the caller.
  (void)TII;
  (void)MFI;
}

//===----------------------------------------------------------------------===//
// eliminateCallFramePseudoInstr — Remove ADJCALLSTACKDOWN/UP pseudos
//
// These pseudo-instructions mark where outgoing call arguments are set up
// and torn down. Since MYISA allocates the maximum needed outgoing arg
// space in the prologue (included in FrameSize), no runtime SP adjustment
// is needed around individual calls — just erase the pseudos.
//===----------------------------------------------------------------------===//

MachineBasicBlock::iterator MYISAFrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator MI) const {
  return MBB.erase(MI);
}
