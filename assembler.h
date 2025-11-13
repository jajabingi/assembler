/**
 * @file assembler.h
 * @brief Core header file for a two-pass assembler implementation
 * 
 * This file defines constants, data structures, and function prototypes
 * for processing assembly language source files into machine code.
 */

#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include "diag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define INITIAL_IC_VAL 0
#define INITIAL_DC_VAL 0
#define LONG_MAX_L 100

/* ========== ASSEMBLER LIMITS ========== */
#define MAX_LINE_LEN 81         /**< Maximum characters per source line */
#define MAX_LABEL_LEN 31        /**< Maximum characters in a label */
#define IC_INIT_VALUE 100       /**< Initial instruction counter value */
#define MAX_CODE_IMAGE 255     /**< Maximum size of code image */

/* ========== SYNTAX CONSTANTS ========== */
#define COMMENT_CHAR            ';'    /**< Line comment delimiter */
#define OPERAND_DELIMITER       ','    /**< Separates operands in instructions */
#define IMMEDIATE_PREFIX        '#'    /**< Prefix for immediate addressing (#123) */

/* Register syntax */
#define REGISTER_MIN_ID_CHAR    '0'    /**< Minimum register ID */

/* Matrix addressing syntax */
#define MATRIX_BRACKET_OPEN     '['    /**< Opening bracket for matrix M[r1][r2] */
#define MATRIX_BRACKET_CLOSE    ']'    /**< Closing bracket for matrix M[r1][r2] */

/* Buffer sizes */
#define REGBUF_MAX              3      /**< Buffer size for register strings */

/* ========== ENCODING CONSTANTS ========== */
/** @brief 2-bit encoding values for addressing modes in machine code */
#define BITS_IMMEDIATE          0u     /**< Immediate addressing mode bits */
#define BITS_DIRECT             1u     /**< Direct addressing mode bits */
#define BITS_MATRIX             2u     /**< Matrix addressing mode bits */
#define BITS_REGISTER           3u     /**< Register addressing mode bits */

/** @brief Immediate value constraints (signed 8-bit) */
#define IMM8_MIN                (-128) /**< Minimum immediate value */
#define IMM8_MAX                (127)  /**< Maximum immediate value */

/** @brief Data value constraints */
#define DATA_MIN      (-128)           /**< Minimum data word value */
#define DATA_MAX      (127)            /**< Maximum data word value */

/* ========= Matrix-operand parsing (diagnostic variant) ========= */
#define AS_E_MAT_BRACKETS         "AS110"  /* missing or mismatched [ ] */
#define AS_E_MAT_EMPTY_INDEX      "AS111"  /* [] empty index */
#define AS_E_MAT_BETWEEN_BRACKETS "AS112"  /* junk/space between ][ */
#define AS_E_MAT_NON_REG          "AS113"  /* non-register used as index */
#define AS_E_MAT_BAD_REG          "AS114"  /* looks like register but invalid (rx, r8, r12...) */


#define REGISTER_PREFIX_LOWER   'r'
#define REGISTER_PREFIX_UPPER   'R'
#define REGISTER_MIN_ID_CHAR    '0'
#define REGISTER_MAX_ID_CHAR    '7'


#define COLUMN_AT(ptr, base, tok) ((base) + (int)((ptr) - (tok)))

/**
* @def IS_VALID_REGISTER_CHAR(c)
* @brief Test whether a character is an allowed register digit.
*
* Accepts characters in the range REGISTER_MIN_ID_CHAR..REGISTER_MAX_ID_CHAR
* (e.g., '0'..'7').
*
* @param c Character to test.
* @return Nonzero if valid; 0 otherwise.
*/
#define IS_VALID_REGISTER_CHAR(c) ((c) >= REGISTER_MIN_ID_CHAR && (c) <= REGISTER_MAX_ID_CHAR)


/**
* @def IS_REGISTER_PREFIX(c)
* @brief Test whether a character is a register prefix.
*
* Recognizes 'r' and 'R'.
*
* @param c Character to test.
* @return Nonzero if 'r' or 'R'; 0 otherwise.
*/
#define IS_REGISTER_PREFIX(c) ((c) == REGISTER_PREFIX_LOWER || (c) == REGISTER_PREFIX_UPPER)


/* ========================================
* Error types for better error categorization
* ======================================== */

