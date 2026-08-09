`include "include/config.vh"
`include "include/opcodes.vh"

module decoder (
    input  logic [`INSTR_WIDTH-1:0] instr,

    // Outputs: one per field in your encoding
    // These names and widths should match YOUR ISA spec
    output logic                    is_alu_op,   // is this an arithmetic/logic instruction?
    output logic                    is_branch,   // is this a branch instruction?
    output logic                    is_mem_op,   // is this a memory instruction?
    output logic [5:0]              opcode,      // which operation?
    output logic [2:0]              rd,          // destination register
    output logic [4:0]              rs1,         // source register 1
    output logic [4:0]              rs2,         // source register 2 (or immediate)
    output logic [7:0]              immediate,   // immediate value
    output logic                    imm_select,  // use immediate instead of rs2?
    output logic                    reg_write_en // should we write back a result?
    // Add more outputs as your design requires
);
    assign opcode = instr[31:26];

    always_comb begin
        if (opcode[5]) begin
            is_alu_op = opcode[5];
            is_imm_op = instr[25];
            is_branch = 1'b0;
            is_mem_op = 1'b0;
        end else begin
            is_alu_op = 1'b0;
            {is_mem_op, is_branch} = opcode[4:3];
        end

        if (is_alu_op) begin
            rd = instr[24:20];
            rs1 = instr[19:15];
            rs2 = instr[14:10];
        end
    end

endmodule