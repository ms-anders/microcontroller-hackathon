// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 Analog Devices, Inc.
//===-- MYISATargetObjectFile.h - MYISA Object File Info ------*- C++ -*-===//
//
// WHAT THIS FILE IS:
//   Declares the MYISATargetObjectFile class — configures the object file
//   format (ELF) for the MYISA target. This controls how compiled code is
//   organized into sections (.text, .data, .bss, .rodata) in the output file.
//
// WHY IT MUST EXIST:
//   LLVM's code generator needs to know which object file format to use and
//   how to organize code and data into sections. The TargetMachine holds a
//   pointer to this object (TLOF), and the AsmPrinter uses it to determine
//   which section each global variable or function should be placed in.
//   Without this, LLVM cannot emit properly-structured object files.
//
// WHAT IT DOES:
//   Currently, this is a minimal implementation that inherits all default
//   behavior from TargetLoweringObjectFileELF. The base class provides:
//     - Standard ELF section selection (.text for code, .data for initialized
//       data, .bss for uninitialized data, .rodata for constants)
//     - Symbol visibility and binding rules
//     - Section flags (SHF_ALLOC, SHF_WRITE, SHF_EXECINSTR)
//     - Thread-local storage section handling
//
// WHAT COULD BE ADDED:
//   - Override getSectionForConstant() to place constants in a custom section
//   - Override SelectSectionForGlobal() for custom section placement rules
//   - Override getExplicitSectionGlobal() for attribute-based section assignment
//   - Custom section names for the 16-bit address space (e.g., .near for
//     frequently-accessed data in the low 256 bytes)
//   - Memory-mapped I/O section definitions (.mmio)
//   - Interrupt vector table section (.vectors)
//   - Support for COFF or custom object file formats instead of ELF
//   - Position-independent code (PIC) data section handling
//   - Small data section (.sdata) for GP-relative addressing optimization
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIB_TARGET_MYISA_MYISATARGETOBJECTFILE_H
#define LLVM_LIB_TARGET_MYISA_MYISATARGETOBJECTFILE_H

#include "llvm/CodeGen/TargetLoweringObjectFileImpl.h"

namespace llvm {

// MYISATargetObjectFile — ELF object file handler for MYISA.
// Inherits all standard ELF behavior. Override methods here to customize
// section selection, symbol handling, or relocation behavior.
class MYISATargetObjectFile : public TargetLoweringObjectFileELF {
public:
  MYISATargetObjectFile() = default;
};

} // end namespace llvm

#endif
