// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 Analog Devices, Inc.
//===-- MYISAISelLowering.cpp - MYISA DAG Lowering -----------------------===//
//
// WHAT THIS FILE IS:
//   Implementation of the MYISATargetLowering class — the "legalization and
//   lowering" component of the backend. This is one of the most important
//   files in any LLVM backend because it teaches the compiler:
//     1. Which operations the hardware can do directly (Legal)
//     2. Which operations need to be replaced by multi-instruction sequences (Custom)
//     3. Which operations must be expanded to library calls or simpler ops (Expand)
//     4. How function calls and returns work at the machine level
//
// WHY IT MUST EXIST:
//   LLVM IR can express operations that MYISA hardware cannot do (e.g., float
//   math, 64-bit integers, hardware divide). The legalizer runs before ISel
//   and uses the rules declared here to transform the DAG until every node is
//   something the backend can handle. Without this file, the ISel would crash
//   on illegal operations.
//
// WHAT EACH SECTION DOES:
//   Constructor:
//     - Registers the GPR class for i32 (the only data type we support)
//     - Declares every unsupported operation as Expand or Custom
//     - Promotes sub-i32 types (i1, i8, i16) to i32 so the legalizer
//       doesn't try to generate 8-bit or 16-bit operations
//
//   LowerOperation dispatch:
//     - Routes Custom-marked operations to their specific lowering methods
//
//   LowerBR_CC (conditional branch):
//     - Converts the generic ISD::BR_CC(cc, lhs, rhs, target) into:
//       MYISAISD::CMP(lhs, rhs) → MYISAISD::BR_CC(cc, target)
//     - Handles SETLE→SETLT and SETGE→SETGT conversions (no direct hardware)
//
//   LowerSELECT_CC (conditional select):
//     - Partially implemented — complex cases fall back to branch expansion
//
//   LowerGlobalAddress:
//     - Resolves global variable references to 16-bit addresses
//
//   LowerFormalArguments (callee side):
//     - Reads arguments from r8–r15 or stack using CC_MYISA rules
//     - Creates virtual registers for each argument
//
//   LowerReturn:
//     - Copies the return value to r8, emits MYISAISD::RET_FLAG
//
//   LowerCall (caller side):
//     - Emits CALLSEQ_START, copies args to r8–r15 / stack, emits CALL,
//       emits CALLSEQ_END, reads return value from r8
//
// WHAT COULD BE ADDED:
//   - LowerVASTART/LowerVACOPY for variadic functions (printf-style)
//   - LowerDYNAMIC_STACKALLOC for C99 variable-length arrays
//   - LowerATOMIC_* for multi-threaded atomic operations
//   - LowerFRAMEADDR/LowerRETURNADDR for __builtin_frame_address etc.
//   - LowerBlockAddress/LowerJumpTable for efficient switch statements
//   - Improved SELECT_CC using conditional-move if hardware adds CMOV
//   - Unsigned comparison support (currently uses signed JLT/JGT for all)
//   - Position-independent code (PIC) support for shared libraries
//   - Thread-local storage (TLS) lowering
//   - Custom constant pool lowering for large constants
//
//===----------------------------------------------------------------------===//

#include "MYISAISelLowering.h"
#include "MYISA.h"
#include "MYISASubtarget.h"
#include "MYISATargetMachine.h"
#include "llvm/CodeGen/CallingConvLower.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"
#include "llvm/IR/Function.h"

using namespace llvm;

// Include the auto-generated calling convention assignment functions.
// This provides CC_MYISA() and RetCC_MYISA() — the logic for assigning
// arguments and return values to registers or stack slots.
#include "MYISAGenCallingConv.inc"

//===----------------------------------------------------------------------===//
// Constructor — Operation legality declarations
//
// This is where we tell LLVM what operations MYISA supports. The rules are:
//   Legal (default): hardware has a direct instruction for this
//   Custom: we handle it ourselves in LowerOperation()
//   Expand: LLVM replaces it with a sequence of legal ops or a libcall
//   Promote: widen to a larger type (e.g., i8 → i32)
//===----------------------------------------------------------------------===//

