// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 Analog Devices, Inc.
//===-- MYISAMCTargetDesc.h - MYISA MC Target Description -----*- C++ -*-===//
//
// WHAT THIS FILE IS:
//   The header for the MC (Machine Code) layer of the MYISA backend. Provides
//   forward declarations and factory function prototypes for MC-layer components.
//   Also includes the auto-generated register, instruction, and subtarget
//   enums so that MC-layer code can reference them.
//
// WHY IT MUST EXIST:
//   The MC layer operates below the code generator — it deals with raw
//   instructions, registers, and encoding without any high-level concepts
//   like functions, basic blocks, or virtual registers. Components in this
//   layer need a common header for:
//     - Forward declarations of MC classes
//     - Factory function prototypes (for future MCCodeEmitter, AsmBackend, etc.)
//     - The getTheMYISATarget() accessor
//     - Register/instruction/subtarget enums (included from generated .inc files)
//
// WHAT EACH SECTION DOES:
//   Forward declarations:
//     Declare MC-layer classes that the target might need to create or reference.
//     Even if some are not fully implemented yet (MCCodeEmitter, MCAsmBackend),
//     their declarations exist here for future use.
//
//   getTheMYISATarget():
//     Accessor for the singleton Target object. Same function declared in
//     MYISA.h but re-declared here because MC-layer code should not need
//     to include the full codegen-level header.
//
//   Factory function declarations:
//     createMYISAMCCodeEmitter() — for binary .o output (stub/future)
//     createMYISAAsmBackend() — for assembly/object writing (stub/future)
//     createMYISAELFObjectWriter() — for ELF-specific output (stub/future)
//
//   Auto-generated enums:
//     GET_REGINFO_ENUM: provides MYISA::R0, R1, ..., R31
//     GET_INSTRINFO_ENUM: provides MYISA::ADD_rrr, MYISA::PUSH, etc.
//     GET_SUBTARGETINFO_ENUM: provides MYISA::FeatureNone, etc.
//     These are included here so MC-layer .cpp files can reference registers
//     and instructions by name without including full codegen headers.
//
// WHAT COULD BE ADDED:
//   - createMYISAMCCodeEmitter() implementation (for .o file output)
//   - createMYISAAsmBackend() implementation (for object file writing)
//   - createMYISAELFObjectWriter() implementation (for ELF relocations)
//   - createMYISAAsmParser() declaration (for .s → .o assembly input)
//   - createMYISADisassembler() declaration (for objdump-style output)
//   - MCTargetStreamer subclass declaration (for target-specific directives)
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIB_TARGET_MYISA_MCTARGETDESC_MYISAMCTARGETDESC_H
#define LLVM_LIB_TARGET_MYISA_MCTARGETDESC_MYISAMCTARGETDESC_H

#include "llvm/MC/MCTargetOptions.h"
#include "llvm/Support/DataTypes.h"
#include <memory>

namespace llvm {

// Forward declarations for MC-layer classes
class MCAsmBackend;
class MCCodeEmitter;
class MCContext;
class MCInstrInfo;
class MCObjectTargetWriter;
class MCRegisterInfo;
class MCSubtargetInfo;
class Target;

// Returns the singleton Target object for MYISA.
// Defined in TargetInfo/MYISATargetInfo.cpp.
Target &getTheMYISATarget();

// Factory: creates an MCCodeEmitter for MYISA (binary instruction encoding).
// NOT YET IMPLEMENTED — declared here for future use when binary .o output
// is added. Would encode MCInsts into their 32-bit binary representation.
MCCodeEmitter *createMYISAMCCodeEmitter(const MCInstrInfo &MCII,
                                         MCContext &Ctx);

// Factory: creates an MCAsmBackend for MYISA (object file assembly backend).
// NOT YET IMPLEMENTED — would handle fixups, relocations, and relaxation.
MCAsmBackend *createMYISAAsmBackend(const Target &T,
                                     const MCSubtargetInfo &STI,
                                     const MCRegisterInfo &MRI,
                                     const MCTargetOptions &Options);

// Factory: creates an ELF object writer for MYISA.
// NOT YET IMPLEMENTED — would define ELF relocation types for 16-bit addresses.
std::unique_ptr<MCObjectTargetWriter> createMYISAELFObjectWriter();

} // end namespace llvm

// --- Auto-generated enum definitions ---
// These give MC-layer code access to register, instruction, and subtarget
// feature names without needing to include codegen-level headers.

// Register enum: MYISA::R0, MYISA::R1, ..., MYISA::R31, MYISA::GPR, etc.
#define GET_REGINFO_ENUM
#include "MYISAGenRegisterInfo.inc"

// Instruction enum: MYISA::ADD_rrr, MYISA::SUB_rrr, MYISA::PUSH, etc.
#define GET_INSTRINFO_ENUM
#include "MYISAGenInstrInfo.inc"

// Subtarget feature enum: MYISA::FeatureNone, etc.
#define GET_SUBTARGETINFO_ENUM
#include "MYISAGenSubtargetInfo.inc"

#endif