/**
* @enum matrix_error_t
* @brief Parser error codes for matrix operands.
*
* Values:
* - MATRIX_ERROR_NONE — no error; parsing succeeded.
* - MATRIX_ERROR_NO_BRACKETS — operand lacks any '[' ... ']' pair.
* - MATRIX_ERROR_MISSING_CLOSE_BRACKET — first '[' lacks a matching ']'.
* - MATRIX_ERROR_MISSING_SECOND_OPEN — missing the second '['.
* - MATRIX_ERROR_MISSING_SECOND_CLOSE — second '[' lacks a matching ']'.
* - MATRIX_ERROR_EMPTY_LABEL — label slice is empty (zero-length).
* - MATRIX_ERROR_LABEL_TOO_LONG — label exceeds project limit (e.g., MAX_LABEL_LEN).
* - MATRIX_ERROR_EMPTY_INDEX — empty between brackets (e.g., LABEL[][r2]).
* - MATRIX_ERROR_INVALID_REGISTER — malformed register (e.g., r9, rA, r77).
* - MATRIX_ERROR_NON_REGISTER — text present but not a register token at all.
* - MATRIX_ERROR_JUNK_BETWEEN_BRACKETS — unexpected chars inside/between brackets.
*/
typedef enum {
        MATRIX_ERROR_NONE = 0,
        MATRIX_ERROR_NO_BRACKETS,
        MATRIX_ERROR_MISSING_CLOSE_BRACKET,
        MATRIX_ERROR_MISSING_SECOND_OPEN,
        MATRIX_ERROR_MISSING_SECOND_CLOSE,
        MATRIX_ERROR_EMPTY_LABEL,
        MATRIX_ERROR_LABEL_TOO_LONG,
        MATRIX_ERROR_EMPTY_INDEX,
        MATRIX_ERROR_INVALID_REGISTER,
        MATRIX_ERROR_NON_REGISTER,
        MATRIX_ERROR_JUNK_BETWEEN_BRACKETS
} matrix_error_t;



/* Structure to hold parsing results */
typedef struct {
    const char *label_start;
    const char *label_end;
    const char *reg1_start;
    const char *reg1_end;
    const char *reg2_start;
    const char *reg2_end;
    matrix_error_t error;
    const char *error_pos;
} matrix_parse_result_t;

/* Register validation result */
typedef enum {
    REG_VALID = 0,
    REG_INVALID_NOT_REGISTER,
    REG_INVALID_BAD_REGISTER
} register_validity_t;



 /* ========== COMMAND/DIRECTIVE ENUMERATION ========== */
 
 /**
 * @brief Assembly language commands and directives
 * 
 * Enumeration of all supported assembly language instructions
 * and assembler directives. Values correspond to opcode encodings
 * for instructions.
 */
typedef enum CommandsTable
{
    /* ---- Instruction Set ---- */
    MOV = 0,     /**< Move data between operands */
    CMP = 1,     /**< Compare two operands */
    ADD = 2,     /**< Add source to destination */
    SUB = 3,     /**< Subtract source from destination */
    NOT = 4,     /**< Bitwise NOT operation */
    CLR = 5,     /**< Clear operand to zero */
    LEA = 6,     /**< Load effective address */
    INC = 7,     /**< Increment operand by 1 */
    DEC = 8,     /**< Decrement operand by 1 */
    JMP = 9,     /**< Unconditional jump */
    BNE = 10,    /**< Branch if not equal */
    RED = 11,    /**< Read input to operand */
    PRN = 12,    /**< Print operand value */
    JSR = 13,    /**< Jump to subroutine */
    RTS = 14,    /**< Return from subroutine */
    STOP = 15,   /**< Halt program execution */
    
    /* ---- Assembler Directives ---- */
    DATA = 16,   /**< .data directive - declare integer data */
    STRING = 17, /**< .string directive - declare string data */
    MATRIX = 18, /**< .mat directive - declare matrix data */
    ENTRY = 19,  /**< .entry directive - mark symbol as entry point */
    EXTERN = 20, /**< .extern directive - declare external symbol */
    
    UNDEFINED = -1  /**< Unknown/invalid command */
} CommandsTable;

/* ========== DATA TYPES ========== */

/**
 * @brief ARE (Absolute/Relocatable/External) encoding types
 * Used to mark how addresses should be resolved during linking
 */
