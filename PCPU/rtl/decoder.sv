`include "include/config.vh"
`include "include/opcodes.vh"

module decoder (
    input  logic [`INSTR_WIDTH-1:0] instr,

    // Outputs: one per field in your encoding
    // These names and widths should match YOUR ISA spec
    output logic                     is_alu_op,   // is this an arithmetic/logic instruction?
    output logic                     is_branch,   // is this a branch instruction?
    output logic                     is_mem_op,   // is this a memory instruction?
    output logic [5:0]               opcode,      // which operation?
    output logic [4:0]               rd,          // destination register
    output logic [4:0]               rs1,         // source register 1
    output logic [4:0]               rs2,         // source register 2 (or immediate)
    output logic [14:0]              immediate,   // immediate value
    output logic [`INSTR_ADDR_W-1:0] jump_target, // target address for branches
    output logic [`DATA_ADDR_W-1:0]  mem_addr,    // memory address for load/store
    output logic                     is_imm_op,   // use immediate instead of rs2?
    output logic                     reg_write_en, // should we write back a result?
    output logic                     mem_write_en // should we write to memory?  
    // Add more outputs as your design requires
);
    assign opcode = instr[31:26];

    assign is_alu_op = opcode[5];
    assign mem_write_en = is_mem_op && opcode[0];

    always_comb begin
        if (is_alu_op) begin
            is_imm_op = instr[25];

            {is_mem_op, is_branch} = 2'b0;
        end else begin
            is_imm_op = 1'b0;

            {is_mem_op, is_branch} = opcode[4:3];
        end

        if (is_alu_op) begin
            rd = instr[24:20];
            rs1 = instr[19:15];

            reg_write_en = 1'b1;

            if (is_imm_op) begin
                rs2 = 5'b0;
                immediate = instr[14:0];
            end else begin
                rs2 = instr[14:10];
                immediate = 15'b0;
            end

        end else begin
            

        end
    end

endmodule