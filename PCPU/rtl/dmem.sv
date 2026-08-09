`include "include/config.vh"

module dmem #(
    parameter DEPTH = `DMEM_DEPTH
) (
    input  logic clk,
    input  logic wr_en,
    input  logic is_word,
    input  logic [`ADDR_MSB:0] addr,
    input  logic [`DATA_MSB:0] wr_data,
    output logic [`DATA_MSB:0] rd_data
) {
    logic [`DATA_MSB:0] mem [0:DEPTH-1];

    
}