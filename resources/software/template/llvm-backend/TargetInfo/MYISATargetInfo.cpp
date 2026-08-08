// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 Analog Devices, Inc.
//===-- MYISATargetInfo.cpp - MYISA Target Registration ------------------===//
//
// WHAT THIS FILE IS:
//   Registers "myisa" as a known target triple with LLVM's target registry.
//   This is the very first piece of code that runs for the MYISA backend —
//   it creates the Target singleton object and tells LLVM "a target called
//   'myisa' exists."
//
// WHY IT MUST EXIST:
//   LLVM discovers available targets through a registry system. When a user
//   passes -march=myisa or --target=myisa, LLVM looks up that name in the
//   registry. If no target is registered under that name, compilation fails
//   with "unknown target." This file provides that registration.
//
//   This is intentionally the smallest possible file — it lives in its own
//   library (LLVMMYISAInfo) so that lightweight tools (like llvm-objdump)
//   can link against it without pulling in the full code generator.
//
// WHAT EACH PART DOES:
//   getTheMYISATarget():
//     Returns a reference to the singleton Target object. This is a function-
//     local static — created once on first call, lives for program lifetime.
//     Every other part of the backend (TargetMachine, AsmPrinter, MCTargetDesc)
//     references this same object to attach its components.
//
//   LLVMInitializeMYISATargetInfo():
//     Registration function called during LLVM startup. RegisterTarget<> tells
//     the registry: "there exists a target named 'myisa' with description
//     'My Instruction Set Architecture', and here is its Target object."
//     Parameters:
//       Triple::UnknownArch — the architecture enum (we don't have a dedicated
//         enum value in LLVM's Triple class; UnknownArch is fine for out-of-tree)
//       HasJIT=false — we don't support JIT compilation
//       "myisa" — the target name (used by -march=myisa)
//       "My Instruction Set Architecture" — human-readable description
//       "MYISA" — the "backend name" for --print-targets output
//
// WHAT COULD BE ADDED:
//   - A proper Triple::ArchType enum value (requires patching LLVM's Triple.h
//     for upstream submission; out-of-tree backends use UnknownArch)
//   - Multiple registered names (aliases) for the target
//   - HasJIT=true if JIT support is implemented
//
// *** ADAPT FOR YOUR ISA: change the second arg ("myisa") to your       ***
// *** target's march name (what you pass to -march= / --target=).        ***
// *** Change the third arg to a human-readable description.              ***
//===----------------------------------------------------------------------===//

#include "MYISA.h"
#include "llvm/MC/TargetRegistry.h"

namespace llvm {

// getTheMYISATarget — Singleton accessor for the MYISA Target object.
// The Target object is the root of the registration tree — all other
// components (TargetMachine, AsmPrinter, MCDesc) attach to it.
Target &getTheMYISATarget() {
  static Target TheMYISATarget;
  return TheMYISATarget;
}

} // namespace llvm

// LLVMInitializeMYISATargetInfo — Called once during LLVM startup.
// Registers the target name "myisa" with the global target registry.
// After this call, LLVM can resolve -march=myisa to this target.
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeMYISATargetInfo() {
  // TODO: change "myisa" to the -march / --target name for your ISA, and the
  //       following string to a human-readable description of your target.
  llvm::RegisterTarget<llvm::Triple::UnknownArch, /*HasJIT=*/false>
      X(llvm::getTheMYISATarget(), "myisa", "My Instruction Set Architecture",
        "MYISA");
}