MYISATargetLowering::MYISATargetLowering(const TargetMachine &TM,
                                           const MYISASubtarget &STI)
    : TargetLowering(TM) {

  // Register the GPR class as the container for i32 values.
  // This tells the register allocator: "i32 virtual registers should be
  // assigned to physical registers from the GPR class."
  addRegisterClass(MVT::i32, &MYISA::GPRRegClass);

  // Compute derived properties (e.g., which types are legal for which ops)
  // based on the registered classes. Must be called after addRegisterClass.
  computeRegisterProperties(STI.getRegisterInfo());

  // Prefer register-pressure-aware scheduling (good for register-constrained
  // targets — reduces spills by scheduling to minimize live ranges).
  setSchedulingPreference(Sched::RegPressure);

  // Tell LLVM which register is the stack pointer (for stack-related opts).
  setStackPointerRegisterToSaveRestore(MYISA::R2);

  // TODO: declare the legality of operations for your ISA. Anything you do NOT
  //       configure here is assumed Legal (i.e. you have a direct instruction
  //       and a matching pattern in the .td files). Use:
  //         setOperationAction(ISD::<op>, MVT::<ty>, Custom); // handle in LowerOperation()
  //         setOperationAction(ISD::<op>, MVT::<ty>, Expand); // let LLVM decompose / libcall
  //         setOperationAction(ISD::<op>, MVT::<ty>, Promote);// widen a narrow type
  //
  //       Things you will almost certainly need to configure:
  //         - Custom-lower control flow that needs a compare + branch pair,
  //           e.g.  setOperationAction(ISD::BR_CC, MVT::i32, Custom);
  //                 setOperationAction(ISD::SELECT_CC, MVT::i32, Custom);
  //         - Custom-lower global variable addresses, e.g.
  //                 setOperationAction(ISD::GlobalAddress, MVT::i32, Custom);
  //         - Expand operations your hardware lacks (division, floating point,
  //           rotates, bit-counting, byte-swap, dynamic stack alloc, ...).
  //         - Promote sub-word integer ops (i1/i8/i16) to your native width so
  //           the type legalizer does not try to emit narrow operations, e.g.
  //             for (MVT VT : {MVT::i1, MVT::i8, MVT::i16})
  //               setOperationAction(ISD::ADD, VT, Promote); // ...and friends
  //         - setBooleanContents(ZeroOrOneBooleanContent);
  //
  //       See the Stage 1 tutorial for a worked example.
}

//===----------------------------------------------------------------------===//
// getTargetNodeName — Returns a printable name for custom DAG node types.
// Used by SelectionDAG::dump() and -debug output to show readable node names
// instead of raw opcode numbers.
//===----------------------------------------------------------------------===//

const char *MYISATargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (Opcode) {
  case MYISAISD::RET_FLAG:   return "MYISAISD::RET_FLAG";
  case MYISAISD::CALL:       return "MYISAISD::CALL";
  case MYISAISD::CMP:        return "MYISAISD::CMP";
  case MYISAISD::BR_CC:      return "MYISAISD::BR_CC";
  default:                    return nullptr;
  }
}

//===----------------------------------------------------------------------===//
// LowerOperation — Dispatch for custom-lowered operations.
// Called by the legalizer whenever it encounters an operation marked Custom.
//===----------------------------------------------------------------------===//

SDValue MYISATargetLowering::LowerOperation(SDValue Op,
                                             SelectionDAG &DAG) const {
  switch (Op.getOpcode()) {
  case ISD::BR_CC:         return LowerBR_CC(Op, DAG);
  case ISD::SELECT_CC:     return LowerSELECT_CC(Op, DAG);
  case ISD::GlobalAddress: return LowerGlobalAddress(Op, DAG);
  default:
    llvm_unreachable("Unexpected operation to lower");
  }
}

//===----------------------------------------------------------------------===//
// LowerBR_CC — Conditional branch lowering
//
// LLVM's generic ISD::BR_CC node represents: "if (LHS cc RHS) goto target"
// MYISA hardware does this in two steps:
//   1. CMP rs1, rs2 → writes result to r5
//   2. Jcc target   → branches based on r5 value
//
// This function emits MYISAISD::CMP and MYISAISD::BR_CC nodes that the
// instruction selector (MYISAISelDAGToDAG.cpp) will later convert to
// physical CMP_rr/CMP_ri + JZ/JNZ/JLT/JGT instructions.
//
// Special handling for SETLE/SETGE:
//   MYISA has no "less-or-equal" or "greater-or-equal" branch. We convert:
//     SETLE(a, b) → SETLT(a, b+1)   (a ≤ b ↔ a < b+1)
//     SETGE(a, b) → SETGT(a, b-1)   (a ≥ b ↔ a > b-1)
//   The ADD/SUB nodes will be folded if b is a constant.
//===----------------------------------------------------------------------===//

