/* Tests the following SystemVerilog ctags:
 * - [x] constant
 * - [x] define
 * - [x] event
 * - [x] function
 * - [x] module
 * - [x] net
 * - [x] port
 * - [x] register
 * - [x] task
 * - [x] block
 * - [x] instance
 * - [ ] class
 * - [ ] enum
 * - [ ] interface
 * - [ ] modport
 * - [ ] package
 * - [x] program
 * - [ ] prototype
 * - [ ] struct
 * - [ ] typedef
 * - [ ] member
 * - [ ] ifclass
 * - [ ] nettype
 */

`timescale 1ns/1ps
`default_nettype none

`define DEFAULT_WIDTH 32
`define BITS_TO_BYTES(x) ((x)/8)

module a_module #(
    parameter  WIDTH = `DEFAULT_WIDTH,
    localparam BYTES = `BITS_TO_BYTES(WIDTH)
) (
    input                    clk, reset,
    input  logic [WIDTH-1:0] data_in,
    input  logic             valid,
    input  wire  [BYTES-1:0] byte_en,
    output wire  [WIDTH-1:0] data_out
);

    wire be_filtered [BYTES-1:0];

    genvar i;
    generate
        for (i = 0; i < BYTES; i++) begin : be_filtered_gen
            logic x;
            and and_gate ( // NB: this SHOULD be detected as an instance (ctags bug?)
                x,
                byte_en[i],
                valid
            );
            assign be_filtered[i] = x;
        end
    endgenerate

    wire [BYTES-1:0][7:0] data_in_bytes;
    reg  [BYTES-1:0][7:0] data_out_bytes;

    assign data_in_bytes = data_in;
    assign data_out = data_out_bytes;

    always @(posedge clk, posedge reset) begin : main_block
        if (reset)
            data_out_bytes <= '0;
        else begin
            int i;
            for (i = 0; i < BYTES; i++)
                if (be_filtered[i])
                    data_out_bytes[i] <= data_in_bytes[i];
        end
    end

endmodule : a_module

program generate_signals #(
    parameter  NUM_UUT   =  4,
    parameter  UUT_WIDTH = 64,
    localparam UUT_BYTES = `BITS_TO_BYTES(UUT_WIDTH)
) (
    input  logic                 clk, reset,
    output logic [UUT_WIDTH-1:0] data_in,
    output logic                 valid_uut [NUM_UUT-1:0],
    output logic [UUT_BYTES-1:0] byte_en,
    output event                 finished // NB: counts as "port", not "event"
);

    task write_byte(
        byte byte_data,
        int  byte_index,
        int  uut_index
    );
    begin
        logic [UUT_BYTES-1:0][7:0] data_in_bytes;
        data_in_bytes = '0;
        data_in_bytes[byte_index] = byte_data;
        data_in = data_in_bytes;
        byte_en = UUT_BYTES'(1) << byte_index;

        valid_uut[uut_index] <= 1'b1;
        @(posedge clk);
        valid_uut[uut_index] <= 1'b0;
    end
    endtask

    initial begin
        @(negedge reset);
        @(posedge clk);

        write_byte(8'h12, 3, 0);
        write_byte(8'h34, 2, 0);
        write_byte(8'h56, 1, 0);
        write_byte(8'h78, 0, 0);

        @(posedge clk);

        -> finished;
    end

endprogram : generate_signals

module testbench;

    localparam NUM_UUT   =  4;
    localparam UUT_WIDTH = 64;
    localparam UUT_BYTES = `BITS_TO_BYTES(UUT_WIDTH);

    logic                 clk = 1'b0, reset = 1'b0;
    logic [UUT_WIDTH-1:0] data_in;
    logic                 valid_uut [NUM_UUT-1:0];
    logic [UUT_BYTES-1:0] byte_en;

    generate
        for (genvar j = 0; j < NUM_UUT; j++) begin : uut_gen
            logic [UUT_WIDTH-1:0] data_out;
            a_module #(
                .WIDTH (UUT_WIDTH)
            ) uut (
                .valid (valid_uut[j]),
                .*
            );
        end
    endgenerate

    event trigger_success, trigger_failure, finished;

    function logic compare(
        logic [UUT_BYTES-1:0][7:0] A, B,
        logic [UUT_BYTES-1:0]      mask
    );
    begin
        int i;
        for (i = 0; i < UUT_BYTES; i++)
            if (mask[i] && (A[i] != B[i])) begin
                return 1'b0;
            end
        compare = 1'b1; // same as return 1'b1
    end
    endfunction

    always begin : clk_gen
        clk = ~clk;
        #0.5;
    end

    initial begin : reset_gen
        reset = 1'b1;
        #3.5;
        reset = 1'b0;
    end

    generate_signals #(
        .NUM_UUT   (NUM_UUT),
        .UUT_WIDTH (UUT_WIDTH)
    ) gen_signals (
        .*
    );

    initial begin
        @finished;

        if (compare(uut_gen[0].data_out, 64'h 00000000_12_34_56_78, 8'b 0000_1111))
            $display("Comparison succeeded.");
        else
            $display("Comparison failed!");

        $finish;
    end

endmodule : testbench