typedef enum {
    ARE_ABS = 0,    /**< Absolute address - no relocation needed */
    ARE_REL = 2,    /**< Relocatable address - add base address */
    ARE_EXT = 1     /**< External address - resolve from other files */
} are_t;

/**
 * @brief Addressing modes supported by the assembler
 * Determines how operands are interpreted and encoded
 */
typedef enum {
    ADDR_IMMEDIATE = 0,  /**< Immediate value (#123) */
    ADDR_DIRECT = 1,     /**< Direct label reference (LABEL) */
    ADDR_MATRIX = 2,     /**< Matrix addressing (M[r1][r2]) */
    ADDR_REGISTER = 3,   /**< Register addressing (r0-r7) */
    ADDR_NONE = -1       /**< No addressing mode (invalid) */
} addr_mode;

/**
 * @brief Boolean type for C90 compatibility
 */
typedef enum { FALSE = 0, TRUE = 1 } boolean;


/* ========== DATA STRUCTURES ========== */

/**
 * @brief ARE (Absolute/Relocatable/External) field structure
 * 
 * 2-bit field used in machine words to indicate address resolution type.
 * Packed structure to ensure exact bit layout.
 */
typedef struct are_type
{
    int are : 2;  /**< 2-bit ARE field */
} are_type;

/**
 * @brief Data word in the data image
 * 
 * Represents a single data item in the assembled program's data section.
 * Forms a linked list of all data declarations.
 */
typedef struct data_word
{
    int address;              /**< Memory address of this data word */
    unsigned short value;     /**< Data payload (integer, character, etc.) */
    struct data_word *next;   /**< Next data word in linked list */
} data_word;

/**
 * @brief Machine word in the code image
 * 
 * Represents a single machine instruction word with 8-bit payload
 * and 2-bit ARE field. Forms the final executable code image.
 */
typedef struct machine_word
{
    int address;              /**< Instruction counter address */
    char label[31];           /**< Associated label (if any) */
    unsigned int value : 8;   /**< 8-bit instruction payload */
    unsigned int are : 2;     /**< 2-bit ARE field */
    struct machine_word *next; /**< Next instruction in linked list */
} machine_word;

/**
 * @brief Entry symbol list node
 * 
 * Stores symbols declared with .entry directive.
 * These symbols are exported for use by other assembly files.
 */
typedef struct entry_list {
    char labels[31];          /**< Entry symbol name */
    int  addr;               /**< Symbol's address */
    struct entry_list * next; /**< Next entry in list */
} entry_list;

/**
 * @brief External symbol usage tracking
 * 
 * Tracks all addresses where an external symbol is referenced.
 * Used to generate relocation information.
 */
typedef struct extern_usage{
    int addr;                    /**< Address where external symbol is used */
    struct extern_usage * next;  /**< Next usage location */
} extern_usage;

/**
 * @brief External symbol list node
 * 
 * Stores symbols declared with .extern directive.
 * These symbols are resolved from other assembly files.
 */
typedef struct extern_list {
    char labels[31];              /**< External symbol name */
    struct extern_usage * addr_head; /**< List of usage addresses */
    struct extern_list * next;    /**< Next external symbol */
} extern_list;

/* ========== ADDRESSING MODES ========== */

/**
 * @brief Operand addressing modes
 * 
 * Defines how operands in instructions should be interpreted
 * and encoded in the machine code.
 */
typedef enum
{
    OPERAND_TYPE_IMMEDIATE = 0,  /**< Immediate value (#123) */
    OPERAND_TYPE_DIRECT = 1,     /**< Direct memory reference (LABEL) */
    OPERAND_TYPE_MATRIX = 2,     /**< Matrix addressing (M[r1][r2]) */
    OPERAND_TYPE_REGISTER = 3    /**< Register reference (r0-r7) */
} operand_type_t;

/* ========== OPCODE INFORMATION ========== */

/**
 * @brief Opcode metadata structure
 * 
 * Contains information about each instruction type including
 * encoding values and operand requirements.
 */
typedef struct
{
    char name[8];          /**< Instruction mnemonic (e.g., "mov", "add") */
    int opcode_value;      /**< Opcode field value for encoding */
    int funct_value;       /**< Function field value (if applicable) */
    int operands_required; /**< Number of operands this instruction takes */
} opcode_info_t;

/* ========== SYMBOL TABLE ========== */

