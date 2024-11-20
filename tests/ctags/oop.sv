/* Tests the following SystemVerilog ctags:
 * - [x] constant
 * - [x] define
 * - [ ] event
 * - [x] function
 * - [ ] module
 * - [ ] net
 * - [ ] port
 * - [x] register
 * - [x] task
 * - [ ] block
 * - [ ] instance
 * - [x] class
 * - [x] enum
 * - [x] interface
 * - [x] modport
 * - [x] package
 * - [ ] program
 * - [x] prototype
 * - [x] struct
 * - [x] typedef
 * - [z] member
 * - [x] ifclass
 * - [x] nettype
 */

package oop;

`define MACRO 1234 // NB: Shouldn't the ctag for this be outside of the package? (ctags bug?)

/* Constants */

const      int  ANSWER_TO_LIFE = 42;
localparam real APPROX_PI      = 3.14;

/* Methods */

task display_int(int x);
    $display("%0d", x);
endtask

function automatic resolve_nettype(real driver[]);
    resolve_nettype = 0.0;
    foreach (driver[i])
        resolve_nettype += driver[i];
endfunction

/* Typedefs */

nettype real net_t with resolve_nettype;

typedef struct packed {
    logic [7:0] data;
    logic       parity;
} struct_t;

typedef union packed {
    logic [7:0] data;
    logic [3:0] control;
} union_t;

typedef enum {
    red,
    yellow,
    green
} enum_t;

/* Classes */

interface class ifclass #(
    type T = logic
);
    pure virtual function T    get_value();
    pure virtual function void set_value(T x);
endclass : ifclass

class a_class implements ifclass #(int);

    int value;

    virtual function int get_value();
        get_value = value;
    endfunction

    virtual function void put_value(int x);
        value = x;
    endfunction

    task print_value();
        $display("value = %0d", value);
    endtask

    function value_plus(int x);
        value_plus = value + x;
    endfunction

endclass : a_class

class other_class;

    struct packed {
        byte s_a, s_b, s_c;
        string s_str;
    } s_member;

    union  packed {
        byte u_a;
        int  u_b;
    } u_member;

    enum { ready, steady, go } e_member;

    struct_t st_member;
    union_t  ut_member;
    enum_t   et_member;

endclass : other_class

/*virtual*/ class vclass; // NB: ctags bug!!  Declaring class as virtual inhibits detection of next element.
    pure virtual function void do_something();
endclass : vclass

/* Interface (not related to "interface class") */

interface spi;

    logic sclk;
    logic cs_n;
    logic mosi;
    logic miso;

    modport master (output sclk, cs_n, mosi,  input  miso);
    modport slave  (input  sclk, cs_n, mosi,  output miso);

    task enable_cs();
        cs_n <= 1'b0;
    endtask

    task disable_cs();
        cs_n <= 1'b1;
    endtask

endinterface : spi

endpackage : oop
