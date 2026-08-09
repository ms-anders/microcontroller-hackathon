`include "include/config.vh"
`include "include/opcodes.vh"

module branch (
    input logic [`INSTR_ADDR_W-1:0] pc,
    input logic [`OPCODE_W-1:0] op,
    input logic [`INSTR_ADDR_W-1:0] target,
    input logic is_jump,
    input logic is_greater,
    input logic is_less,
    input logic is_equal,
    output logic [`INSTR_ADDR_W-1:0] next_pc,
    output logic jump_taken
);
    always_comb begin
        jump_taken = 1'b0;
        case (op)
            `OP_JMP: jump_taken = is_jump;
            `OP_JEQ: jump_taken = is_equal;
            `OP_JNE: jump_taken = !is_equal;
            `OP_JLT: jump_taken = is_less;
            `OP_JGT: jump_taken = is_greater;
            default: jump_taken = 1'b0;
        endcase
    end

    assign next_pc = jump_taken ? target : pc + 1;
endmodule