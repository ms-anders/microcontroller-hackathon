`ifndef OPCODES_VH
`define OPCODES_VH

`define OP_ADD  6'b100000
`define OP_SUB  6'b100001
`define OP_NEG  6'b110010
 
 
`define OP_AND  6'b100100
`define OP_OR   6'b100101
`define OP_XOR  6'b100110
`define OP_NOT  6'b110111
 
`define OP_LSL  6'b101000
`define OP_LSR  6'b101001
`define OP_ASR  6'b101011
 
`define OP_CMP  6'b101100
 
 
 
`define OP_MOV  6'b000000
`define OP_LI2  6'b000001
 
`define OP_LD   6'b010000
`define OP_STO  6'b010001
 
`define OP_LDB  6'b010010
`define OP_STB  6'b010011

`define OP_POP  6'b010100
`define OP_PUSH 6'b010101



`define OP_JMP  6'b001000
 
`define OP_JEQ  6'b001010
`define OP_JNE  6'b001011
`define OP_JLT  6'b001100
`define OP_JGT  6'b001101
 
`define OP_JSR  6'b011001
`define OP_RET  6'b011000

`endif