// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 Analog Devices, Inc.
//===-- MYISATargetMachine.cpp - MYISA Target Machine --------------------===//
//
// WHAT THIS FILE IS:
//   Implementation of the MYISATargetMachine class — the top-level backend
//   entry point. This file is the first thing LLVM calls when a user compiles
//   for the MYISA target.
//
// WHY IT MUST EXIST:
//   LLVM's target registry system calls LLVMInitializeMYISATarget() at
//   startup (triggered by -march=myisa). This function registers the
//   MYISATargetMachine class as the factory for MYISA compilation. Without
//   this file, LLVM has no TargetMachine to instantiate and cannot compile.
//
// WHAT EACH PART DOES:
//   1. LLVMInitializeMYISATarget(): registration function called by LLVM's
//      initialization infrastructure. Links this TargetMachine to the Target
//      object created in MYISATargetInfo.cpp.
//   2. computeDataLayout(): constructs the LLVM data layout string that
//      describes the target's memory model (endianness, pointer size, alignment).
//   3. MYISATargetMachine constructor: initializes the base LLVMTargetMachine,
//      creates the object file handler (ELF), and initializes the subtarget.
//   4. MYISAPassConfig: defines which passes run during code generation.
//      Currently only adds the instruction selector (ISel) pass.
//   5. createPassConfig(): factory method called by the pass manager.
//
// WHAT COULD BE ADDED:
//   - addPreEmitPass(): for branch relaxation (if branch offsets exceed 16 bits)
//   - addPreRegAlloc(): for custom optimizations before register allocation
//   - addPostRegAlloc(): for post-RA scheduling or peephole optimizations
//   - addPreSched2(): for pre-emission scheduling passes
//   - addMachineLateOptimization(): for late machine-level optimizations
//   - Override getTargetTransformInfo() for cost modeling (affects inlining,
//     vectorization, loop unrolling decisions)
//   - JIT support (override addPassesToEmitMC or addPassesToEmitFile)
//
//===----------------------------------------------------------------------===//

#include "MYISATargetMachine.h"
#include "MYISA.h"
#include "MYISATargetObjectFile.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FormattedStream.h"

using namespace llvm;

// LLVMInitializeMYISATarget — Called once at program startup by LLVM's
// initialization system. The extern "C" and LLVM_EXTERNAL_VISIBILITY ensure
// this symbol is findable by the dynamic linker / static init system.
// RegisterTargetMachine<T> tells LLVM: "when someone asks for the MYISA
// target, construct a MYISATargetMachine."
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeMYISATarget() {
  RegisterTargetMachine<MYISATargetMachine> X(getTheMYISATarget());
}

// computeDataLayout — Produces the LLVM data layout string for MYISA.
// This string is parsed by LLVM's DataLayout class and affects:
//   - How structs are laid out in memory (field alignment, padding)
//   - Pointer arithmetic (pointer width determines address calculations)
//   - ABI compatibility (must match what the linker/runtime expect)
//
// *** ADAPT FOR YOUR ISA: change this string to match your data model.  ***
// Reference: https://llvm.org/docs/LangRef.html#data-layout
//
// Fields in our string "e-p:32:32-i32:32-n32-S32":
//   e       = little endian byte order
//   p:32:32 = pointers are 32 bits wide, 32-bit aligned
//             (physical address bus is 16 bits, but we use 32-bit pointers
//              to match register width and simplify type legalization)
//   i32:32  = 32-bit integers are 32-bit aligned
//   n32     = native integer width is 32 bits (affects type promotion)
//   S32     = stack is 32-bit (4-byte) aligned
static std::string computeDataLayout() {
  // TODO: return the LLVM data-layout string describing your memory model.
  //       Format reference: https://llvm.org/docs/LangRef.html#data-layout
  //       Example (little-endian, 32-bit pointers/ints, 32-bit native+stack):
  //         return "e-p:32:32-i32:32-n32-S32";
  return ""; // TODO: replace with your data-layout string
}

// MYISATargetMachine constructor
// Parameters (all provided by LLVM's driver/command-line parsing):
//   T:       The Target object (singleton from MYISATargetInfo.cpp)
//   TT:      Target triple (e.g., "myisa-unknown-unknown")
//   CPU:     CPU name from -mcpu= (defaults to "generic")
//   FS:      Feature string from -mattr= (e.g., "+hwdiv")
//   Options: Misc target options (e.g., fast-math, frame pointer policy)
//   RM:      Relocation model (Static, PIC, etc.) — we default to Static
//   CM:      Code model (Small, Large, etc.) — we default to Small
//   OL:      Optimization level (None, Less, Default, Aggressive)
//   JIT:     Whether this is for JIT compilation (we don't support JIT)
MYISATargetMachine::MYISATargetMachine(
    const Target &T, const Triple &TT, StringRef CPU, StringRef FS,
    const TargetOptions &Options, std::optional<Reloc::Model> RM,
    std::optional<CodeModel::Model> CM, CodeGenOpt::Level OL, bool JIT)
    : LLVMTargetMachine(T, computeDataLayout(), TT,
                        CPU.empty() ? "generic" : CPU, FS, Options,
                        RM.value_or(Reloc::Static),
                        CM.value_or(CodeModel::Small), OL),
      TLOF(std::make_unique<MYISATargetObjectFile>()),
      Subtarget(TT, CPU.empty() ? std::string("generic") : std::string(CPU),
                std::string(FS), *this) {
  // initAsmInfo() must be called after construction to finalize MC-layer
  // configuration (reads MCAsmInfo, connects register info, etc.).
  initAsmInfo();
}

// MYISAPassConfig — Configures the code generation pass pipeline.
// TargetPassConfig is LLVM's mechanism for backends to customize which
// passes run and in what order. The minimum requirement is addInstSelector()
// which inserts the instruction selection pass.
namespace {
class MYISAPassConfig : public TargetPassConfig {
public:
  MYISAPassConfig(MYISATargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  MYISATargetMachine &getMYISATargetMachine() const {
    return getTM<MYISATargetMachine>();
  }

  // addInstSelector — Adds the instruction selection pass to the pipeline.
  // This is where DAG-based ISel happens (SelectionDAG → MachineInstr).
  // Returns false to indicate success (true would mean "skip default passes").
  bool addInstSelector() override;

  // Additional pass insertion points that could be overridden:
  // void addPreRegAlloc() override;    // Before register allocation
  // void addPostRegAlloc() override;   // After register allocation
  // void addPreSched2() override;      // Before post-RA scheduling
  // void addPreEmitPass() override;    // Before final emission (branch relaxation)
};
} // end anonymous namespace

bool MYISAPassConfig::addInstSelector() {
  // Create and add the MYISA-specific instruction selection pass.
  // This pass (defined in MYISAISelDAGToDAG.cpp) converts the legalized
  // SelectionDAG into MachineInstrs using both TableGen patterns and
  // custom C++ matching logic.
  addPass(createMYISAISelDag(getMYISATargetMachine()));
  return false;
}

// createPassConfig — Factory method called by the pass manager infrastructure.
// Returns a new MYISAPassConfig that will configure the pipeline.
TargetPassConfig *MYISATargetMachine::createPassConfig(PassManagerBase &PM) {
  return new MYISAPassConfig(*this, PM);
}
