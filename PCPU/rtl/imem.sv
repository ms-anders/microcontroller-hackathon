`include "include/config.vh"

module imem #(
    parameter DEPTH = `IMEM_DEPTH
)(
    input  logic [`INSTR_ADDR_W-1:0] addr,
    output logic [`INSTR_WIDTH-1:0] data
);

    logic [`INSTR_WIDTH-1:0] mem [0:DEPTH-1];
    assign data = mem[addr];

endmodule