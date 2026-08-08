`ifndef MY_CONFIG_VH
`define MY_CONFIG_VH

`define DATA_WIDTH
`define ADDR_WIDTH

`define INSTR_WIDTH
`define REG_COUNT
`define REG_ADDR_W
`define IMEM_DEPTH
`define DMEM_DEPTH

`define DATA_MSB (`DATA_WIDTH- 1)
`define ADDR_MSB (`ADDR_WIDTH- 1)
`define PC_INIT {`ADDR_WIDTH{??}}


