// rtl/alu.v
`include "include/config.vh"
`include "include/opcodes.vh"
module alu (
input logic [`OPCODE_W-1:0] op, // use a config define for opcode width
input logic [`DATA_MSB:0] a,
input logic [`DATA_MSB:0] b,
output logic [`DATA_MSB:0] result,
output logic is_greater,
output logic is_less,
output logic is_equal
);
always @(*) begin
case (op)
`OP_ADD: result = a + b;
`OP_SUB: result = a - b;
`OP_MOV: result = a;
`OP_NEG: result = ~a + 1'b1;
`OP_AND: result = a & b;
`OP_OR:  result = a | b;
`OP_XOR: result = a ^ b;
`OP_NOT: result = ~a;
`OP_LSL: result = a << b;
`OP_LSR: result = a >> b;
`OP_ASR: result = $signed(a) >>> b;
`OP_CMP: begin if(a > b) is_greater = 1'b1; else is_greater = 1'b0;
if(a < b) is_less = 1'b1; else is_less = 1'b0;
if(a == b) is_equal = 1'b1; else is_equal = 1'b0; end


// Add more operations in later stages
default: result = {`DATA_WIDTH{1'b0}};
endcase
end
endmodule