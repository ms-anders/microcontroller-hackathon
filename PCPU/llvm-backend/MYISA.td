// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 Analog Devices, Inc.
//===-- MYISA.td - Top-level TableGen file ---------------*- tablegen -*-===//
//
// WHAT THIS FILE IS:
//   This is the top-level TableGen description for the MYISA backend.
//   It serves as the single entry point that the `tablegen()` CMake command
//   reads — all other .td files are pulled in via `include` directives.
//
// WHY IT MUST EXIST:
//   LLVM's TableGen tool needs exactly one root .td file per target. The
//   CMakeLists.txt sets `LLVM_TARGET_DEFINITIONS` to this file, and all
//   -gen-register-info, -gen-instr-info, etc. invocations start here.
//   Without this file, TableGen has no entry point and cannot generate the
//   .inc files that the C++ sources depend on.
//
// WHAT IT DOES:
//   1. Includes LLVM's base Target.td (defines Register, Instruction,
//      Target, ProcessorModel, etc. — the "standard library" for backends).
//   2. Declares subtarget features (extensibility hooks for future variants).
//   3. Declares a processor model (scheduling model — currently none).
//   4. Includes the register, instruction, and calling convention .td files.
//   5. Declares the top-level Target record that ties everything together.
//
// WHAT COULD BE ADDED:
//   - Additional SubtargetFeature defs for ISA extensions (e.g., hardware
//     multiply-accumulate, SIMD, hardware divide, floating point).
//   - Multiple ProcessorModel defs for different microarchitectures with
//     distinct scheduling characteristics (latency, throughput, issue width).
//   - An AsmParser definition (def MYISAAsmParser : AsmParser) if you
//     implement assembly input (.s → .o) support.
//   - A Disassembler definition for objdump-style binary → assembly.
//   - include "MYISASchedule.td" for a SchedMachineModel.
//
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

//===----------------------------------------------------------------------===//
// MYISA subtarget features (none yet — placeholder for extensions)
//
// SubtargetFeature records let users pass -mattr=+featurename to enable
// optional hardware capabilities. The backend can then check these flags
// at compile time to select different instruction sequences.
//
// Example future features:
//   def FeatureHWDiv : SubtargetFeature<"hwdiv", "HasHWDiv", "true",
//                                        "Hardware integer divider">;
//   def FeatureMAC   : SubtargetFeature<"mac", "HasMAC", "true",
//                                        "Hardware multiply-accumulate">;
//===----------------------------------------------------------------------===//

def FeatureNone : SubtargetFeature<"none", "HasNone", "true",
                                    "No special features">;

//===----------------------------------------------------------------------===//
// MYISA processor models
//
// ProcessorModel ties a CPU name (used with -mcpu=) to a scheduling model
// and a set of enabled features. "generic" is the default when no -mcpu
// is specified. NoSchedModel means no instruction scheduling reordering
// is performed (instructions are emitted in DAG order).
//
// Future additions:
//   def : ProcessorModel<"myisa-v2", MYISAV2SchedModel, [FeatureHWDiv]>;
//===----------------------------------------------------------------------===//

def : ProcessorModel<"generic", NoSchedModel, []>;

//===----------------------------------------------------------------------===//
// Include register, instruction, and calling convention definitions
//
// Order matters: registers must be defined before instructions (since
// instructions reference register classes), and instructions before the
// calling convention (since CC references register names).
//===----------------------------------------------------------------------===//

include "MYISARegisterInfo.td"
include "MYISAInstrInfo.td"
include "MYISACallingConv.td"

//===----------------------------------------------------------------------===//
// Target declaration
//
// The Target record is the capstone — it tells LLVM "this target has this
// instruction set and these assembly writers." LLVM uses this to wire up
// the generated code (register info, instruction info, etc.) to the target
// machine infrastructure.
//
// InstructionSet: points to the InstrInfo record that collects all
//   instruction definitions (auto-gathered from all Instruction subclasses).
//
// AssemblyWriters: list of AsmWriter records. Each one generates a separate
//   printInstruction() function. Most targets have exactly one.
//===----------------------------------------------------------------------===//

def MYISAInstrInfo : InstrInfo;

def MYISAAsmWriter : AsmWriter;

def MYISA : Target {
  let InstructionSet = MYISAInstrInfo;
  let AssemblyWriters = [MYISAAsmWriter];
}
