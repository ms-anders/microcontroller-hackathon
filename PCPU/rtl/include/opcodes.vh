`ifndef OPCODES_VH
`define OPCODES_VH

`define OP_ADD 100000
`define OP_SUB 100001
`define OP_NEG 100010


`define OP_AND 100100
`define OP_OR  100101
`define OP_XOR 100110
`define OP_NOT 100111

`define OP_LSL 101000
`define OP_LSR 101001
`define OP_ASR 101011

`define OP_CMP 101100



`define OP_MOV 000000
`define OP_LI2 000001

`define OP_LD  010000
`define OP_STO 010001

`define OP_LDB 010100
`define OP_STB 010101

`define OP_POP  010010
`define OP_PUSH 010011



`define OP_JMP 001000

`define OP_JEQ 001010
`define OP_JNE 001011
`define OP_JL  001100
`define OP_JG  001101

`define OP_JSR 011000
`define OP_RET 011001 

`endif