// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 Analog Devices, Inc.
//===-- MYISAISelDAGToDAG.cpp - MYISA DAG->DAG Instruction Selection -----===//
//
// WHAT THIS FILE IS:
//   Implements the instruction selection pass — the transformation from a
//   legalized SelectionDAG (abstract operations on virtual registers) into
//   MachineInstrs (concrete MYISA instructions). This is where abstract DAG
//   nodes like "add" become concrete opcodes like MYISA::ADD_rrr.
//
// WHY IT MUST EXIST:
//   While TableGen patterns (in MYISAInstrInfo.td) handle most instruction
//   selection automatically via SelectCode(), some operations need custom C++
//   logic that cannot be expressed as a TableGen pattern:
//     - Constant materialization (choosing between ADD_rri, LI, or NEG+LI)
//     - CALLSEQ_START/END handling (result type mismatch with TableGen)
//     - Custom node selection (MYISAISD::CALL, CMP, BR_CC)
//     - Complex addressing mode decomposition (SelectAddr)
//     - Peephole optimizations (ADD with negative constant → SUB)
//
// WHAT EACH PART DOES:
//   MYISADAGToDAGISel class:
//     - Inherits from SelectionDAGISel (LLVM's ISel framework)
//     - Includes MYISAGenDAGISel.inc (TableGen-generated pattern matcher)
//     - Overrides Select() to handle custom cases before falling through
//       to SelectCode() for pattern-based matching
//
//   Select() method (the core of this file):
//     Handles these cases before falling through to SelectCode():
//       ISD::Constant — Multi-strategy constant materialization:
//         0–31:      ADD rd, r0, #imm (single instruction, 5-bit immediate)
//         32–65535:  LI rd, #imm (single instruction, 16-bit immediate)
//         -1..-31:   ADD rd, r0, #abs; NEG rd, rd (two instructions)
//         -32..-65535: LI rd, #abs; NEG rd, rd (two instructions)
//         Others:    fall through to ISelLowering (LUI+OR, multi-insn)
//       ISD::ADD — Peephole: add(x, -small_const) → SUB_rri(x, abs)
//       ISD::CALLSEQ_START/END — Manual selection to ADJCALLSTACKDOWN/UP
//       MYISAISD::CALL — Repacks operands for CALL machine instruction
//       MYISAISD::CMP — Selects between CMP_rr and CMP_ri
//       MYISAISD::BR_CC — Maps condition codes to branch opcodes
//
//   SelectAddr() method:
//     Implements the ComplexPattern "addr" from MYISAInstrInfo.td.
//     Decomposes an address expression into (Base, Offset) for memory
//     instructions. Handles three cases:
//       FrameIndex → (FI, 0)
//       reg + const → (reg, const) if const fits in 16 bits
//       reg + reg → (reg, reg) with 0 offset
//       bare reg → (reg, 0)
//
// WHAT COULD BE ADDED:
//   - Peephole optimizations: combine LOAD+SIGN_EXTEND, fuse CMP+Branch pairs
//   - More aggressive constant materialization for large values (LUI sequences)
//   - Post-ISel peephole pass integration (addPreEmitPass)
//   - Custom lowering for multiply-accumulate if hardware adds MAC instruction
//   - Address mode folding for scaled-offset memory accesses
//   - Predicated instruction selection if hardware adds conditional execution
//   - VLIW scheduling considerations if hardware adds instruction bundles
//
//===----------------------------------------------------------------------===//

#include "MYISA.h"
#include "MYISAISelLowering.h"
#include "MYISASubtarget.h"
#include "MYISATargetMachine.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"

using namespace llvm;

#define DEBUG_TYPE "myisa-isel"

namespace {

// MYISADAGToDAGISel — The instruction selection pass for MYISA.
// This is a FunctionPass that runs on each function in the module.
// It transforms the SelectionDAG (after legalization) into MachineInstrs
// by calling Select() on each DAG node in bottom-up order.
class MYISADAGToDAGISel : public SelectionDAGISel {
  const MYISASubtarget *Subtarget;

public:
  static char ID;

  MYISADAGToDAGISel(MYISATargetMachine &TM, CodeGenOpt::Level OL)
      : SelectionDAGISel(ID, TM, OL) {}

  // runOnMachineFunction — Called once per function. Caches the subtarget
  // reference, then delegates to the base class which drives Select().
  bool runOnMachineFunction(MachineFunction &MF) override {
    Subtarget = &MF.getSubtarget<MYISASubtarget>();
    return SelectionDAGISel::runOnMachineFunction(MF);
  }

  // Select — Called for each DAG node. Our override handles custom cases;
  // unhandled nodes fall through to SelectCode() (TableGen patterns).
  void Select(SDNode *N) override;

  // SelectAddr — ComplexPattern implementation for memory address decomposition.
  // Called by the TableGen-generated code when matching the "addr" pattern.
  bool SelectAddr(SDValue Addr, SDValue &Base, SDValue &Offset);

