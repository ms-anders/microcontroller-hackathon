#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 Analog Devices, Inc.
"""
asm.py — Simple two-pass assembler template.

Reads a .asm file and produces a .hex file suitable for $readmemh in Verilog.

This is the **generic** half of the assembler — it contains no ISA-specific
knowledge. All opcodes, register names, encoding layouts, and the encode_*()
functions live in `isa_config.py`, which you edit for your own ISA. You should
not normally need to change this file.

Usage:
    python3 asm.py input.asm -o output.hex

Assembly syntax (the example config supports these forms):
    ; comment
    label:
    ADD  rd, rs1, rs2          ; Type 1 reg-reg
    ADD  rd, rs1, #imm         ; Type 1 reg-imm
    NOT  rd, rs1               ; Type 1 single-operand
    LUI  rd, #imm              ; Load upper immediate
    LI   rd, #imm16            ; Load 16-bit immediate (zero-extended)
    LOAD rd, [addr]            ; Type 2 absolute
    STORE rs, [addr]           ; Type 2 absolute
    JMP  label                 ; PC-relative branch
    JMP  #addr                 ; Absolute branch
    CALL label                 ; PC-relative call
    RET                        ; Return
    PUSH rs                    ; Push register
    POP  rd                    ; Pop register
    NOP                        ; No operation
    HALT                       ; JMP to self (pseudo)
"""

import sys
import re
import argparse
from isa_config import (
    BRANCH_OPS,
    MEM_OPS,
    REG_ALIASES,
    REG_COUNT,
    TYPE1_IMM_MAX,
    T1_OPS,
    T1_UNARY,
    TYPE2_ADDR_MASK,
    T2_OPS,
    encode_type1,
    encode_type2,
)

def parse_reg(s):
    """Parse register name, return register number in ISA range."""
    s = s.strip().lower()
    if s in REG_ALIASES:
        return REG_ALIASES[s]
    m = re.match(r'^r(\d+)$', s)
    if not m:
        raise ValueError(f"Invalid register: {s}")
    n = int(m.group(1))
    if n < 0 or n >= REG_COUNT:
        raise ValueError(f"Register out of range: {s}")
    return n

def parse_imm(s):
    """Parse immediate value (decimal or hex with 0x prefix)."""
    s = s.strip()
    if s.startswith('#'):
        s = s[1:]
    if s.startswith('0x') or s.startswith('0X'):
        return int(s, 16)
    if s.startswith('-'):
        return int(s)
    return int(s)

def assemble(lines):
    """Two-pass assembler. Returns list of instruction words."""

    # --- Pass 1: collect labels ---
    labels = {}
    addr = 0
    cleaned = []
    for lineno, raw in enumerate(lines, 1):
        line = raw.split(';')[0].strip()
        if not line:
            continue
        # Label?
        if ':' in line and not line.startswith((' ', '\t')):
            parts = line.split(':', 1)
            label = parts[0].strip()
            labels[label] = addr
            rest = parts[1].strip()
            if rest:
                cleaned.append((lineno, addr, rest))
                addr += 1
        else:
            cleaned.append((lineno, addr, line))
            addr += 1

    # --- Pass 2: encode ---
    words = []
    for lineno, pc, line in cleaned:
        try:
            word = assemble_line(line, pc, labels)
            words.append(word)
        except Exception as e:
            print(f"Error on line {lineno}: {e}", file=sys.stderr)
            print(f"  {line}", file=sys.stderr)
            sys.exit(1)

    return words

