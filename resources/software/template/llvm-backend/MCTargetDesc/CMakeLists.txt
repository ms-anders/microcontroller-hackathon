# ===-- MCTargetDesc/CMakeLists.txt - MC-layer library build rules ----------===
#
# WHAT THIS FILE IS:
#   CMake build script for the LLVMMYISADesc library — the MC (Machine Code)
#   layer components for MYISA.
#
# WHY IT MUST EXIST:
#   LLVM separates backends into multiple libraries with different dependency
#   levels. The MC-layer library (LLVMMYISADesc) contains components that
#   tools like objdump, llvm-mc, and the assembler need WITHOUT pulling in
#   the full code generator. This separation means lightweight tools can
#   link just MYISADesc + MYISAInfo without the heavy CodeGen library.
#
# WHAT IT BUILDS:
#   LLVMMYISADesc library containing:
#     - MYISAMCTargetDesc.cpp: MC component registration (instr/reg/subtarget info)
#     - MYISAInstPrinter.cpp: MCInst → assembly text rendering
#
# LINK_COMPONENTS:
#   MC: base MC classes (MCInst, MCStreamer, MCRegisterInfo, etc.)
#   Support: LLVM utilities (StringRef, raw_ostream, etc.)
#   MYISAInfo: the TargetInfo registration library (for getTheMYISATarget)
#
# WHAT COULD BE ADDED:
#   - MYISAMCCodeEmitter.cpp (binary instruction encoding for .o output)
#   - MYISAAsmBackend.cpp (fixups, relocations, object file writing)
#   - MYISAELFObjectWriter.cpp (ELF relocation type definitions)
#   - MYISAAsmParser.cpp (assembly → MCInst parsing, if in this directory)
#   - MYISAMCTargetStreamer.cpp (target-specific assembly directives)
#
# ============================================================================

add_llvm_component_library(LLVMMYISADesc
  MYISAMCTargetDesc.cpp
  MYISAInstPrinter.cpp

  LINK_COMPONENTS
  MC
  Support
  MYISAInfo
)
