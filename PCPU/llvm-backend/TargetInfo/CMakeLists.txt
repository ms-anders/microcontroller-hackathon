# ===-- TargetInfo/CMakeLists.txt - Target registration library build rules --===
#
# WHAT THIS FILE IS:
#   CMake build script for the LLVMMYISAInfo library — the minimal target
#   registration component for MYISA.
#
# WHY IT MUST EXIST:
#   This is the lightest-weight library in the backend. It contains ONLY the
#   target registration code (one function: LLVMInitializeMYISATargetInfo and
#   the Target singleton accessor getTheMYISATarget). Every other MYISA
#   library depends on this one (MYISADesc depends on it, MYISACodeGen
#   depends on it) because they all need to reference the Target singleton.
#
#   This separation exists so that tools which only need to enumerate available
#   targets (e.g., `llc --version`) can link this tiny library without pulling
#   in the entire MC layer or code generator.
#
# WHAT IT BUILDS:
#   LLVMMYISAInfo library containing:
#     - MYISATargetInfo.cpp: Target singleton + LLVMInitializeMYISATargetInfo()
#
# LINK_COMPONENTS:
#   MC: needed for TargetRegistry.h (RegisterTarget<>)
#   Support: LLVM utilities
#
# WHAT COULD BE ADDED:
#   Nothing — this library is intentionally minimal. All additions go into
#   MCTargetDesc (MC-layer components) or the parent directory (CodeGen).
#
# ============================================================================

add_llvm_component_library(LLVMMYISAInfo
  MYISATargetInfo.cpp

  LINK_COMPONENTS
  MC
  Support
)
