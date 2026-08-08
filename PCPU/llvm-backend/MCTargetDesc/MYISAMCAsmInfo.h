// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 Analog Devices, Inc.
//===-- MYISAMCAsmInfo.h - MYISA Asm Info --------------------*- C++ -*-===//
//
// WHAT THIS FILE IS:
//   Configures the assembly language syntax for the MYISA target. MCAsmInfo
//   is the MC-layer class that tells the assembly printer (and parser) the
//   syntactic conventions of the target's assembly language: comment characters,
//   data directives, alignment rules, etc.
//
// WHY IT MUST EXIST:
//   When LLVM emits assembly text, it needs to know target-specific syntax:
//     - How to write comments (";")
//     - How to emit data values (.word, .half, .byte)
//     - How to emit zero-fills (.zero)
//     - Whether the target is little-endian or big-endian
//     - How wide a code pointer is (for label arithmetic)
//   Without this, the assembly output would use incorrect directives that
//   the MYISA assembler wouldn't understand.
//
// WHAT EACH FIELD DOES:
//   CommentString = ";"
//     Character that starts a comment (rest of line is ignored by assembler).
//     MYISA uses semicolons; ARM uses @, x86 uses # or //.
//
//   SupportsDebugInformation = true
//     Enables DWARF debug info emission (even though we don't fully support
//     it yet, this prevents crashes when -g is passed to the compiler).
//
//   Data32bitsDirective = "\t.word\t"
//     Directive to emit a 32-bit value. Used for global variable initializers,
//     constant pools, and jump tables.
//
//   Data16bitsDirective = "\t.half\t"
//     Directive to emit a 16-bit value.
//
//   Data8bitsDirective = "\t.byte\t"
//     Directive to emit an 8-bit value.
//
//   ZeroDirective = "\t.zero\t"
//     Directive to emit N bytes of zeros (for .bss-like sections in .data).
//
//   IsLittleEndian = true
//     Byte order for multi-byte data directives. MYISA is little-endian.
//
//   UsesELFSectionDirectiveForBSS = true
//     Use .section .bss instead of .lcomm for BSS data.
//
//   HasDotTypeDotSizeDirective = true
//     Emit .type and .size directives for ELF symbol table entries.
//
//   AlignmentIsInBytes = false
//     .align directive specifies power-of-2, not byte count.
//     (e.g., .align 2 means 4-byte alignment, not 2-byte)
//
//   CalleeSaveStackSlotSize = 4
//     Size of each callee-save slot on the stack (4 bytes = one register).
//
//   CodePointerSize = 2
//     Code pointers (function addresses) are 16 bits wide. This matches
//     the 16-bit address space. Affects label-difference calculations.
//
// WHAT COULD BE ADDED:
//   - InlineAsmStart/InlineAsmEnd: strings emitted around inline asm blocks
//   - GlobalDirective: how to declare a global symbol (.global)
//   - AsciiDirective/AscizDirective: how to emit string literals
//   - SeparatorString: instruction separator for multi-instruction lines
//   - LabelSuffix: character after labels (usually ":")
//   - UseAssignmentForEHBegin: for exception handling support
//   - PrivateGlobalPrefix: prefix for compiler-generated labels (e.g., ".L")
//   - PrivateLabelPrefix: prefix for basic block labels
//   - Override for getExprForFDESymbol() for custom debug frame handling
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIB_TARGET_MYISA_MCTARGETDESC_MYISAMCASMINFO_H
#define LLVM_LIB_TARGET_MYISA_MCTARGETDESC_MYISAMCASMINFO_H

#include "llvm/MC/MCAsmInfoELF.h"

namespace llvm {

class Triple;

// MYISAMCAsmInfo — Assembly syntax configuration for MYISA.
// Inherits from MCAsmInfoELF which provides ELF-specific defaults.
// The constructor sets MYISA-specific overrides.
class MYISAMCAsmInfo : public MCAsmInfoELF {
public:
  explicit MYISAMCAsmInfo(const Triple &TT) {
    // Comment syntax: semicolons start comments (matches MYISA assembler)
    CommentString = ";";

    // Enable debug information generation (DWARF)
    SupportsDebugInformation = true;

    // Data emission directives (must match what the MYISA assembler accepts)
    Data32bitsDirective = "\t.word\t";     // 32-bit values
    Data16bitsDirective = "\t.half\t";     // 16-bit values
    Data8bitsDirective = "\t.byte\t";      // 8-bit values
    ZeroDirective = "\t.zero\t";           // Zero-fill N bytes

    // Byte order
    IsLittleEndian = true;

    // ELF section handling
    UsesELFSectionDirectiveForBSS = true;  // Use .section .bss not .lcomm
    HasDotTypeDotSizeDirective = true;     // Emit .type/.size for symbols

    // Alignment directive interpretation
    AlignmentIsInBytes = false;            // .align N means 2^N bytes

    // Stack slot size for callee-saved registers (one 32-bit register = 4 bytes)
    CalleeSaveStackSlotSize = 4;

    // Code pointer width: 16 bits (matching the 16-bit address space)
    // This affects label arithmetic and PC-relative calculations.
    CodePointerSize = 2;
  }
};

} // end namespace llvm

#endif