SDValue MYISATargetLowering::LowerBR_CC(SDValue Op, SelectionDAG &DAG) const {
  // TODO: lower a generic conditional branch "if (LHS cc RHS) goto Target"
  //       into your ISA's compare-then-branch idiom.
  //       The operands of an ISD::BR_CC node are:
  //         Op.getOperand(0) = chain
  //         Op.getOperand(1) = condition code (cast<CondCodeSDNode>(...)->get())
  //         Op.getOperand(2) = LHS,  Op.getOperand(3) = RHS
  //         Op.getOperand(4) = target basic block
  //       A common approach: emit a custom CMP node (e.g. MYISAISD::CMP) that
  //       sets your condition register, then a custom branch node
  //       (e.g. MYISAISD::BR_CC) carrying the condition code and target.
  //       If your ISA has no less-or-equal / greater-or-equal branch, rewrite
  //       SETLE/SETGE into SETLT/SETGT by adjusting the RHS by 1.
  //       See the Stage 2 tutorial for a worked example.
  return SDValue();
}

//===----------------------------------------------------------------------===//
// LowerSELECT_CC — Conditional select lowering (partially implemented)
//
// SELECT_CC(lhs, rhs, trueval, falseval, cc) selects one of two values based
// on a comparison. Without a hardware conditional-move instruction, this must
// be expanded to a branch sequence:
//   CMP lhs, rhs
//   Jcc .true
//   MOV result, falseval
//   JMP .end
// .true:
//   MOV result, trueval
// .end:
//
// Currently returns SDValue() to signal that the generic expansion (via
// branches and phi nodes) should be used instead.
//===----------------------------------------------------------------------===//

SDValue MYISATargetLowering::LowerSELECT_CC(SDValue Op,
                                              SelectionDAG &DAG) const {
  // TODO: lower a conditional select "result = (LHS cc RHS) ? TrueVal : FalseVal".
  //       Operands: 0=LHS, 1=RHS, 2=TrueVal, 3=FalseVal, 4=condition code.
  //       If your ISA has a conditional-move instruction, emit it here. If it
  //       does not, return SDValue() to fall back to LLVM's generic expansion
  //       into a compare + branch + phi sequence (perfectly fine to start with).
  return SDValue();
}

//===----------------------------------------------------------------------===//
// LowerGlobalAddress — Global variable address lowering
//
// When C code references a global variable, LLVM emits a GlobalAddress node.
// Since MYISA has a 16-bit address space, all globals fit in a single 16-bit
// immediate — no GOT, PLT, or multi-instruction address materialization needed.
//
// This function converts the generic GlobalAddress to a TargetGlobalAddress
// (which the instruction selector can match against LI or LOAD_abs).
//===----------------------------------------------------------------------===//

SDValue MYISATargetLowering::LowerGlobalAddress(SDValue Op,
                                                 SelectionDAG &DAG) const {
  SDLoc DL(Op);
  const GlobalAddressSDNode *GA = cast<GlobalAddressSDNode>(Op.getNode());
  EVT VT = Op.getValueType();

  // Convert to TargetGlobalAddress — this signals to the ISel that the
  // address has been "lowered" and can be used directly in an instruction's
  // immediate field. The 16-bit address space means no relocation splitting.
  SDValue Addr = DAG.getTargetGlobalAddress(GA->getGlobal(), DL, VT,
                                            GA->getOffset());
  return Addr;
}

//===----------------------------------------------------------------------===//
// LowerFormalArguments — Callee-side argument reception
//
// Called once per function to set up the function's arguments. For each arg:
//   - If it was assigned to a register (r8–r15): create a virtual register,
//     add the physical register as a live-in, and copy it to the vreg.
//   - If it was assigned to a stack slot: create a fixed stack object at the
//     correct offset and load from it.
//
// The assignment is determined by CC_MYISA (generated from MYISACallingConv.td).
//===----------------------------------------------------------------------===//

