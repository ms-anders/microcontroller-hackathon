<<<<<<< HEAD
`include "include/config.vh"
=======
// rtl/dmem.v
`include "config.vh"
>>>>>>> 22760ec (12345678#)

module dmem #(
    parameter DEPTH = `DMEM_DEPTH
) (
<<<<<<< HEAD
    input  logic clk,
    input  logic wr_en,
    input  logic is_word,
    input  logic [`ADDR_MSB:0] addr,
    input  logic [`DATA_MSB:0] wr_data,
    output logic [`DATA_MSB:0] rd_data
) {
    logic [`DATA_MSB:0] mem [0:DEPTH-1];

    
}
=======
    input  wire                  clk,
    input  wire                  wr_en,
    input  wire [`ADDR_MSB:0]    addr,
    input  wire [`DATA_MSB:0]    wr_data,
    output wire [`DATA_MSB:0]    rd_data
);

    reg [`DATA_MSB:0] mem [0:DEPTH-1];

    always @(posedge clk) begin
        if (wr_en)
            mem[addr] <= wr_data;
    end

    assign rd_data = mem[addr];  // asynchronous read

endmodule
>>>>>>> 22760ec (12345678#)
