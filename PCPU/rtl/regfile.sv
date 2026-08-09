// rtl/regfile.v
`include "include/config.vh"

module regfile (
    input  wire                      clk,
    input  wire                      wr_en,
    input  wire [`REG_ADDR_W-1:0]    wr_addr,
    input  wire [`DATA_MSB:0]        wr_data,
    input  wire [`REG_ADDR_W-1:0]    rd_addr_a,
    input  wire [`REG_ADDR_W-1:0]    rd_addr_b,
    output wire [`DATA_MSB:0]        rd_data_a,
    output wire [`DATA_MSB:0]        rd_data_b
);

    reg [`DATA_MSB:0] regs [0:`REG_COUNT-1];

    // Synchronous write - adapt the zero-guard to your design.
    // Replace 0 below with whichever register number you use as your zero reg.
    always @(posedge clk) begin
        if (wr_en && wr_addr != 0)  // 0 = zero register - change to your choice
            regs[wr_addr] <= wr_data;
    end

    // Asynchronous reads - enforces the zero register on the read side too.
    // If your ISA has no zero register, just write: assign rd_data_a = regs[rd_addr_a];
    assign rd_data_a = (rd_addr_a == 0) ? {`DATA_WIDTH{1'b0}} : regs[rd_addr_a];
    assign rd_data_b = (rd_addr_b == 0) ? {`DATA_WIDTH{1'b0}} : regs[rd_addr_b];

endmodule