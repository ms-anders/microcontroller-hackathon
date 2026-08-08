// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 Analog Devices, Inc.
//===-- MYISAAsmPrinter.cpp - MYISA Assembly Printer ---------------------===//
//
// WHAT THIS FILE IS:
//   Implements the MYISAAsmPrinter class — the bridge between the code
//   generator's MachineInstr representation and the MC (Machine Code) layer's
//   MCInst representation. The AsmPrinter is the last codegen pass; it takes
//   fully-allocated, scheduled MachineInstrs and "lowers" them to MCInsts
//   which are then rendered as assembly text by the MCStreamer.
//
// WHY IT MUST EXIST:
//   MachineInstr and MCInst are different representations:
//     - MachineInstr: high-level, has virtual register info, frame indices,
//       machine basic block references, implicit operands, debug info.
//     - MCInst: low-level, has only physical register numbers, immediates,
//       and MC expressions (symbols). Ready for text output or binary encoding.
//   The AsmPrinter converts from one to the other. Without it, no assembly
//   output (.s file) can be produced.
//
// WHAT EACH PART DOES:
//   MYISAAsmPrinter class:
//     - Inherits from AsmPrinter (LLVM's generic assembly output framework)
//     - Overrides emitInstruction() to lower each MachineInstr to MCInst
//     - Overrides PrintAsmOperand/PrintAsmMemoryOperand for inline asm support
//
//   emitInstruction():
//     For each non-pseudo MachineInstr, creates an MCInst by:
//       1. Copying the opcode
//       2. Converting each operand:
//          - MO_Register → MCOperand::createReg (physical reg number)
//          - MO_Immediate → MCOperand::createImm (literal value)
//          - MO_MachineBasicBlock → MCOperand::createExpr (symbol reference)
//          - MO_GlobalAddress → MCOperand::createExpr (symbol reference)
//       3. Emitting the MCInst to the MCStreamer (which prints it as text)
//     Pseudo-instructions (ADJCALLSTACKDOWN/UP) are skipped (isPseudo() check).
//
//   LLVMInitializeMYISAAsmPrinter():
//     Registration function — tells LLVM "use MYISAAsmPrinter for the MYISA
//     target". Called during startup alongside other Init functions.
//
// WHAT COULD BE ADDED:
//   - A dedicated MCInstLowering class (cleaner separation of concerns;
//     standard practice in mature backends like ARM, RISC-V, X86)
//   - Handling for more operand types: MO_ExternalSymbol (for libcalls),
//     MO_ConstantPoolIndex, MO_JumpTableIndex, MO_BlockAddress
//   - emitFunctionBodyStart/End() for function-level directives (.type, .size)
//   - emitStartOfAsmFile() for file-level directives (.cpu, .arch)
//   - Custom MCExpr subclass for target-specific relocations
//   - DWARF debug info emission (requires CFI directives in prologue/epilogue)
//   - Exception handling table emission (.gcc_except_table)
//   - Inline assembly constraint validation (validateAsmConstraint)
//   - Constant pool emission for large constants
//
//===----------------------------------------------------------------------===//

#include "MYISA.h"
#include "MYISATargetMachine.h"
#include "MCTargetDesc/MYISAInstPrinter.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbolELF.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "myisa-asm-printer"

namespace {

// MYISAAsmPrinter — Converts MachineInstrs to MCInsts and emits assembly.
// This is registered as the AsmPrinter for the MYISA target via
// RegisterAsmPrinter<> in LLVMInitializeMYISAAsmPrinter().
class MYISAAsmPrinter : public AsmPrinter {
public:
  explicit MYISAAsmPrinter(TargetMachine &TM,
                            std::unique_ptr<MCStreamer> Streamer)
      : AsmPrinter(TM, std::move(Streamer)) {}

  StringRef getPassName() const override { return "MYISA Assembly Printer"; }

  // emitInstruction — Called once per MachineInstr in the final output order.
  // Lowers the MachineInstr to an MCInst and sends it to the MCStreamer.
  void emitInstruction(const MachineInstr *MI) override;

  // PrintAsmOperand — Prints a single operand for inline assembly.
  // Called when the compiler encounters asm("..." : constraints) in C code.
  bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                       const char *ExtraCode, raw_ostream &OS) override;

  // PrintAsmMemoryOperand — Prints a memory operand for inline assembly.
  // Renders as "[reg + offset]" format.
  bool PrintAsmMemoryOperand(const MachineInstr *MI, unsigned OpNo,
                             const char *ExtraCode, raw_ostream &OS) override;
};

} // end anonymous namespace

