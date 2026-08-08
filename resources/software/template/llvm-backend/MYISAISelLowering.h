// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 Analog Devices, Inc.
//===-- MYISAISelLowering.h - MYISA DAG Lowering -------------*- C++ -*-===//
//
// WHAT THIS FILE IS:
//   Declares the MYISATargetLowering class and the MYISAISD namespace.
//   TargetLowering is LLVM's interface for telling the code generator which
//   operations the target supports natively, which need custom handling,
//   and which should be expanded into simpler operations or library calls.
//
// WHY IT MUST EXIST:
//   Without a TargetLowering subclass, LLVM's SelectionDAG legalizer doesn't
//   know what's legal for MYISA. It would try to emit operations (like
//   hardware divide or floating-point math) that don't exist, causing crashes.
//   This class also implements the call ABI: how arguments are passed, how
//   return values come back, and how function calls are lowered to machine code.
//
// WHAT EACH PART DOES:
//   MYISAISD namespace:
//     Defines custom SelectionDAG node types that exist only for MYISA.
//     These nodes are emitted by the lowering methods below and consumed by
//     the instruction selector (MYISAISelDAGToDAG.cpp).
//       - RET_FLAG: return from function (matched by the RET instruction)
//       - CALL: function call (matched by the CALL instruction)
//       - CMP: compare two values, write result to r5
//       - BR_CC: conditional branch (uses result from CMP)
//
//   MYISATargetLowering class:
//     - Constructor: declares legal/custom/expand for every ISD operation
//     - LowerOperation(): dispatch for custom-lowered operations
//     - LowerFormalArguments(): implements callee-side argument reception
//     - LowerReturn(): implements function return (copies to r8, emits RET_FLAG)
//     - LowerCall(): implements caller-side call sequence
//     - LowerBR_CC(): converts conditional branch into CMP + Jcc
//     - LowerSELECT_CC(): converts select-on-condition (partially implemented)
//     - LowerGlobalAddress(): resolves global variable addresses
//
// WHAT COULD BE ADDED:
//   - LowerVASTART / LowerVACOPY for variadic function support
//   - LowerDYNAMIC_STACKALLOC for alloca() support
//   - LowerATOMIC_* for atomic operations (if hardware supports them)
//   - LowerFRAMEADDR / LowerRETURNADDR for debugging intrinsics
//   - LowerBlockAddress / LowerJumpTable for switch statement optimization
//   - LowerEH_RETURN for C++ exception handling
//   - Custom SDNode types for target-specific operations (MAC, saturating ops)
//   - getSetCCResultType() override for custom boolean representation
//   - shouldConvertConstantLoadToIntImm() for constant materialization tuning
//   - isLegalAddressingMode() for addressing mode cost model
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIB_TARGET_MYISA_MYISAISELLOWERING_H
#define LLVM_LIB_TARGET_MYISA_MYISAISELLOWERING_H

#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {

class MYISASubtarget;

// MYISAISD — Custom SelectionDAG node opcodes for MYISA.
// These extend LLVM's built-in ISD opcodes (which end at BUILTIN_OP_END).
// Custom nodes are created in the lowering methods and matched in ISel.
namespace MYISAISD {
enum NodeType : unsigned {
  FIRST_NUMBER = ISD::BUILTIN_OP_END,  // Start numbering after built-in ops
  RET_FLAG,     // Function return — terminates function, uses r3 for PC restore
  CALL,         // Function call — jumps to target, clobbers r3 and caller-saved
  CMP,          // Compare — writes comparison result to r5 (implicit def)
  BR_CC,        // Conditional branch — reads r5, jumps if condition met
};
} // end namespace MYISAISD

// MYISATargetLowering — The main legalization and lowering class.
// Inherits from TargetLowering which provides the interface for:
//   - Declaring what's legal (setOperationAction)
//   - Custom-lowering specific operations (LowerOperation)
//   - Call ABI implementation (LowerFormalArguments, LowerReturn, LowerCall)
class MYISATargetLowering : public TargetLowering {
public:
  explicit MYISATargetLowering(const TargetMachine &TM,
                                const MYISASubtarget &STI);

  // Returns a human-readable name for custom node types (for debug dumps).
  const char *getTargetNodeName(unsigned Opcode) const override;

  // Dispatch function for custom-lowered operations. Called by the legalizer
  // when it encounters an operation marked "Custom" in the constructor.
  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;

  // --- Call ABI implementation ---

  // LowerFormalArguments: called for the callee side — reads function arguments
  // from their assigned locations (registers r8-r15 or stack) into virtual regs.
  SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv,
                               bool IsVarArg,
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &DL, SelectionDAG &DAG,
                               SmallVectorImpl<SDValue> &InVals) const override;

  // LowerReturn: called when the function returns — copies the return value
  // to r8 and emits RET_FLAG.
  SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
                      SelectionDAG &DAG) const override;

  // LowerCall: called for the caller side — sets up arguments, emits CALL,
  // and retrieves the return value.
  SDValue LowerCall(TargetLowering::CallLoweringInfo &CLI,
                    SmallVectorImpl<SDValue> &InVals) const override;

private:
  // Custom lowering helpers:
  SDValue LowerBR_CC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerSELECT_CC(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const;
};

} // end namespace llvm

#endif
