// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 Analog Devices, Inc.
//===-- MYISATargetMachine.h - MYISA Target Machine ----------*- C++ -*-===//
//
// WHAT THIS FILE IS:
//   Declares the MYISATargetMachine class — the top-level entry point for
//   the entire MYISA backend. This is the object that LLVM's core creates
//   when a user requests compilation for the "myisa" target.
//
// WHY IT MUST EXIST:
//   Every LLVM backend must provide a TargetMachine subclass. It is the
//   single object that:
//     1. Defines the data layout string (endianness, pointer size, alignment)
//     2. Holds the Subtarget (which in turn holds InstrInfo, RegInfo, etc.)
//     3. Creates the pass pipeline (which passes run during code generation)
//     4. Provides the object file format (ELF, COFF, MachO)
//   Without this class, `llc -march=myisa` and `clang --target=myisa`
//   cannot function.
//
// WHAT EACH PART DOES:
//   - LLVMTargetMachine base: provides the standard LLVM codegen infrastructure
//   - TLOF (TargetLoweringObjectFile): configures ELF section behavior
//   - Subtarget: aggregates all sub-components (instr info, reg info, etc.)
//   - getSubtargetImpl(): returns the subtarget for a given function (allows
//     per-function feature customization in theory)
//   - createPassConfig(): builds the pipeline of codegen passes
//   - getObjFileLowering(): returns the object file format handler
//
// WHAT COULD BE ADDED:
//   - Per-function subtargets (different features for hot vs. cold functions)
//   - Custom pass pipeline additions (pre-emit passes, branch relaxation, etc.)
//   - JIT support (currently disabled — would need JITTargetMachineBuilder)
//   - PostRAScheduler, MachinePipeliner, or other advanced passes
//   - Custom TargetTransformInfo for cost modeling (loop unrolling decisions)
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIB_TARGET_MYISA_MYISATARGETMACHINE_H
#define LLVM_LIB_TARGET_MYISA_MYISATARGETMACHINE_H

#include "MYISASubtarget.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {

class MYISATargetMachine : public LLVMTargetMachine {
  // Object file format handler (ELF sections, symbol visibility, etc.)
  std::unique_ptr<TargetLoweringObjectFile> TLOF;

  // The subtarget holds all target-specific sub-components (InstrInfo,
  // RegisterInfo, FrameLowering, TargetLowering) as member objects.
  MYISASubtarget Subtarget;

public:
  // Constructor: called by LLVM's target registration when the user selects
  // -march=myisa. Parameters come from the command line or driver.
  MYISATargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                      StringRef FS, const TargetOptions &Options,
                      std::optional<Reloc::Model> RM,
                      std::optional<CodeModel::Model> CM,
                      CodeGenOpt::Level OL, bool JIT);

  // Returns the subtarget for a specific function. In a more complex backend,
  // different functions could have different subtargets (e.g., one function
  // uses hardware divide, another doesn't). Here we return the same one always.
  const MYISASubtarget *getSubtargetImpl(const Function &F) const override {
    return &Subtarget;
  }

  // Creates the pass pipeline configuration (which passes run and in what order).
  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;

  // Returns the object file lowering handler (ELF configuration).
  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }
};

} // end namespace llvm

#endif
