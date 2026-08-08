`include "include/config.vh"
`timescale 1ns/1ps

module tb;
reg clk = 0;
reg ist_n = 0;

wire[`ADDR_WIDTH-1:0] dbg_pc;
wire dbg_halt;

always #5 clk = ~clk; // 100MHz clock

cpu u_cpu (
    .clk(clk),
    .rst_n(rst_n),
    .dbg_pc(dbg_pc),
    .dbg_halt(dbg_halt)
);

reg [255*8-1:0] program_file;
integer         max_cycles;
integer         cycle_count;

initial begin
    // Program file and cycle limit come from command-line plusargs,
    // set automatically to simulate.py from your .yml config.
    if (!$value$plusargs("PROGRAM=%s", program_file)) begin
        $display("ERROR: no <PROGRAM=<file>> specified");
        $finish;
    end
    if (!$value$plusargs("MAX_CYCLES=%d", max_cycles)) begin
        max_cycles = 100000;
    end

    $readmemh(program_file, u_cpu.u_imem.mem);
    $dumpfile("cpu_tb.vcd");
    $dumpvars(0, test_harness);

    #25 rst_n = 1;  // hold reset for a few cycles

    cycle_count = 0;
    while (!dbg_halt && cycle_count < max_cycles) begin
        @(posedge clk);
        cycle_count = cycle_count + 1;
    end

    if (dbg_halt)
        $display("CPU halted after %0d cycles", cycle_count);
    else
        $display("TIMEOUT after %0d cycles (no HALT reached)", cycle_count);

    // Print register state on completion.
    // simulate.py parses this output to verify results.
    // Format must be: rN = 0x<hex>
    begin : dump_regs
        integer i;
        for (i = 0; i < REG_COUNT; i = i + 1)
            $display("r%0d = 0x%08h", i, u_cpu.u_regfile.regs[i]);
    end

    $finish;
end
endmodule

