// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 Analog Devices, Inc.
//===-- MYISAInstPrinter.cpp - MYISA MCInst to assembly ------------------===//
//
// WHAT THIS FILE IS:
//   Implements the MYISAInstPrinter class — the final step that converts
//   MCInst objects into the assembly text strings that appear in .s files.
//   This is a thin implementation because most of the work is done by the
//   auto-generated printInstruction() from TableGen.
//
// WHY IT MUST EXIST:
//   The auto-generated printInstruction() handles instruction mnemonics and
//   operand templates, but it calls back into user-defined methods for each
//   operand type. This file provides those callback implementations:
//     - printOperand(): how to render a register, immediate, or symbol
//     - printMemOperand(): how to render memory addressing syntax
//     - printInst(): the top-level entry point
//
// WHAT EACH FUNCTION DOES:
//   printInst():
//     Entry point called by the MCStreamer. Calls the auto-generated
//     printInstruction() (from MYISAGenAsmWriter.inc) which handles the
//     AsmString template, then prints any annotation (e.g., from debug info).
//
//   printOperand() [3-arg]:
//     Renders a single MCOperand according to its type:
//       - Register: print the assembly name (e.g., "r8")
//       - Immediate: print as decimal number
//       - Expression: print the MCExpr (symbol name, for branches/globals)
//
//   printOperand() [4-arg]:
//     Called by the generated code for PC-relative operands (takes the
//     instruction address as context). Currently delegates to the 3-arg
//     version without PC adjustment.
//
//   printMemOperand():
//     Renders a memory operand as "[base + offset]" or "[base]" if offset=0.
//     Called when the generated printInstruction encounters a memsrc operand
//     (because memsrc has PrintMethod="printMemOperand" in the .td file).
//
// WHAT COULD BE ADDED:
//   - printBranchTarget(): render branch targets as label names + offset
//   - printHexImmediate(): print immediates in hex format (0xFF)
//   - Instruction aliases: detect NOP encoding (ADD r0, r0, r0) and print "NOP"
//   - Custom annotation printing for profiling/coverage info
//   - Multiple syntax variant support (if the assembler has alternate syntax)
//
//===----------------------------------------------------------------------===//

#include "MYISAInstPrinter.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCRegister.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

// Include the TableGen-generated printing functions:
//   - printInstruction(): the main printer (generated from AsmString fields)
//   - getRegisterName(): maps register enum → string (from Register<n> defs)
//   - getMnemonic(): returns the mnemonic for an opcode
#include "MYISAGenAsmWriter.inc"

//===----------------------------------------------------------------------===//
// printInst — Top-level entry point for instruction printing
//
// Called by the MCStreamer for each instruction to emit. Delegates to the
// auto-generated printInstruction() which uses the AsmString templates from
// MYISAInstrInfo.td to format the output. Then appends any annotation
// (debug comments, etc.) via printAnnotation().
//===----------------------------------------------------------------------===//

void MYISAInstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                  StringRef Annot,
                                  const MCSubtargetInfo &STI,
                                  raw_ostream &O) {
  printInstruction(MI, Address, O);
  printAnnotation(O, Annot);
}

//===----------------------------------------------------------------------===//
// printOperand — Render a single operand as assembly text (3-arg version)
//
// This is called by the generated printInstruction() for each $operand
// placeholder in the AsmString. The operand type determines rendering:
//   Register: use the auto-generated getRegisterName() for the name
//   Immediate: print as decimal integer
//   Expression: call MCExpr::print() for symbol names and arithmetic
//===----------------------------------------------------------------------===//

void MYISAInstPrinter::printOperand(const MCInst *MI, unsigned OpNo,
                                     raw_ostream &O) {
  const MCOperand &MO = MI->getOperand(OpNo);
  if (MO.isReg()) {
    // Register operand → assembly register name (e.g., "r8", "r30")
    O << getRegisterName(MCRegister(MO.getReg()));
  } else if (MO.isImm()) {
    // Immediate operand → decimal number (e.g., "42", "-1")
    O << MO.getImm();
  } else if (MO.isExpr()) {
    // Expression operand → symbol name (e.g., "main", ".LBB0_1")
    // MCExpr handles the rendering (symbol refs, arithmetic, etc.)
    MO.getExpr()->print(O, &MAI);
  }
}

//===----------------------------------------------------------------------===//
// printOperand — 4-arg version for PC-relative operands
//
// The generated asm-writer calls this variant for operands that have
// OPERAND_PCREL type (like brtarget16). The Address parameter is the
// instruction's address, allowing PC-relative display. Currently we just
// delegate to the 3-arg version (print the target label as-is).
//===----------------------------------------------------------------------===//

void MYISAInstPrinter::printOperand(const MCInst *MI, uint64_t Address,
                                     unsigned OpNo, raw_ostream &O) {
  printOperand(MI, OpNo, O);
}

//===----------------------------------------------------------------------===//
// printMemOperand — Render a composite memory operand
//
// Memory instructions use the "memsrc" operand type which expands to two
// MCInst operands: a base register and an offset immediate. This function
// renders them in the format the MYISA assembler expects:
//   [r8 + 4]   (base=r8, offset=4)
//   [r2]       (base=r2, offset=0 — omitted for readability)
//===----------------------------------------------------------------------===//

void MYISAInstPrinter::printMemOperand(const MCInst *MI, int OpNum,
                                        raw_ostream &O) {
  O << "[";
  printOperand(MI, OpNum, O);  // Print base register
  const MCOperand &Offset = MI->getOperand(OpNum + 1);
  if (Offset.getImm() != 0)
    O << " + " << Offset.getImm();  // Print offset only if non-zero
  O << "]";
}
