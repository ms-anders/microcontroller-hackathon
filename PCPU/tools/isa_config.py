# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 Analog Devices, Inc.
"""
isa_config.py — ISA configuration for the assembler (edit this file).

This is the single source of truth for your instruction encoding. The generic
assembler engine (`asm.py`) imports the tables and encode_*() functions defined
here and needs no changes as your ISA grows — you only edit this file.

>>> The values below are a complete, working EXAMPLE encoding (a 32-bit, two-
>>> format design) so the toolchain runs out of the box. Replace the opcode
>>> tables, register map, and bit-field layouts with YOUR OWN ISA's design. <<<

Steps to adapt it:
  1. Set INSTR_WIDTH / REG_COUNT / REG_ALIASES to your architecture.
  2. Replace T1_OPS / T2_OPS with your mnemonics and opcode numbers.
  3. Update T1_UNARY / BRANCH_OPS / MEM_OPS to match your instruction groups.
  4. Rewrite TYPE1_LAYOUT / TYPE2_LAYOUT with your exact bit-field positions.
  5. Adjust encode_type1() / encode_type2() (or add more encoders) if your
     instruction formats differ from the two shown here.
"""

# ---------------------------------------------------------------------------
# Global ISA shape  — TODO: set for your architecture
# ---------------------------------------------------------------------------
INSTR_WIDTH = 32
REG_COUNT = 32
REG_ALIASES = {
    "pc": 31,
    "zer": 30,
    "sp": 29,
    "lr": 28,
    "arg0": 20,
    "arg1": 21,
    "arg2": 22,
    "arg3": 23,
    "arg4": 24,
    "arg5": 25,
    "arg6": 26,
    "ret": 27,
    "acc": 0
}

# ---------------------------------------------------------------------------
# Opcode tables  — TODO: replace with your mnemonics + opcode values
# ---------------------------------------------------------------------------

# Type 1 opcodes (example: 14-bit opcode, MSB of the word is always 1)
T1_OPS = {
    "ADD":  0b100000,
    "SUB":  0b100001,
    "NEG":  0b110010,
    "AND":  0b100100,
    "OR":   0b100101,
    "XOR":  0b100110,
    "NOT":  0b110111,
    "LSL":  0b101000,
    "LSR":  0b101001,
    "ASR":  0b101011,
    "CMP":  0b101100,
    "MOV":  0b000000,
    "LI2":  0b000001,
    "LD":   0b010000,
    "STO":  0b010001,
    "LDB":  0b010010,
    "STB":  0b010011,
    "POP":  0b010100,
    "PUSH": 0b010101,
    "JMP":  0b001000,
    "JEQ":  0b001010,
    "JNE":  0b001011,
    "JL":   0b001100,
    "JG":   0b001101,
    "JSR":  0b011001,
    "RET":  0b011000
}

# Type 1 ops that only use arg1 (no arg2)
T1_UNARY = {"NOT", "NEG"}

# Type 2 opcodes (example: 9-bit opcode, MSB of the word is always 0)
T2_OPS = {
}

# Control-flow instructions that use branch/call target handling
BRANCH_OPS = {"JMP", "JZ", "JNZ", "JLT", "JGT", "CALL"}

# Memory instructions that use [addr] / [rN + off] operand handling
MEM_OPS = {"LOAD", "STORE", "LOADB", "STOREB"}


# ---------------------------------------------------------------------------
# Encoding layout  — TODO: all bit positions and widths live here
# ---------------------------------------------------------------------------

TYPE1_LAYOUT = {
    "msb": {"bit": 31, "value": 1},
    "opcode": {"lsb": 17, "width": 14},
    "arg1_imm": {"lsb": 16, "width": 1},
    "arg1_val": {"lsb": 11, "width": 5},
    "arg2_imm": {"lsb": 10, "width": 1},
    "arg2_val": {"lsb": 5, "width": 5},
    "result_reg": {"lsb": 0, "width": 5},
}

TYPE2_LAYOUT = {
    "msb": {"bit": 31, "value": 0},
    "opcode": {"lsb": 22, "width": 9},
    "ri": {"lsb": 21, "width": 1},
    "reg": {"lsb": 16, "width": 5},
    "addr": {"lsb": 0, "width": 16},
}


def _mask(width: int) -> int:
    return (1 << width) - 1


TYPE1_IMM_MAX = _mask(TYPE1_LAYOUT["arg1_val"]["width"])
TYPE2_ADDR_MASK = _mask(TYPE2_LAYOUT["addr"]["width"])


def _encode_with_layout(layout: dict, fields: dict) -> int:
    """Generic encoder from a field layout + field values."""
    instr = 0
    if "msb" in layout:
        instr |= (layout["msb"]["value"] & 1) << layout["msb"]["bit"]

    for name, spec in layout.items():
        if name == "msb":
            continue
        value = fields[name]
        instr |= (value & _mask(spec["width"])) << spec["lsb"]

    # Keep instruction width bounded for safety.
    return instr & _mask(INSTR_WIDTH)


def encode_type1(opcode, arg1_imm, arg1_val, arg2_imm, arg2_val, result_reg):
    return _encode_with_layout(
        TYPE1_LAYOUT,
        {
            "opcode": opcode,
            "arg1_imm": arg1_imm,
            "arg1_val": arg1_val,
            "arg2_imm": arg2_imm,
            "arg2_val": arg2_val,
            "result_reg": result_reg,
        },
    )


def encode_type2(opcode, ri, reg, addr):
    return _encode_with_layout(
        TYPE2_LAYOUT,
        {
            "opcode": opcode,
            "ri": ri,
            "reg": reg,
            "addr": addr,
        },
    )