//===----------------------------------------------------------------------===//
// emitInstruction — The core lowering function
//
// For each MachineInstr, we build an MCInst by translating operands:
//   - Pseudo-instructions are skipped (they have no encoding)
//   - Implicit operands (Defs/Uses from .td) are skipped (not in MCInst)
//   - Each explicit operand is converted to its MCInst equivalent
//
// A more mature backend would use a separate MCInstLowering class with
// per-operand-type methods, but for a simple backend this inline approach
// is sufficient and easier to understand.
//===----------------------------------------------------------------------===//

void MYISAAsmPrinter::emitInstruction(const MachineInstr *MI) {
  // Skip pseudo / meta instructions (ADJCALLSTACKDOWN, ADJCALLSTACKUP,
  // DBG_VALUE, IMPLICIT_DEF, etc.) — they have no assembly representation.
  if (MI->isPseudo())
    return;

  // Create a fresh MCInst and copy the opcode.
  MCInst TmpInst;
  TmpInst.setOpcode(MI->getOpcode());

  // Convert each explicit operand from MachineOperand to MCOperand.
  for (const MachineOperand &MO : MI->operands()) {
    // Skip implicit operands (Defs=[R5], Uses=[R2], etc. from .td file).
    // These are tracked at the MachineInstr level for register allocation
    // but don't appear in the MCInst or encoded instruction.
    if (MO.isImplicit())
      continue;

    switch (MO.getType()) {
    case MachineOperand::MO_Register:
      // Physical register → MCOperand register
      TmpInst.addOperand(MCOperand::createReg(MO.getReg()));
      break;

    case MachineOperand::MO_Immediate:
      // Literal integer → MCOperand immediate
      TmpInst.addOperand(MCOperand::createImm(MO.getImm()));
      break;

    case MachineOperand::MO_MachineBasicBlock:
      // Branch target (basic block label) → MCOperand expression (symbol ref)
      TmpInst.addOperand(MCOperand::createExpr(
          MCSymbolRefExpr::create(MO.getMBB()->getSymbol(), OutContext)));
      break;

    case MachineOperand::MO_GlobalAddress:
      // Global variable reference → MCOperand expression (symbol ref)
      TmpInst.addOperand(MCOperand::createExpr(
          MCSymbolRefExpr::create(getSymbol(MO.getGlobal()), OutContext)));
      break;

    default:
      // Other operand types (ConstantPoolIndex, JumpTableIndex, etc.)
      // are not yet handled — they would be needed for more complex code.
      break;
    }
  }

  // Send the completed MCInst to the streamer for output.
  // The streamer calls MYISAInstPrinter::printInst() to render it as text.
  EmitToStreamer(*OutStreamer, TmpInst);
}

//===----------------------------------------------------------------------===//
// PrintAsmOperand — Inline assembly operand printing
//
// When C code uses inline assembly: asm("ADD %0, %1, %2" : "=r"(c) : "r"(a), "r"(b))
// the compiler needs to print each operand (%0, %1, %2) in the target's syntax.
// Returns false on success, true on failure.
//===----------------------------------------------------------------------===//

bool MYISAAsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                                       const char *ExtraCode,
                                       raw_ostream &OS) {
  const MachineOperand &MO = MI->getOperand(OpNo);
  switch (MO.getType()) {
  case MachineOperand::MO_Register:
    OS << MYISAInstPrinter::getRegisterName(MO.getReg());
    return false;  // Success
  case MachineOperand::MO_Immediate:
    OS << MO.getImm();
    return false;
  default:
    return AsmPrinter::PrintAsmOperand(MI, OpNo, ExtraCode, OS);
  }
}

//===----------------------------------------------------------------------===//
// PrintAsmMemoryOperand — Inline assembly memory operand printing
//
// For memory operands in inline assembly (e.g., "m" constraint).
// Prints in the format: [reg + offset]
//===----------------------------------------------------------------------===//

bool MYISAAsmPrinter::PrintAsmMemoryOperand(const MachineInstr *MI,
                                             unsigned OpNo,
                                             const char *ExtraCode,
                                             raw_ostream &OS) {
  const MachineOperand &Base   = MI->getOperand(OpNo);
  const MachineOperand &Offset = MI->getOperand(OpNo + 1);
  OS << "[";
  OS << MYISAInstPrinter::getRegisterName(Base.getReg());
  if (Offset.getImm() != 0)
    OS << " + " << Offset.getImm();
  OS << "]";
  return false;
}

//===----------------------------------------------------------------------===//
// Registration — Tell LLVM to use this AsmPrinter for MYISA
//
// This extern "C" function is called during LLVM startup (alongside
// LLVMInitializeMYISATarget and LLVMInitializeMYISATargetMC).
// RegisterAsmPrinter<T> associates this printer with the target.
//===----------------------------------------------------------------------===//

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeMYISAAsmPrinter() {
  RegisterAsmPrinter<MYISAAsmPrinter> X(getTheMYISATarget());
}
