`include "include/config.vh"

module decoder (
    input logic [`INSTR_WIDTH-1:0] instr,
    output logic [`REG_ADDR_W-1:0] r1_addr,
    output logic [`REG_ADDR_W-1:0] r2_addr,
    output logic [`REG_ADDR_W-1:0] rd_addr,
    output logic                   is_alu_op,
    output logic                   is_data_wr_op
);


// rtl/decoder.v
`include "include/config.vh"
`include "include/opcodes.vh"

module decoder (
    input  logic [`INSTR_WIDTH-1:0] instr,

    // Outputs: one per field in your encoding
    // These names and widths should match YOUR ISA spec
    output logic                    is_alu_op,   // is this an arithmetic/logic instruction?
    output logic [5:0]              opcode,      // which operation?
    output logic [2:0]              rd,          // destination register
    output logic [4:0]              rs1,         // source register 1
    output logic [4:0]              rs2,         // source register 2 (or immediate)
    output logic [7:0]              immediate,   // immediate value
    output logic                    imm_select,  // use immediate instead of rs2?
    output logic                    reg_write_en // should we write back a result?
    // Add more outputs as your design requires
);

    // Extract fields from the instruction word.
    // The bit ranges must match the layout you documented in your ISA spec.
    assign opcode    = instr[15:10];
    assign rd         = instr[9:7];
    assign rs1        = instr[6:4];
    assign rs2         = instr[3:1];
    assign immediate  = instr[7:0];
    // ... fill in the rest ...

    // Control signals derived from the opcode
    assign is_alu_op    = (opcode[1] == 1'b1); /* true when opcode is an arithmetic/logic op */;
    assign reg_write_en = (instr[15:10] == `OP_ADD || instr[15:10] == `OP_SUB || instr[15:10] == `OP_MOV); /* true for instructions that write to a register */;

endmodule