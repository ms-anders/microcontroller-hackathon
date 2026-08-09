`include "include/config.vh"

module decoder (
    input logic [`INSTR_WIDTH-1:0] instr,
    output logic [`REG_ADDR_W-1:0] r1_addr,
    output logic [`REG_ADDR_W-1:0] r2_addr,
    output logic [`REG_ADDR_W-1:0] rd_addr,
    output logic                   is_alu_op,
    output logic                   is_data_wr_op
);



endmodule