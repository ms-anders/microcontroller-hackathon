// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Copyright (c) 2026 Analog Devices, Inc.
//===-- MYISASubtarget.cpp - MYISA Subtarget ----------------------------===//
//
// WHAT THIS FILE IS:
//   Implementation of the MYISASubtarget class. This is a small file because
//   most of the Subtarget's functionality is either inline (in the header) or
//   auto-generated (from TableGen).
//
// WHY IT MUST EXIST:
//   The constructor must be defined in a .cpp file (not inline in the header)
//   because it calls ParseSubtargetFeatures(), which is defined in the
//   auto-generated MYISAGenSubtargetInfo.inc. That .inc file can only be
//   included once with the _TARGET_DESC and _CTOR defines, so it must live
//   in exactly one translation unit.
//
// WHAT IT DOES:
//   1. Includes the TableGen-generated subtarget implementation code
//      (GET_SUBTARGETINFO_TARGET_DESC provides scheduling tables;
//       GET_SUBTARGETINFO_CTOR provides the constructor body)
//   2. Constructs the Subtarget by:
//      a. Initializing the base class MYISAGenSubtargetInfo with the triple,
//         CPU, and feature string
//      b. Constructing each sub-component (InstrInfo, RegInfo, etc.)
//      c. Calling ParseSubtargetFeatures() to parse -mattr= into booleans
//
// WHAT COULD BE ADDED:
//   - Feature-dependent initialization (e.g., if HasHWDiv is set, configure
//     TLInfo differently — mark ISD::SDIV as Legal instead of Expand)
//   - Multiple constructor variants for different build configurations
//   - Override resolveSchedClass() for dynamic scheduling decisions
//
//===----------------------------------------------------------------------===//

#include "MYISASubtarget.h"
#include "MYISA.h"

#define DEBUG_TYPE "myisa-subtarget"

// Pull in the auto-generated subtarget implementation:
// - Scheduling model tables (empty since we use NoSchedModel)
// - Feature bit arrays
// - The getSubtargetImpl() dispatcher
#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "MYISAGenSubtargetInfo.inc"

using namespace llvm;

// MYISASubtarget constructor
// Initializes all sub-components in member-initializer order:
//   1. MYISAGenSubtargetInfo — base class (generated), needs triple/CPU/FS
//   2. InstrInfo — default-constructed (no parameters needed)
//   3. RegInfo — default-constructed
//   4. FrameLowering — default-constructed (stack grows down, 4-byte aligned)
//   5. TLInfo — needs the TargetMachine and this Subtarget (for register classes)
// After construction, ParseSubtargetFeatures() processes the feature string
// to set boolean flags (currently only HasNone).
MYISASubtarget::MYISASubtarget(const Triple &TT, const std::string &CPU,
                                 const std::string &FS,
                                 const TargetMachine &TM)
    : MYISAGenSubtargetInfo(TT, CPU, /*TuneCPU=*/CPU, FS),
      InstrInfo(), RegInfo(), FrameLowering(),
      TLInfo(TM, *this) {
  ParseSubtargetFeatures(CPU, CPU, FS);
}