/**
 * @brief Symbol classification types
 * 
 * Categories of symbols that can be stored in the symbol table.
 */
typedef enum
{
    DATA_SYMBOL,      /**< Symbol refers to data (.data, .string, .mat) */
    CODE_SYMBOL,      /**< Symbol refers to instruction label */
    EXTERNAL_SYMBOL,  /**< Symbol declared as .extern */
    ENTRY_SYMBOL,     /**< Symbol declared as .entry */
    NONE_SYMBOL       /**< Undefined or no symbol type */
} symbol_type;

/**
 * @brief Symbol table entry node
 * 
 * Single entry in the symbol table linked list.
 * Stores symbol name, value/address, and classification.
 */
typedef struct symbol_table
{
    char *key;                   /**< Symbol name (dynamically allocated) */
    long value;                  /**< Symbol value/address */
    symbol_type type;            /**< Symbol classification */
    struct symbol_table *next;   /**< Next symbol in table */
} symbol_table;

/**
 * @brief Symbol table container
 * 
 * Wrapper structure containing the symbol table linked list
 * and metadata about the table.
 */
typedef struct
{
    symbol_table *head;  /**< Head of symbol table linked list */
    int size;           /**< Number of symbols in table */
} table;



/* ========== SYMBOL TABLE FUNCTIONS ========== */

/**
 * @brief Add an item to the symbol table
 * @param tab Pointer to symbol table
 * @param key Symbol name
 * @param value Symbol address/value
 * @param type Type of symbol (label, data, etc.)
 * @return Success/failure code
 */
int add_table_item(table *tab, const char *key, long value, symbol_type type);

/**
 * @brief Print contents of symbol table for debugging
 * @param tab Pointer to symbol table to print
 */
void print_symbol_table(const table *tab);

/* ========== ENTRY/EXTERNAL SYMBOL MANAGEMENT ========== */

/**
 * @brief Add a symbol to the entry list (.entry directive)
 * @param head Pointer to head of entry list
 * @param label Name of entry symbol
 * @param addr Address of the symbol
 */
void add_entry(entry_list **head, const char *label, int addr);

/**
 * @brief Add a symbol to the external list (.extern directive)
 * @param head Pointer to head of external list
 * @param label Name of external symbol
 */
void add_extern(extern_list **head, const char *label);

/**
 * @brief Create new entry list node
 * @param label Symbol name
 * @param addr Symbol address
 * @return Pointer to new entry node
 */
entry_list *create_entry_node(const char *label, int addr);

/**
 * @brief Create new external list node
 * @param label Symbol name
 * @return Pointer to new external node
 */
extern_list *create_extern_node(const char *label);

/* ========== DATA DIRECTIVE PROCESSING ========== */

/**
 * @brief Process .data directive (integer values)
 * @param line Source line containing .data directive
 * @param DC Pointer to data counter
 * @param data_image Pointer to data image linked list
 * @param IC Pointer to instruction counter
 * @param line_no Line number for error reporting
 * @return Success/failure code
 */
int process_data_directive(char line[], long *DC, data_word **data_image, long *IC, long line_no);

/**
 * @brief Process .string directive (character strings)
 * @param line Source line containing .string directive
 * @param DC Pointer to data counter
 * @param data_image Pointer to data image linked list
 * @param IC Pointer to instruction counter
 * @param line_no Line number for error reporting
 * @return Success/failure code
 */
int process_string_directive(char line[], long *DC, data_word **data_image, long *IC, long line_no);

/**
 * @brief Process .mat directive (matrix definitions)
 * @param line Source line containing .mat directive
 * @param DC Pointer to data counter
 * @param data_image Pointer to data image linked list
 * @param IC Pointer to instruction counter
 * @param line_no Line number for error reporting
 * @return Success/failure code
 */
int process_matrix_directive(char line[], long *DC, data_word **data_image, long *IC, long line_no);

/**
 * @brief Generic processor for data/string/mat directives
 * @param cmd Command type (DATA, STRING, or MAT)
 * @param line Source line
 * @param DC Data counter
 * @param data_image Data image
 * @param IC Instruction counter
 * @param line_no Line number
 * @param file_am this the name of the file
 * @return Success/failure code
 */
int process_data_string_mat(CommandsTable cmd, 
                            char line[], 
                            long *DC, 
                            data_word **data_image, 
                            long *IC, 
                            long line_no ,
                            const char *file_am);