  StringRef getPassName() const override {
    return "MYISA DAG->DAG Pattern Instruction Selection";
  }

// Include the TableGen-generated pattern matching code.
// SelectCode() is defined here — it's a giant switch/case that matches
// DAG patterns to MYISA instructions based on MYISAInstrInfo.td patterns.
#include "MYISAGenDAGISel.inc"
};

char MYISADAGToDAGISel::ID = 0;

} // end anonymous namespace

//===----------------------------------------------------------------------===//
// Select — The main instruction selection dispatch
//
// This is called bottom-up for every node in the SelectionDAG. Nodes that
// have already been selected (isMachineOpcode()) are skipped. Custom cases
// are handled by the switch statement; everything else falls through to
// SelectCode() which applies TableGen patterns.
//===----------------------------------------------------------------------===//

void MYISADAGToDAGISel::Select(SDNode *N) {
  // Already selected by a previous pass or TableGen — skip.
  if (N->isMachineOpcode()) {
    N->setNodeId(-1);
    return;
  }

  switch (N->getOpcode()) {
  default:
    break;  // Fall through to SelectCode() for TableGen pattern matching

  // TODO: handle the DAG nodes that need custom C++ selection (i.e. anything
  //       that cannot be expressed as a simple TableGen pattern in the .td).
  //       For each case, build the machine instruction(s) with
  //       CurDAG->getMachineNode(...) and finish with ReplaceNode(N, ...);
  //       then `return;`. Cases you typically need to implement:
  //
  //         case ISD::Constant:   materialise an integer constant using the
  //             cheapest sequence your ISA can encode (small immediate, load-
  //             immediate, load-upper + or, negate, ...).
  //         case ISD::CALLSEQ_START / ISD::CALLSEQ_END:  emit your
  //             ADJCALLSTACKDOWN / ADJCALLSTACKUP pseudos (these usually must
  //             be selected by hand because of their {chain, glue} result).
  //         case <YourISD>::CALL:  repack the call operands into machine form.
  //         case <YourISD>::CMP:   pick the register vs. immediate compare.
  //         case <YourISD>::BR_CC: map the LLVM condition code onto your
  //             conditional-branch opcodes (JZ/JNZ/JLT/JGT or equivalents).
  //
  //       You may also add peephole cases (e.g. rewrite ADD(x, -k) into a
  //       SUB) here. The Stage 2–3 tutorials walk through each case above.
  }

  // No custom match — try TableGen-generated patterns (SelectCode).
  // This handles all the simple cases: ADD_rrr, SUB_rrr, LOAD_reg, etc.
  SelectCode(N);
}

//===----------------------------------------------------------------------===//
// SelectAddr — Complex pattern implementation for memory addressing
//
// This function is called by the TableGen-generated ISel code whenever a
// memory instruction uses the "addr" ComplexPattern. It decomposes an address
// DAG node into a (Base, Offset) pair suitable for the LOAD_reg/STORE_reg
// instruction format: LOAD rd, [Base + Offset].
//
// Cases handled:
//   1. FrameIndex → (FI, 0) — stack-allocated variables
//   2. reg + const → (reg, const) — if constant fits in signed 16 bits
//   3. reg + reg → (reg, reg) — base + index (offset as register)
//   4. bare reg → (reg, 0) — pointer dereference with no offset
//===----------------------------------------------------------------------===//

bool MYISADAGToDAGISel::SelectAddr(SDValue Addr, SDValue &Base,
                                    SDValue &Offset) {
  // Case 1: Frame index (stack variable access)
  // Convert to TargetFrameIndex which will be replaced with SP+offset
  // during frame index elimination (MYISARegisterInfo::eliminateFrameIndex).
  if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
    Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i32);
    Offset = CurDAG->getTargetConstant(0, SDLoc(Addr), MVT::i32);
    return true;
  }

  // TODO: recognise richer addressing modes so the compiler can fold address
  //       arithmetic into your load/store instructions. You may additionally
  //       match:
  //         - reg + constant  -> (Base = reg,  Offset = constant)  when the
  //           constant fits your instruction's offset field
  //         - reg + reg       -> (Base = reg,  Offset = index reg)
  //       Match ISD::ADD here and set Base/Offset accordingly before falling
  //       through to the bare-register case below.

  // Fallback: use the whole expression as the base with a zero offset.
  Base = Addr;
  Offset = CurDAG->getTargetConstant(0, SDLoc(Addr), MVT::i32);
  return true;
}

//===----------------------------------------------------------------------===//
// Factory function — Creates and returns the ISel pass instance.
// Called by MYISAPassConfig::addInstSelector() in MYISATargetMachine.cpp.
//===----------------------------------------------------------------------===//

FunctionPass *llvm::createMYISAISelDag(MYISATargetMachine &TM) {
  return new MYISADAGToDAGISel(TM, CodeGenOpt::Default);
}
