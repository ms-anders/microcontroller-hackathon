`include "include/config.vh"
`include "include/opcodes.vh"

`timescale 1ns/1ps

module tb;
    reg clk = 0;
    reg [`OPCODE_W-1:0] op = 0;
    reg signed [`DATA_WIDTH-1:0] a = 0;
    reg signed [`DATA_WIDTH-1:0] b = 0;

    wire signed [`DATA_WIDTH-1:0] dbg_result;
    wire dbg_eq;
    wire dbg_gt;
    wire dbg_lt;
    wire dbg_carry;

    always #5 clk = ~clk; // 100MHz clock

    alu u_alu(
        .op(op),
        .a(a),
        .b(b),
        .result(dbg_result),
        .is_equal(dbg_eq),
        .is_greater(dbg_gt),
        .is_less(dbg_lt),
        .carry_out(dbg_carry)
    );

    reg [255*8-1:0] program_file;
    integer         max_cycles;
    integer         cycle_count;

    initial begin
        $dumpfile("alu_tb.vcd");
        $dumpvars(0, tb);

        op = `OP_ADD;
        a = `DATA_WIDTH'd25;
        b = `DATA_WIDTH'd9;
        $display("ADD: %0d + %0d = %0d", a, b, dbg_result);

        #10;

        op = `OP_SUB;
        a = `DATA_WIDTH'd25;
        b = `DATA_WIDTH'd9;
        $display("SUB: %0d - %0d = %0d", a, b, dbg_result);

        #10;

        op = `OP_NEG;
        a = `DATA_WIDTH'd25;
        $display("NEG: -%0d = %0d", a, dbg_result);

        #10;

        op = `OP_AND;
        a = `DATA_WIDTH'd62;
        b = `DATA_WIDTH'd13;
        $display("AND: %0d & %0d = %0d", a, b, dbg_result);

        #10;

        op = `OP_CMP;
        a = `DATA_WIDTH'd25;
        b = `DATA_WIDTH'd9;
        $display("CMP: %0d vs %0d", a, b);
        $display("EQ: %0d, GT: %0d, LT: %0d", dbg_eq, dbg_gt, dbg_lt);

        #10;

        a = `DATA_WIDTH'd14;
        b = `DATA_WIDTH'd14;
        $display("CMP: %0d vs %0d", a, b);
        $display("EQ: %0d, GT: %0d, LT: %0d", dbg_eq, dbg_gt, dbg_lt);

        #10;

        a = `DATA_WIDTH'd9;
        b = `DATA_WIDTH'd25;
        $display("CMP: %0d vs %0d", a, b);
        $display("EQ: %0d, GT: %0d, LT: %0d", dbg_eq, dbg_gt, dbg_lt);

        $finish;

    end

    /*
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
    */
endmodule