/* ========== DATA STRUCTURE MANAGEMENT ========== */

/**
 * @brief Create a new data word node
 * @param value Integer value to store
 * @param addr Address of the data word
 * @return Pointer to new data word
 */
data_word *create_data_word(int value, int addr);

/**
 * @brief Add data word to end of linked list
 * @param head Pointer to head of data word list
 * @param new_node New data word to append
 */
void add_data_word_to_end(data_word **head, data_word *new_node);

/* ========== CODE GENERATION FUNCTIONS ========== */

/**
 * @brief Determine addressing mode from operand string
 * @param opnd Operand string (e.g., "#123", "r5", "LABEL")
 * @return Addressing mode enumeration
 */
addr_mode get_addr_method(const char *opnd);

/**
 * @brief Convert addressing mode to 2-bit encoding
 * @param m Addressing mode
 * @return 2-bit encoding value
 */
unsigned to2bits(addr_mode m);


/**
 * @brief Split instruction operands into source and destination
 * @param text Operand text to parse
 * @param out_src Output pointer for source operand
 * @param out_dst Output pointer for destination operand
 * @return Number of operands found
 */
int split_operands(char *text, char **out_src, char **out_dst);

/**
 * @brief Process complete command instruction
 * @param cmd Command type
 * @param line Source line
 * @param IC Instruction counter
 * @param image Machine code image
 * @param symtab_ptr Symbol table
 * @param file_name Source file name
 * @param line_no Line number
 * @return Success/failure code
 */
int process_commands_words(CommandsTable cmd, char line[], long *IC, machine_word **image, 
                          table *symtab_ptr, const char *file_name, long line_no);

/* ========== MACHINE CODE EMISSION ========== */


/**
 * @brief Emit address word with ARE encoding
 * @param symbol Symbol name being referenced
 * @param addr8 8-bit address value
 * @param are ARE type
 * @param IC Instruction counter
 * @param img Machine code image
 */
void emit_addr_word(char symbol[], unsigned addr8, are_t are, long *IC, machine_word *img[]);

/**
 * @brief Emit immediate value word
 * @param val Immediate value
 * @param IC Instruction counter
 * @param img Machine code image
 */
void emit_imm_word(int val, long *IC, machine_word *img[]);

/**
 * @brief Emit register encoding for single destination register
 * @param r Register number (0-7)
 * @param IC Instruction counter
 * @param img Machine code image
 */
void emit_regcode_single_dest(int r, long *IC, machine_word *img[]);

/**
 * @brief Emit register encoding for single source register
 * @param r Register number (0-7)
 * @param IC Instruction counter
 * @param img Machine code image
 */
void emit_regcode_single_src(int r, long *IC, machine_word *img[]);

/**
 * @brief Emit register encoding for register pair
 * @param rA First register number
 * @param rB Second register number
 * @param IC Instruction counter
 * @param img Machine code image
 */
void emit_regcode_pair(int rA, int rB, long *IC, machine_word *img[]);

/* ========== PARSING UTILITIES ========== */

/**
 * @brief Parse 8-bit immediate value from token
 * @param tok Token string containing immediate value
 * @param out Output pointer for parsed value
 * @return Success/failure code
 */
int parse_imm8(const char *tok, int *out);

/**
 * @brief Extract register ID from register token
 * @param tok Register token (e.g., "r5")
 * @return Register number (0-7) or -1 if invalid
 */
int reg_id(const char *tok);


/* ========== FIRST PASS UTILITIES ========== */

/**
 * @brief Check if line contains a label definition
 * @param line Source line to check
 * @param symbol_table Symbol table for adding label
 * @param label_name Output pointer for label name
 * @return True if line defines a label
 */
int is_label_definition(const char *line_start,
                        const char *full_line,
                        const char *file_name,
                        long line_no,
                        char **label_out);
/**
 * @brief Determine command type from source line
 * @param line Source line
 * @param file_name File name for error reporting
 * @param line_no Line number for error reporting
 * @return Command type enumeration
 */
CommandsTable get_command_type(const char *line, const char *file_name, long line_no);

/* ========== SECOND PASS & OUTPUT ========== */

/**
 * @brief Relocate data addresses after first pass completion
 * @param symtab Symbol table
 * @param data_img Data image linked list
 * @param finalIC Final instruction counter value
 */
