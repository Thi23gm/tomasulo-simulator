#ifndef TOMASULO_H
#define TOMASULO_H

#include <deque>
#include <string>

// Define the maximum number of visible and invisible registers
#define VISIBLE_REGISTERS 12
#define INVISIBLE_REGISTERS 24
#define REGISTERS_MAX (VISIBLE_REGISTERS + INVISIBLE_REGISTERS)

// Enumerate different types of operations
enum op_t {
    add,    // Addition
    sub,    // Subtraction
    mul,    // Multiplication
    divd,   // Division
    lw,     // Load
    sw      // Store
};

// Enumerate register identifiers
enum reg_t {
    r0, r1, r2, r3, r4, r5,
    r6, r7, r8, r9, r10, r11,
    ra, rb, rc, rd, re, rf,
    rg, rh, ri, rj, rk, rl,
    rm, rn, ro, rp, rq, rr,
    rs, rt, ru, rv, rw, rx,
    noreg   // No register
};

// Enumerate functional unit identifiers
enum funum_t { add1, add2, mul1, mul2, load1, load2, store1, store2 };

// Enumerate the usage of registers
enum used_as_t { free_reg, dest, src };

// Structure to store register information
struct regstat_t {
    int value;              // Register value
    used_as_t used_as;      // How the register is being used
    int dest_used_by;       // Instruction ID using the register as destination
    reg_t renamed_to;       // Renamed to register
    reg_t renamed_from;     // Renamed from register
    int rename_ref_count;   // Number of references to the renamed register
};

// Structure to represent an instruction
struct inst_t {
    op_t op;        // Operation
    reg_t dest;     // Destination register
    reg_t src1;     // Source register 1
    reg_t src2;     // Source register 2
    int imm;        // Immediate value (for load and store instructions)
    int time;       // Execution time of the instruction
    int issue;      // Time when the instruction was issued
    int exec;       // Time when the instruction started execution
    int write;      // Time when the instruction finished execution (write-back)
    int commit;     // Time when the instruction committed
};

// Define execution times for different operations
#define add_time 2
#define sub_time 2
#define mul_time 10
#define div_time 40
#define lw_time 5
#define sw_time 5

// Define station sizes for each type of reservation station
#define ALL_STATIONS 6
#define STATION_TYPES 3
#define ADD_STATIONS 2
#define MUL_STATIONS 2
#define LOAD_STATIONS 2

// Structure to represent a functional unit
struct fu_t {
    int id;             // Identifier
    bool busy;          // Indicates if the unit is busy
    inst_t *inst;       // Pointer to the instruction in the unit
    int vj;             // Value of source register 1
    int vk;             // Value of source register 2
    int qj;             // Identifier of the instruction producing vj
    int qk;             // Identifier of the instruction producing vk
    int time_left;      // Time left for execution
    bool locks1;        // Indicates if source register 1 is locked
    bool locks2;        // Indicates if source register 2 is locked
};

#endif // TOMASULO_H
