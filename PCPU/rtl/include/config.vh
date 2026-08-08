`ifndef MY_CONFIG_VH
`define MY_CONFIG_VH

`define DATA_WIDTH 64
`define ADDR_WIDTH 16

`define INSTR_WIDTH 32
`define REG_COUNT 32
`define REG_ADDR_W 5

// TOTAL MEMORY SIZE = 131072 bytes = 131KB
`define IMEM_DEPTH 4096  // in WORDS (32768 bytes)
`define DMEM_DEPTH 98304 // in BYTES

`define DATA_MSB (`DATA_WIDTH- 1)
`define ADDR_MSB (`ADDR_WIDTH- 1)
`define PC_INIT {`ADDR_WIDTH{??}}

`endif