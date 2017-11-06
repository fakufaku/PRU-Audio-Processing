/* Code for the CIC filter in PRU1. */

/* PSEUDOCODE:

Wait for falling (rising ?) edge...
Wait for t_dv time (max 125ns, so 25 cycles)...
Read data from the DATA signal.

counter += 1
r0 += data   // XXX: why not just equals ?
r1 += r0
r2 += r1
r3 += r2

if counter % 64 == 0:
    // TODO

*/

/* Instructions set and macros:

http://processors.wiki.ti.com/index.php/PRU_Assembly_Instructions

*/

#include "prudefs.hasm"

// TODO: Figure out how interrupts work
#define PRU1_ARM_INTERRUPT 20

// Input pins offsets
#define CLK_OFFSET 0
#define DATA_OFFSET 1

// Register aliases
#define IN_PINS r31
#define SAMPLE_COUNTER r5
#define WAIT_COUNTER r6
#define TMP_REG r7

#define INT0 r0
#define INT1 r1
#define INT2 r2
#define INT3 r3
#define LAST_INT r4

// TODO: define more registers for comb stage
#define COMB0 r10
#define COMB1 r11
#define COMB2 r12
//#define COMB3 r13
#define LAST_COMB0 r14
#define LAST_COMB1 r15
#define LAST_COMB2 r16

.origin 0
.entrypoint TOP

TOP:
    // Setup counters to 0 at first
    LDI     SAMPLE_COUNTER, 0
    // Set all integrator and comb registers to 0 at first
    LDI     INT0, 0
    LDI     INT1, 0
    LDI     INT2, 0
    LDI     INT3, 0
    LDI     COMB0, 0
    LDI     COMB1, 0
    LDI     COMB2, 0
    LDI     COMB3, 0
    LDI     LAST_INT, 0
    LDI     LAST_COMB0, 0
    LDI     LAST_COMB1, 0
    LDI     LAST_COMB2, 0

WAIT_EDGE:
    // First wait for CLK = 0
    WBC     IN_PINS, CLK_OFFSET
    // Then wait for CLK = 1
    WBS     IN_PINS, CLK_OFFSET

    // Wait for t_dv time, since it can be at most 125ns, we have to wait for 25 cycles
WAIT_SIGNAL:
    LDI     WAIT_COUNTER, 8 // Because 8 = ceil(25 / 3) and the loop takes 3 one-cycle ops
    SUB     WAIT_COUNTER, WAIT_COUNTER, 1
    QBNE    WAIT_SIGNAL, WAIT_COUNTER, 0

    // Retrieve data from DATA pin (only one bit!)
    AND     TMP_REG, IN_PINS, 1 << DATA_OFFSET
    LSR     TMP_REG, TMP_REG, DATA_OFFSET
    // Do the integrator operations
    ADD     SAMPLE_COUNTER, SAMPLE_COUNTER, 1
    ADD     INT0, INT0, TMP_REG
    ADD     INT1, INT1, INT0
    ADD     INT2, INT2, INT1
    ADD     INT3, INT3, INT2

    // Branch for oversampling
    QBNE    WAIT_EDGE, SAMPLE_COUNTER, 64

    // Reset sample counter once we reach R
    LDI     SAMPLE_COUNTER, 0

    // 4 stage comb filter
    SUB     COMB0, INT3, LAST_INT
    SUB     COMB1, COMB0, LAST_COMB0
    SUB     COMB2, COMB1, LAST_COMB1
    SUB     TMP_REG, COMB2, LAST_COMB2

    // TODO: normalize level and output the result to memory

    // Update LAST_INT value and LAST_COMBs
    // TODO: check this is correct, and this could perhaps be done in fewer instructions
    MOV     LAST_INT, INT3
    MOV     LAST_COMB0, COMB0
    MOV     LAST_COMB1, COMB1
    MOV     LAST_COMB2, COMB2

    // Branch back to wait edge
    QBA     WAIT_EDGE

    // Interrupt the host so it knows we're done
    MOV     r31.b0, PRU1_ARM_INTERRUPT + 16

    HALT
