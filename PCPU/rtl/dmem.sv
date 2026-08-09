`include "include/config.vh"

module dmem #(
    parameter DEPTH = `DMEM_DEPTH
) (
    input  logic                  clk,
    input  logic                  wr_en,
    input  logic                  is_word,
    input  logic [`ADDR_MSB:0]    addr,
    input  logic [`DATA_MSB:0]    wr_data,
    output logic [`DATA_MSB:0]    rd_data
);

    logic [7:0] mem [0:DEPTH-1];
    logic [`ADDR_MSB:0] word_addr = (addr >> 2) << 2; // Align to word boundary

    logic [15:0] i;
    always @(posedge clk) begin
        if (wr_en)
            for (i = 0; i < 8; i++) begin
                mem[word_addr + i] <= wr_data[(i+1)*8 -: 8];
            end
        else
            mem[addr] <= wr_data[7:0]; // Write only the least significant byte if not a word write
    end

    logic [15:0] j;
    always_comb begin : blockName
        if (is_word)
            for (j = 0; j < 8; j++) begin
                rd_data[(j+1)*8-1 -: 8] = mem[word_addr + j];
            end
        else
            rd_data = {56'(mem[addr][7]), mem[addr]};
    end

endmodule