SDValue MYISATargetLowering::LowerFormalArguments(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &DL,
    SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {

  MachineFunction &MF = DAG.getMachineFunction();
  MachineRegisterInfo &RegInfo = MF.getRegInfo();

  // Run the calling convention analysis to determine where each arg lives.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeFormalArguments(Ins, CC_MYISA);

  for (auto &VA : ArgLocs) {
    if (VA.isRegLoc()) {
      // Argument was passed in a register (r8–r15).
      // Create a virtual register, mark the physical reg as live-in, and
      // emit a CopyFromReg to move the value into the vreg.
      Register VReg = RegInfo.createVirtualRegister(&MYISA::GPRRegClass);
      RegInfo.addLiveIn(VA.getLocReg(), VReg);
      SDValue ArgVal = DAG.getCopyFromReg(Chain, DL, VReg, MVT::i32);
      InVals.push_back(ArgVal);
    } else {
      // Argument was passed on the stack.
      // Create a fixed stack object at the caller-assigned offset and load.
      int FI = MF.getFrameInfo().CreateFixedObject(4, VA.getLocMemOffset(),
                                                    true);
      SDValue FIN = DAG.getFrameIndex(FI, MVT::i32);
      SDValue Load = DAG.getLoad(MVT::i32, DL, Chain, FIN,
                                 MachinePointerInfo::getFixedStack(MF, FI));
      InVals.push_back(Load);
    }
  }

  return Chain;
}

//===----------------------------------------------------------------------===//
// LowerReturn — Function return lowering
//
// Emits the sequence to return from a function:
//   1. Copy return value to r8 (if there is one)
//   2. Emit MYISAISD::RET_FLAG which becomes the RET instruction
//
// The return value register (r8) is determined by RetCC_MYISA.
//===----------------------------------------------------------------------===//

SDValue MYISATargetLowering::LowerReturn(
    SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
    const SmallVectorImpl<ISD::OutputArg> &Outs,
    const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
    SelectionDAG &DAG) const {

  // Determine where each return value should go (r8 for i32).
  SmallVector<CCValAssign, 16> RVLocs;
  CCState CCInfo(CallConv, IsVarArg, DAG.getMachineFunction(), RVLocs,
                 *DAG.getContext());
  CCInfo.AnalyzeReturn(Outs, RetCC_MYISA);

  SDValue Glue;
  SmallVector<SDValue, 4> RetOps;
  RetOps.push_back(Chain);  // First operand is always the chain

  // Copy each return value to its assigned physical register.
  for (unsigned i = 0; i < RVLocs.size(); ++i) {
    CCValAssign &VA = RVLocs[i];
    Chain = DAG.getCopyToReg(Chain, DL, VA.getLocReg(), OutVals[i], Glue);
    Glue = Chain.getValue(1);  // Glue ensures copy happens before return
    RetOps.push_back(DAG.getRegister(VA.getLocReg(), VA.getLocVT()));
  }

  RetOps[0] = Chain;  // Update chain after all copies
  if (Glue.getNode())
    RetOps.push_back(Glue);  // Attach final glue

  // Emit MYISAISD::RET_FLAG — this becomes the RET machine instruction.
  return DAG.getNode(MYISAISD::RET_FLAG, DL, MVT::Other, RetOps);
}

//===----------------------------------------------------------------------===//
// LowerCall — Caller-side function call lowering
//
// This is the most complex lowering function. It implements the full call
// sequence:
//   1. Analyze outgoing arguments (CC_MYISA assigns regs/stack)
//   2. CALLSEQ_START (marks beginning of outgoing arg area)
//   3. Copy register args to physical registers (r8–r15)
//   4. Store stack args to the outgoing argument area
//   5. Emit MYISAISD::CALL with all operands
//   6. CALLSEQ_END (marks end of outgoing arg area)
//   7. Copy return value from r8 back to a virtual register
//
// The CALLSEQ_START/END pseudo-instructions bracket the call and allow
// the frame lowering to account for outgoing argument stack space.
//===----------------------------------------------------------------------===//

SDValue MYISATargetLowering::LowerCall(
    TargetLowering::CallLoweringInfo &CLI,
    SmallVectorImpl<SDValue> &InVals) const {

  SelectionDAG &DAG = CLI.DAG;
  SDLoc &DL = CLI.DL;
  SmallVectorImpl<ISD::OutputArg> &Outs = CLI.Outs;
  SmallVectorImpl<SDValue> &OutVals = CLI.OutVals;
  SmallVectorImpl<ISD::InputArg> &Ins = CLI.Ins;
  SDValue Chain = CLI.Chain;
  SDValue Callee = CLI.Callee;
  CallingConv::ID CallConv = CLI.CallConv;
  bool IsVarArg = CLI.IsVarArg;

  // MYISA does not support tail call optimization. Clear the flag so the
  // SelectionDAG builder emits the caller's RET_FLAG after this call.
  CLI.IsTailCall = false;

  MachineFunction &MF = DAG.getMachineFunction();

  // Step 1: Analyze outgoing arguments to determine register/stack assignment.
  SmallVector<CCValAssign, 16> ArgLocs;
  CCState CCInfo(CallConv, IsVarArg, MF, ArgLocs, *DAG.getContext());
  CCInfo.AnalyzeCallOperands(Outs, CC_MYISA);

  // Step 2: Emit CALLSEQ_START to mark the beginning of the outgoing arg area.
  unsigned StackSize = CCInfo.getStackSize();
  Chain = DAG.getCALLSEQ_START(Chain, StackSize, 0, DL);

  SmallVector<std::pair<unsigned, SDValue>, 8> RegsToPass;
  SmallVector<SDValue, 8> MemOpChains;

  // Step 3 & 4: Process each argument — either queue for register copy
  // or store to the stack.
  for (unsigned i = 0; i < ArgLocs.size(); ++i) {
    CCValAssign &VA = ArgLocs[i];
    SDValue Arg = OutVals[i];

    if (VA.isRegLoc()) {
      // Argument goes in a register — queue it for the CopyToReg chain below.
      RegsToPass.push_back(std::make_pair(VA.getLocReg(), Arg));
    } else {
      // Argument goes on the stack — compute address and store.
      SDValue StackPtr = DAG.getCopyFromReg(Chain, DL, MYISA::R2, MVT::i32);
      SDValue PtrOff = DAG.getIntPtrConstant(VA.getLocMemOffset(), DL);
      SDValue Addr = DAG.getNode(ISD::ADD, DL, MVT::i32, StackPtr, PtrOff);
      MemOpChains.push_back(
          DAG.getStore(Chain, DL, Arg, Addr, MachinePointerInfo()));
    }
  }

  // Merge all stack stores into a single chain (they can execute in any order).
  if (!MemOpChains.empty())
    Chain = DAG.getNode(ISD::TokenFactor, DL, MVT::Other, MemOpChains);

  // Build a chain of CopyToReg nodes for register arguments.
  // Glue ensures all copies happen atomically before the CALL.
  SDValue InGlue;
  for (auto &Reg : RegsToPass) {
    Chain = DAG.getCopyToReg(Chain, DL, Reg.first, Reg.second, InGlue);
    InGlue = Chain.getValue(1);
  }

  // Step 5: Build the CALL node operand list.
  SmallVector<SDValue, 8> Ops;
  Ops.push_back(Chain);    // Chain input
  Ops.push_back(Callee);   // Call target (function address or symbol)

  // Add implicit register uses for each register argument.
  for (auto &Reg : RegsToPass)
    Ops.push_back(DAG.getRegister(Reg.first, MVT::i32));

  // Add register mask (tells RA which regs survive the call = callee-saved).
  Ops.push_back(DAG.getRegisterMask(
      MF.getSubtarget<MYISASubtarget>()
          .getRegisterInfo()
          ->getCallPreservedMask(MF, CallConv)));

  // Add input glue if present.
  if (InGlue.getNode())
    Ops.push_back(InGlue);

  // Emit MYISAISD::CALL node. Result types: {chain, glue}.
  SDVTList NodeTys = DAG.getVTList(MVT::Other, MVT::Glue);
  Chain = DAG.getNode(MYISAISD::CALL, DL, NodeTys, Ops);
  InGlue = Chain.getValue(1);

  // Step 6: Emit CALLSEQ_END to mark the end of the outgoing arg area.
  Chain = DAG.getCALLSEQ_END(Chain, StackSize, 0, InGlue, DL);
  InGlue = Chain.getValue(1);

  // Step 7: Handle return values — copy from physical register (r8) to vreg.
  SmallVector<CCValAssign, 16> RVLocs;
  CCState RVInfo(CallConv, IsVarArg, MF, RVLocs, *DAG.getContext());
  RVInfo.AnalyzeCallResult(Ins, RetCC_MYISA);

  for (auto &VA : RVLocs) {
    Chain = DAG.getCopyFromReg(Chain, DL, VA.getLocReg(), VA.getValVT(),
                               InGlue).getValue(1);
    InGlue = Chain.getValue(2);
    InVals.push_back(Chain.getValue(0));
  }

  return Chain;
}
