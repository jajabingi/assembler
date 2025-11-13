#ifndef OP_MODES_H
#define OP_MODES_H

#include "assembler.h"  /* for CommandsTable, addr_mode */

/* -----------------------------------------------------------
   Addressing-mode masks (bit flags, based on enum values)
   ----------------------------------------------------------- */
/* Macro to generate a bit mask for a given addressing mode */
#define AM_BIT(n)   (1u << (n))

/* Individual addressing modes as bit masks */
#define AM_IMM      AM_BIT(ADDR_IMMEDIATE)  /* Immediate (#value) */
#define AM_DIR      AM_BIT(ADDR_DIRECT)     /* Direct (label) */
#define AM_MAT      AM_BIT(ADDR_MATRIX)     /* Matrix access (label[row][col]) */
#define AM_REG      AM_BIT(ADDR_REGISTER)   /* Register (r0..r7) */

/* Special masks */
#define AM_NONE     0u                                /* No addressing modes allowed */
#define AM_ALL      (AM_IMM | AM_DIR | AM_MAT | AM_REG) /* All modes allowed */


/* -----------------------------------------------------------
   Extra constants for diagnostic and CSV formatting
   ----------------------------------------------------------- */
#define AM_FIRST_BIT 0u                /* First bit index in mask */
#define AM_LAST_BIT  3u                /* Last bit index in mask */
#define AM_MASK_CSV_MAX_CHARS 64u      /* Max chars needed to print mask as CSV */
#define DIAG_LEVEL_ERROR 1             /* Error severity level */
#define DIAG_COL_START  1              /* Default column start for error reporting */


/* -----------------------------------------------------------
   Operand count constants (replace raw 0/1/2 with named values)
   ----------------------------------------------------------- */
#define OPS_ZERO    0  /* Instruction requires zero operands */
#define OPS_ONE     1   /* Instruction requires one operand  */
#define OPS_TWO     2   /* Instruction requires two operands */


/* -----------------------------------------------------------
   Opcode count (0..15 == MOV..STOP in CommandsTable enum)
   ----------------------------------------------------------- */
#define OPCODE_COUNT 16u   /* Total number of opcodes defined in CommandsTable */


/* -----------------------------------------------------------
   Rule descriptor: one per opcode
   ----------------------------------------------------------- */
typedef struct {
    const char *name;      /* Mnemonic string, e.g. "mov" */
    unsigned min_ops;      /* Minimum operands required (OPS_ZERO..OPS_TWO) */
    unsigned max_ops;      /* Maximum operands allowed (OPS_ZERO..OPS_TWO) */
    unsigned src_mask;     /* Allowed addressing modes for source operand */
    unsigned dst_mask;     /* Allowed addressing modes for destination operand */
} opcode_mode;


/* -----------------------------------------------------------
   Public tables / functions
   ----------------------------------------------------------- */

/* 
 * Global opcode validation table (implemented in op_modes.c).
 * Each entry describes the legal operand count and addressing 
 * modes for a specific opcode.
 */
extern const opcode_mode OPCODE_MODES[OPCODE_COUNT];

/* 
 * Validator: checks whether an opcode’s operand count and 
 * addressing modes are legal.
 * Returns 1 if an error was reported, 0 otherwise.
 */
int validate_modes_for_opcode(CommandsTable cmd,
                              int operand_count,
                              addr_mode src_mode,
                              addr_mode dst_mode,
                              const char *file_name,
                              long line_no,
                              const char *full_line_text);

#endif /* OP_MODES_H */
