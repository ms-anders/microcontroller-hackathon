`ifndef MY_CONFIG_VH
`define MY_CONFIG_VH

`define DATA_WIDTH 64
`define DATA_ADDR_W 16

`define INSTR_WIDTH 32
`define INSTR_ADDR_W 12
`define REG_COUNT 32
`define REG_ADDR_W 5

// IMEM Size = 12 address bits = 2^12 addresses = 4096 instructions = 16 KiB
`define IMEM_DEPTH 4096   // in INSTRUCTIONS (16384 bytes, 2048 words)
// DMEM Size = 16 address bits = 2^16 addresses = 65536 bytes = 64 KiB
`define DMEM_DEPTH 65536  // in BYTES (8192 words)

`define DATA_MSB (`DATA_WIDTH- 1)
`define ADDR_MSB (`DATA_ADDR_W- 1)
`define PC_INIT {`INSTR_ADDR_W{0}}

`endif