def assemble_line(line, pc, labels):
    """Assemble a single instruction line."""
    # Tokenise
    tokens = re.split(r'[,\s]+', line.strip())
    tokens = [t for t in tokens if t]
    mnemonic = tokens[0].upper()

    # --- Pseudo-instructions ---
    if mnemonic == 'NOP':
        return encode_type1(T1_OPS['ADD'], 0, 0, 0, 0, 0)
    if mnemonic == 'HALT':
        # JMP to self (PC-relative offset 0)
        return encode_type2(T2_OPS['JMP'], 0, 0, 0)

    # --- Type 1 ---
    if mnemonic in T1_OPS:
        opcode14 = T1_OPS[mnemonic]

        if mnemonic in T1_UNARY:
            # Unary: MNEMONIC rd, rs1  (or rd, #imm for LUI)
            rd = parse_reg(tokens[1])
            arg1_str = tokens[2]
            if arg1_str.startswith('#'):
                imm_val = parse_imm(arg1_str)
                if not (0 <= imm_val <= TYPE1_IMM_MAX):
                    raise ValueError(
                        f"Immediate #{imm_val} out of range for Type 1 "
                        f"(must be 0–{TYPE1_IMM_MAX}); "
                        f"use LI for 16-bit values")
                return encode_type1(opcode14, 1, imm_val, 0, 0, rd)
            else:
                return encode_type1(opcode14, 0, parse_reg(arg1_str),
                                    0, 0, rd)
        else:
            # Binary: MNEMONIC rd, rs1, rs2/#imm
            rd = parse_reg(tokens[1])
            arg1_str = tokens[2]
            arg2_str = tokens[3]

            # arg1
            if arg1_str.startswith('#'):
                a1_val = parse_imm(arg1_str)
                if not (0 <= a1_val <= TYPE1_IMM_MAX):
                    raise ValueError(
                        f"Immediate #{a1_val} out of range for Type 1 "
                        f"(must be 0–{TYPE1_IMM_MAX}); "
                        f"use LI for 16-bit values")
                a1_imm = 1
            else:
                a1_imm, a1_val = 0, parse_reg(arg1_str)

            # arg2
            if arg2_str.startswith('#'):
                a2_val = parse_imm(arg2_str)
                if not (0 <= a2_val <= TYPE1_IMM_MAX):
                    raise ValueError(
                        f"Immediate #{a2_val} out of range for Type 1 "
                        f"(must be 0–{TYPE1_IMM_MAX}); "
                        f"use LI for 16-bit values")
                a2_imm = 1
            else:
                a2_imm, a2_val = 0, parse_reg(arg2_str)

            return encode_type1(opcode14, a1_imm, a1_val,
                                a2_imm, a2_val, rd)

    # --- Type 2 ---
    if mnemonic in T2_OPS:
        opcode9 = T2_OPS[mnemonic]

        if mnemonic == 'LI':
            reg = parse_reg(tokens[1])
            imm16 = parse_imm(tokens[2])
            return encode_type2(opcode9, 1, reg, imm16 & TYPE2_ADDR_MASK)

        if mnemonic == 'RET':
            return encode_type2(opcode9, 0, 0, 0)

        if mnemonic in ('PUSH', 'POP'):
            reg = parse_reg(tokens[1])
            return encode_type2(opcode9, 0, reg, 0)

        if mnemonic in BRANCH_OPS:
            target = tokens[1]
            # Check if it's a label
            if target in labels:
                offset = labels[target] - pc
                return encode_type2(opcode9, 0, 0, offset & TYPE2_ADDR_MASK)
            elif target.startswith('#'):
                addr16 = parse_imm(target)
                return encode_type2(opcode9, 1, 0, addr16 & TYPE2_ADDR_MASK)
            else:
                # Try as numeric offset
                offset = parse_imm(target)
                return encode_type2(opcode9, 0, 0, offset & TYPE2_ADDR_MASK)

        if mnemonic in MEM_OPS:
            reg = parse_reg(tokens[1])
            # Reconstruct the address operand (tokenizer may have split it on
            # spaces inside brackets, e.g. [r30 + -8] → ['[r30', '+', '-8]']).
            mem_str = "".join(tokens[2:]).strip("[]")
            # Register-indirect: [rN], [rN+offset], [rN + offset], [rN + -N]
            ri_m = re.match(
                r'^(r\d{1,2}|sp|lr)(.*)?$', mem_str.replace(' ', ''),
                re.IGNORECASE)
            if ri_m and not mem_str.lstrip().startswith('#'):
                base_reg_str = ri_m.group(1)
                # Verify it really is a register, not a label starting with 'r'
                try:
                    base_reg = parse_reg(base_reg_str)
                except ValueError:
                    pass
                else:
                    offset_str = ri_m.group(2) or ''
                    # Normalise LLVM-style '+-N' → '-N'
                    offset_str = re.sub(r'^\+-', '-', offset_str)
                    offset = int(offset_str) if offset_str else 0
                    return encode_type2(opcode9, 0, reg, offset & TYPE2_ADDR_MASK)
            # Absolute: [#addr], [addr], [label]
            if mem_str.startswith('#'):
                addr16 = parse_imm(mem_str)
            elif mem_str in labels:
                addr16 = labels[mem_str]
            else:
                addr16 = parse_imm(mem_str)
            return encode_type2(opcode9, 1, reg, addr16 & TYPE2_ADDR_MASK)

    raise ValueError(f"Unknown mnemonic: {mnemonic}")

def main():
    parser = argparse.ArgumentParser(description='Two-pass assembler')
    parser.add_argument('input', help='Input .asm file')
    parser.add_argument('-o', '--output', default='program.hex',
                        help='Output .hex file (default: program.hex)')
    parser.add_argument('--disasm', action='store_true',
                        help='Print per-instruction hex/binary dump after assembly')
    args = parser.parse_args()

    with open(args.input, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    words = assemble(lines)

    with open(args.output, 'w') as f:
        for w in words:
            f.write(f'{w:08X}\n')

    print(f"Assembled {len(words)} instructions → {args.output}")

    if args.disasm:
        for i, w in enumerate(words):
            print(f"  0x{i:04X}: 0x{w:08X}  ({w:032b})")

if __name__ == '__main__':
    main()
