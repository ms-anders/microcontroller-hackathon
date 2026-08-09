`include "include/config.vh"
`include "include/opcodes.vh"

`timescale 1ns/1ps

module alu (
    input logic [`OPCODE_W-1:0] op, // use a config define for opcode width
    input logic [`DATA_MSB:0] a,
    input logic [`DATA_MSB:0] b,
    output logic [`DATA_MSB:0] result = `DATA_WIDTH'b0,
    output logic carry_out = 1'b0,
    output logic is_greater = 1'b0,
    output logic is_less = 1'b0,
    output logic is_equal = 1'b0
);

    always_comb begin
        result = `DATA_WIDTH'b0; // default to previous value
        is_greater = 1'b0; // default to previous value
        is_less = 1'b0; // default to previous value
        is_equal = 1'b0; // default to previous value
        carry_out = 1'b0; // default to previous value

        case (op)
            `OP_ADD: {carry_out, result} = a + b;
            `OP_SUB: result = a - b;
            `OP_MOV: result = a;
            `OP_NEG: result = ~a + 1'b1;
            `OP_AND: result = a & b;
            `OP_OR:  result = a | b;
            `OP_XOR: result = a ^ b;
            `OP_NOT: result = ~a;
            `OP_LSL: result = a << b;
            `OP_LSR: result = a >> b;
            `OP_ASR: result = $signed(a) >>> b;
            `OP_CMP: {is_greater, is_less, is_equal} = {a > b, a < b, a == b};


            // Add more operations in later stages
            default: result = {`DATA_WIDTH{1'b0}};
        endcase
    end
endmodule
