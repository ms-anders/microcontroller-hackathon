// rtl/cpu.v
`include "include/config.vh"

module cpu #(
    parameter IMEM_INIT_FILE = ""
) (
    input  wire                  clk,
    input  wire                  rst_n,
    output wire [`ADDR_MSB:0]    dbg_pc,
    output wire                  dbg_halt
);

    // ── Program Counter ──────────────────────────────────────────
    reg  [`ADDR_MSB:0] pc;
    wire [`ADDR_MSB:0] pc_next;

    assign dbg_pc  = pc;
    assign pc_next = pc + 1;  // Replaced in Stage 2

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n)
            pc <= `PC_INIT;
        else
            pc <= pc_next;
    end

    // ── Instruction Fetch ───────────────────────────────────────
    wire [`INSTR_WIDTH-1:0] instr;

    imem #(
        .DEPTH(`IMEM_DEPTH)
    ) u_imem (
        .addr(pc),
        .data(instr)
    );

    // ── Decode ──────────────────────────────────────────────────
    // Declare wires matching your decoder's outputs, then instantiate.
    // (Fill in based on your decoder's port list)
    wire             is_alu_op;
    wire [??:0]      opcode;
    wire [??:0]      rd, rs1, rs2;
    wire [??:0]      immediate;
    wire             imm_select;
    wire             reg_write_en;

    decoder u_decoder (
        .instr(instr)
        // ... connect each port ...
    );

    // ── Register File ─────────────────────────────────────────────
    wire [`DATA_MSB:0] rf_a, rf_b;
    wire [`DATA_MSB:0] wb_data;

    regfile u_regfile (
        .clk(clk),
        .wr_en(reg_write_en),
        .wr_addr(rd),
        .wr_data(wb_data),
        .rd_addr_a(rs1),
        .rd_data_a(rf_a),
        .rd_addr_b(rs2),
        .rd_data_b(rf_b)
    );

    // ── Operand mux (register vs immediate) ─────────────────────
    wire [`DATA_MSB:0] alu_a = rf_a;
    wire [`DATA_MSB:0] alu_b = imm_select ? /* zero-extend immediate */ : rf_b;

    // ── ALU ───────────────────────────────────────────────────────
    wire [`DATA_MSB:0] alu_result;

    alu u_alu (
        .op(opcode),
        .a(alu_a),
        .b(alu_b),
        .result(alu_result)
    );

    // ── Write-back mux ───────────────────────────────────────────
    // Where does wb_data come from?
    // Option A (simpler): treat LI as "ADD rd, r0, #imm" — it goes through
    // the ALU just like any other instruction, so wb_data = alu_result always.
    // This works if your ALU correctly handles immediates and your zero
    // register always reads 0.
    // Option B (separate path): LI has its own write-back mux entry:
    //   is_alu_op → alu_result
    //   is_li     → zero-extended immediate
    // Pick one, be consistent in your decoder and your ALU.
    // (Stage 3 adds: is_load → memory read data)
    assign wb_data = /* your mux here */;

    // ── Halt detection ───────────────────────────────────────────
    // Halt when the CPU executes a jump to its own address.
    // Add this after Stage 2 adds branch logic; for now a placeholder:
    assign dbg_halt = 1'b0;  // TODO: implement in Stage 2

endmodule