/**
 * @file first_pass.h
 * @brief First pass assembler definitions and data structures
 * 
 * This header defines the core data structures and function prototypes
 * for the first pass of the two-pass assembler. The first pass builds
 * the symbol table and processes directives.
 */

#ifndef FIRST_PASS_H
#define FIRST_PASS_H

#include "diag.h"
#include "assembler.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

/* ========== UTILITY MACROS ========== */

/**
 * @brief Skip whitespace characters in a string
 * @param line String to process
 * @param i Index variable to advance
 * 
 * Advances index i until a non-whitespace character is found
 * or end of string is reached.
 */
#define MOVE_TO_NOT_WHITE(line, i) while ((line)[(i)] && isspace((unsigned char)(line)[(i)])) {(i)++;}

/* ========== CONSTANTS ========== */
#define TOK_MAX_LEN 32  /**< Maximum length of token (mnemonic/directive), excluding '\0' */
#define OB_SUM_LIMIT 255  /* your rule: IC + DC must be < 255 */



/* --- File extensions --- */
#define FILE_EXT_AM   ".am"

/* --- Defaults --- */
#define DEFAULT_LINE_NO  1
#define DEFAULT_COL      1

/* --- Memory/Image limits --- */
#define MAX_MEMORY_WORDS 255


/* ========== FUNCTION PROTOTYPES ========== */

/* ========== VALIDATION HELPERS ========== */

/**
 * @brief Check if string is a valid register name
 * @param s String to check (e.g., "r0", "r7", "R3")
 * @return 1 if valid register name, 0 otherwise
 * 
 * Validates register syntax: 'r' or 'R' followed by digit 0-7
 */
int is_register_name(const char *s);

/**
 * @brief Check if string is a valid alphanumeric label
 * @param s String to validate
 * @return 1 if valid label, 0 otherwise
 * 
 * Valid labels start with letter, contain only letters/digits/underscores
 */
int is_alpha_num_label(const char *s);

/**
 * @brief Case-insensitive string comparison
 * @param a First string
 * @param b Second string  
 * @return 1 if strings are equal (ignoring case), 0 otherwise
 */
int ieq(const char *a, const char *b);

/**
 * @brief Check if token is a reserved mnemonic or directive
 * @param tok Token to check
 * @return 1 if reserved word, 0 otherwise
 * 
 * Prevents use of instruction names and directives as labels
 */
int is_reserved_mnemonic(const char *tok);

/* ========== DIRECTIVE HANDLERS ========== */

/**
 * @brief Process .extern directive
 * @param line Source line containing .extern directive
 * @param ext_list Pointer to external symbols list
 * 
 * Parses .extern directive and adds symbols to external list
 */
void handle_extern(char line[], extern_list **ext_list);

/**
 * @brief Process .entry directive  
 * @param line Source line containing .entry directive
 * @param ent_list Pointer to entry symbols list
 * 
 * Parses .entry directive and adds symbols to entry list
 */
void handle_entry(char line[], entry_list **ent_list);


/* ========== MAIN FIRST PASS FUNCTION ========== */

/**
 * @brief Execute first pass of assembly process
 * @param file_stem Base filename without extension
 * @param symbol_table Symbol table to populate
 * @param data_img Output pointer for data image
 * @param ic_image Output pointer for instruction image
 * @param IC_out Output pointer for final instruction counter
 * @param DC_out Output pointer for final data counter
 * @param ext_list Output pointer for external symbols list
 * @param ent_list Output pointer for entry symbols list
 * @return Success (0) or error code
 * 
 * Main function for first pass processing:
 * - Opens and reads source file line by line
 * - Builds symbol table with labels and their addresses
 * - Processes .data, .string, .mat directives
 * - Collects .entry and .extern declarations
 * - Calculates instruction and data counter values
 * - Validates syntax and reports errors
 */
int first_pass(const char *file_stem,
               table *symbol_table,
               data_word **data_img,
               machine_word **ic_image,
               long *IC_out,
               long *DC_out,
               extern_list **ext_list,
               entry_list **ent_list);




#endif /* FIRST_PASS_H */