void relocate_data(table *symtab, data_word *data_img, long finalIC);

/**
 * @brief Execute second pass of assembly process
 * @param stem Base filename (without extension)
 * @param symtab Symbol table from first pass
 * @param code_head Machine code image
 * @param data_head Data image
 * @param IC_final Final instruction counter
 * @param DC_final Final data counter
 * @param ext_list External symbols list
 * @param ent_list Entry symbols list
 * @return Success/failure code
 */
int second_pass(const char *stem, table *symtab, machine_word *code_head, data_word *data_head,
                long IC_final, long DC_final, extern_list *ext_list, entry_list *ent_list);


/**
 * @brief Create duplicate of string 
 * @param s String to duplicate
 * @return Pointer to duplicated string
 */
char *my_strdup(const char *s);



/* ========== DEBUG/UTILITY FUNCTIONS ========== */

/**
 * @brief Print data word list for debugging
 * @param head Head of data word list
 */
void print_data_word_list(const data_word *head);

/**
 * @brief Print machine word list for debugging
 * @param head Head of machine word list
 */
void print_machine_word_list(const machine_word *head);

/**
 * @brief Print external symbols list
 * @param head Head of external list
 */
void print_extern_list(const extern_list *head);

/**
 * @brief Print entry symbols list
 * @param head Head of entry list
 */
void print_entry_list(const entry_list *head);

/**
 * @brief Print external symbol usage
 * @param head Head of external usage list
 */
void print_extern_usage(const extern_usage *head);

/**
 * @brief Print binary representation of value
 * @param value Value to print
 * @param bits Number of bits to display
 */
void print_binary(unsigned int value, int bits);



int split_matrix_ex(const char *tok,
                    char *out_label, size_t L,
                    char regA[REGBUF_MAX], char regB[REGBUF_MAX],
                    const char *file_am, long line_no,
                    const char *line_text, int col_base);






/** 
* Treats [start, *end) as a slice and moves *end leftwards while the character before
* it is whitespace. Leading whitespace is not modified.
*
* @param start Pointer to the beginning of the slice (unchanged).
* @param end In/out: one-past-the-last character on entry; updated on return to
* exclude trailing spaces/tabs.
*/
 void trim_whitespace(const char **start, const char **end);


/**
* @brief Validate that a slice encodes a register token of the form [rR][0-7].
*
* @param start Pointer to the first character of the candidate slice.
* @param end One-past-the-last character of the candidate slice.
* @return REG_VALID if the slice is exactly a register; otherwise a reason code.
*/
 register_validity_t validate_register_slice(const char *start, const char *end);


/**
* @brief Safely copy a [start, end) slice into a destination buffer.
*
* Copies up to cap-1 bytes and always writes a terminating '\\0'.
*
* @param start Slice start.
* @param end Slice end (one past the last character to copy).
* @param dst Destination character buffer.
* @param cap Capacity of \p dst, including the NUL terminator.
*/
 void copy_slice_safe(const char *start, const char *end, char *dst, size_t cap);


/**
* @brief Parse a matrix operand token of the form LABEL[rX][rY].
*
* Performs bracket discovery, extracts label and register slices, trims as needed,
* and validates register syntax. On failure, returns a result with an appropriate
* error code and an error_pos pointer suitable for column mapping via COLUMN_AT.
*
* @param operand Pointer to the start of the operand token (NUL-terminated string).
* @return A matrix_parse_result_t with slice pointers and error info.
* @warning Returned pointers reference the original operand buffer; keep it alive
* while using the slices.
*/
 matrix_parse_result_t parse_matrix_operand(const char *operand);


int is_register_token(const char *token);



/**
* @brief Emit a human-readable diagnostic for a matrix parsing error.
*
* Maps error codes and positions to file/line/column and may render a caret span using
* the provided line text.
*
* @param result Parse result (carries error and error_pos).
* @param filename Current file name/path for diagnostics.
* @param line_number 1-based source line number.
* @param line_text Full source line contents (for context underline).
* @param column_base Column where the token begins (used with COLUMN_AT).
* @param token Pointer to the token start within the line.
*/

/* New (correct) */
void report_matrix_error(const matrix_parse_result_t *result,
                         const char *filename,
                         long line_number,
                         const char *line_text,
                         int column_base,
                         const char *token);

#endif