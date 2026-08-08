// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 Analog Devices, Inc.
//===-- MYISAMCTargetDesc.cpp - MYISA MC Target Description --------------===//
//
// WHAT THIS FILE IS:
//   Implements the MC (Machine Code) layer registration for MYISA. The MC
//   layer is the lowest abstraction in LLVM — it deals with raw instructions,
//   registers, and encoding without any concept of functions, control flow,
//   or optimization. This file registers all MC-layer components with LLVM's
//   target registry.
//
// WHY IT MUST EXIST:
//   LLVM's MC layer is used by:
//     - The AsmPrinter (to emit assembly text)
//     - The assembler (to parse .s files → .o, if implemented)
//     - The disassembler (to decode binary → assembly, if implemented)
//     - llc/clang (to know what registers, instructions, and features exist)
//   Without registering these components, even assembly text output would fail
//   because the MCStreamer wouldn't know how to print register names or format
//   instruction operands.
//
// WHAT EACH PART DOES:
//   createMYISAMCInstrInfo():
//     Creates and initializes an MCInstrInfo object containing instruction
//     descriptors (opcode names, operand counts, flags). Used by the
//     InstPrinter and any tool that needs instruction metadata.
//
//   createMYISAMCRegisterInfo():
//     Creates and initializes an MCRegisterInfo object with register names,
//     encodings, and class membership. The return address register (R3) is
//     passed for DWARF unwinding.
//
//   createMYISAMCSubtargetInfo():
//     Creates an MCSubtargetInfo with feature flag definitions. Allows tools
//     to query which features are enabled for a given CPU.
//
//   createMYISAMCAsmInfo():
//     Creates the MYISAMCAsmInfo object (assembly syntax configuration).
//
//   createMYISAMCInstPrinter():
//     Creates the MYISAInstPrinter (renders MCInst as assembly text).
//
//   LLVMInitializeMYISATargetMC():
//     The registration function — called during LLVM startup to connect all
//     MC-layer components to the MYISA target. Uses TargetRegistry::Register*
//     functions to associate each factory with the target.
//
// WHAT COULD BE ADDED:
//   - TargetRegistry::RegisterMCCodeEmitter (for binary .o output)
//   - TargetRegistry::RegisterMCAsmBackend (for object file writing)
//   - TargetRegistry::RegisterMCObjectFileInfo (for ELF section metadata)
//   - TargetRegistry::RegisterAsmParser (for .s → .o assembly input)
//   - TargetRegistry::RegisterMCDisassembler (for binary → assembly)
//   - TargetRegistry::RegisterObjectTargetStreamer (for ELF emission)
//   - TargetRegistry::RegisterAsmTargetStreamer (for assembly directives)
//
//===----------------------------------------------------------------------===//

#include "MYISAMCTargetDesc.h"
#include "MYISAInstPrinter.h"
#include "MYISAMCAsmInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

// Include auto-generated MC descriptor tables.
// These provide the raw data arrays that InitMYISA*() functions populate
// into the Info objects (instruction names, register names, feature tables).

// Instruction descriptor arrays (opcode→name mapping, operand info, etc.)
#define GET_INSTRINFO_MC_DESC
#define GET_INSTRINFO_MC_HELPERS
#include "MYISAGenInstrInfo.inc"

// Register descriptor arrays (register→name mapping, encoding, classes)
#define GET_REGINFO_MC_DESC
#include "MYISAGenRegisterInfo.inc"

// Subtarget feature descriptor arrays (feature names, implications)
#define GET_SUBTARGETINFO_MC_DESC
#include "MYISAGenSubtargetInfo.inc"

// --- Factory functions ---

// Creates MCInstrInfo populated with MYISA instruction descriptors.
// MCInstrInfo provides: getInstrName(opcode), getOperandInfo(opcode), etc.
static MCInstrInfo *createMYISAMCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitMYISAMCInstrInfo(X);  // Generated function — fills in descriptor tables
  return X;
}

// Creates MCRegisterInfo populated with MYISA register descriptors.
// MCRegisterInfo provides: getRegName(reg), getRegClass(idx), getEncoding(reg), etc.
// The second arg is the return address register (R3) — used for DWARF unwinding.
static MCRegisterInfo *createMYISAMCRegisterInfo(const Triple &TT) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitMYISAMCRegisterInfo(X, MYISA::R3 /* return address reg */);
  return X;
}

// Creates MCSubtargetInfo populated with MYISA feature and scheduling data.
// MCSubtargetInfo provides: hasFeature(feat), getSchedModel(), etc.
static MCSubtargetInfo *
createMYISAMCSubtargetInfo(const Triple &TT, StringRef CPU, StringRef FS) {
  return createMYISAMCSubtargetInfoImpl(TT, CPU, /*TuneCPU=*/CPU, FS);
}

// Creates the MCAsmInfo object (assembly syntax rules).
// See MYISAMCAsmInfo.h for what gets configured.
static MCAsmInfo *createMYISAMCAsmInfo(const MCRegisterInfo &MRI,
                                       const Triple &TT,
                                       const MCTargetOptions &Options) {
  return new MYISAMCAsmInfo(TT);
}

// Creates the MCInstPrinter (renders MCInst to assembly text).
// SyntaxVariant 0 is the default (only) assembly syntax for MYISA.
static MCInstPrinter *createMYISAMCInstPrinter(const Triple &TT,
                                                unsigned SyntaxVariant,
                                                const MCAsmInfo &MAI,
                                                const MCInstrInfo &MII,
                                                const MCRegisterInfo &MRI) {
  return new MYISAInstPrinter(MAI, MII, MRI);
}

//===----------------------------------------------------------------------===//
// LLVMInitializeMYISATargetMC — MC-layer registration
//
// This function is called once during LLVM startup (triggered by
// -march=myisa or InitializeAllTargetMCs()). It associates each factory
// function with the MYISA Target object, so LLVM knows how to create
// MC-layer components for this target.
//
// The registration order doesn't matter — LLVM queries components lazily.
//===----------------------------------------------------------------------===//

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeMYISATargetMC() {
  Target &T = getTheMYISATarget();

  // Register assembly syntax configuration
  TargetRegistry::RegisterMCAsmInfo(T, createMYISAMCAsmInfo);

  // Register instruction metadata
  TargetRegistry::RegisterMCInstrInfo(T, createMYISAMCInstrInfo);

  // Register register file metadata
  TargetRegistry::RegisterMCRegInfo(T, createMYISAMCRegisterInfo);

  // Register subtarget features and scheduling info
  TargetRegistry::RegisterMCSubtargetInfo(T, createMYISAMCSubtargetInfo);

  // Register the assembly text printer
  TargetRegistry::RegisterMCInstPrinter(T, createMYISAMCInstPrinter);

  // Future registrations (when implemented):
  // TargetRegistry::RegisterMCCodeEmitter(T, createMYISAMCCodeEmitter);
  // TargetRegistry::RegisterMCAsmBackend(T, createMYISAAsmBackend);
}
