// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 Analog Devices, Inc.
//===-- MYISA.h - Top-level MYISA target header ---------------*- C++ -*-===//
//
// WHAT THIS FILE IS:
//   The top-level header for the MYISA backend. Provides forward declarations
//   and factory function prototypes that are shared across multiple backend
//   source files.
//
// WHY IT MUST EXIST:
//   LLVM backends need a central header that all source files can include to
//   access shared declarations without creating circular #include dependencies.
//   This file declares:
//     1. The ISel pass factory function (createMYISAISelDag) — needed by
//        MYISATargetMachine.cpp to register the instruction selection pass.
//     2. The target registration accessor (getTheMYISATarget) — needed by
//        TargetInfo, MCTargetDesc, and AsmPrinter to reference the same
//        Target object during initialization.
//
// WHAT COULD BE ADDED:
//   - Factory functions for additional passes (e.g., createMYISAPreEmitPass,
//     createMYISABranchRelaxation, createMYISAExpandPseudo).
//   - Forward declarations for other backend components.
//   - Utility functions shared across multiple .cpp files (e.g., helper to
//     check if a constant fits in N bits).
//   - Namespace-scoped enums for condition codes or other shared constants.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIB_TARGET_MYISA_MYISA_H
#define LLVM_LIB_TARGET_MYISA_MYISA_H

#include "llvm/Target/TargetMachine.h"

namespace llvm {

class MYISATargetMachine;
class FunctionPass;

// Factory function to create the MYISA instruction selection pass.
// Called by MYISAPassConfig::addInstSelector() in MYISATargetMachine.cpp.
// Defined in MYISAISelDAGToDAG.cpp.
FunctionPass *createMYISAISelDag(MYISATargetMachine &TM);

// Returns a reference to the singleton Target object for MYISA.
// Every LLVM target has exactly one Target instance, created in TargetInfo
// (MYISATargetInfo.cpp). All registration functions (TargetMachine, AsmPrinter,
// MCTargetDesc) reference this same object to attach themselves to it.
Target &getTheMYISATarget();

} // end namespace llvm

#endif
