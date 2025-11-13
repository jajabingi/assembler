/**
 * @file process_macros.h
 * @brief Macro preprocessing system for assembler
 * 
 * This header defines a macro preprocessing system that handles macro
 * definition and expansion before the main assembly process. Macros
 * allow code reuse and parameterization in assembly programs.
 */

#ifndef PROCESS_MACROS_H
#define PROCESS_MACROS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h> /* size_t */
#include "diag.h"


/* External validators from main assembler - check if names are reserved */
int is_reserved_mnemonic(const char *s);
int is_register_name(const char *s);

/* ========== ERROR CODES ========== */
/* All error codes follow MC_ prefix for Macro Compiler errors */
#define MC_LINE_TOO_LONG     "MC001"  /* Source line exceeds maximum length */
#define MC_NO_SPACE_AFTER    "MC002"  /* Missing space after 'mcro' keyword */
#define MC_NO_NAME           "MC003"  /* Macro name missing after 'mcro' */
#define MC_NAME_TOO_LONG     "MC004"  /* Macro name exceeds maximum length */
#define MC_NAME_BAD_SYNTAX   "MC005"  /* Invalid characters in macro name */
#define MC_NAME_RESERVED     "MC006"  /* Macro name conflicts with reserved word */
#define MC_NAME_DUP          "MC007"  /* Duplicate macro definition */
#define MC_GARBAGE_AFTER_END "MC008"  /* Extra text after 'mcroend' */
#define MC_FILE_OPEN         "MC009"  /* Cannot open input/output files */
#define MC_FILE_TOO_LONG     "MC010"  /* Too many lines in source file */
#define MC_UNTERMINATED_MACRO "MC_UNTERMINATED_MACRO"  /* EOF inside macro definition */
#define MC_FILE_RENAME       "MC_FILE_RENAME"  /* Cannot rename temp file to final */

/* ========== CONSTANTS ========== */
#define MACRO_NAME_MAX 31      /* Maximum characters in macro name */
#define MAX_SOURCE_LINES 10000 /* Maximum lines allowed in source file */
#define MAX_LINE_LEN 81         /**< Maximum characters per source line */


/* ========== MACRO DEFINITION STRUCTURE ========== */

/**
 * @brief Single macro definition
 * 
 * Represents a complete macro with its name and body content.
 * The body consists of multiple lines that will be expanded
 * when the macro is invoked.
 */
typedef struct {
    char *name;         /**< Macro name (dynamically allocated, null-terminated) */
    char **body;        /**< Array of body lines (each dynamically allocated) */
    size_t n_body;      /**< Current number of lines in body array */
    size_t cap_body;    /**< Allocated capacity of body array (for growth) */
} Macro;

/**
 * @brief Macro table container
 * 
 * Dynamic table storing all defined macros. Uses resizable arrays
 * for efficient storage and lookup of macro definitions.
 */
typedef struct {
    Macro *items;       /**< Dynamic array of Macro structures */
    size_t n_macros;    /**< Current number of macros in table */
    size_t cap_macros;  /**< Allocated capacity of items array (for growth) */
} MacroTable;

/* ========== TABLE MANAGEMENT ========== */

/**
 * @brief Initialize empty macro table
 * @param tbl Pointer to MacroTable structure to initialize
 * 
 * Sets up an empty macro table with zero macros and no allocated memory.
 * Must be called before using any other macro table functions.
 * 
 */
void init_macro_table(MacroTable *tbl);

/**
 * @brief Free all memory used by macro table
 * @param tbl Pointer to MacroTable to deallocate
 * 
 * Releases all dynamically allocated memory including:
 * - All macro names
 * - All macro body lines  
 * - Body arrays for each macro
 * - The main macro items array
 * 
 * After calling this function, the table is reset to empty state.
 * Safe to call multiple times on the same table.
 */
void free_macro_table(MacroTable *tbl);

/* ========== MACRO LOOKUP ========== */

/**
 * @brief Find macro by name in table
 * @param tbl Pointer to MacroTable to search
 * @param name Name of macro to find (case-sensitive)
 * @return Pointer to Macro if found, NULL if not found
 * 
 * Performs linear search through the macro table to find a macro
 * with the specified name. Search is case-sensitive.
 */
Macro *find_macro(const MacroTable *tbl, const char *name);

/* ========== MACRO CREATION ========== */

/**
 * @brief Add new macro definition to table
 * @param tbl Pointer to MacroTable to add to
 * @param name Name of new macro (will be copied internally)
 * @return Pointer to newly created Macro, or NULL on allocation failure
 * 
 * Creates a new macro with the specified name and empty body.
 * The name string is copied internally, so caller retains ownership
 * of the original string. The returned Macro can be used with
 * append_macro_line() to add body content.
 * 
 * If a macro with the same name already exists, behavior is undefined
 * (may create duplicate or overwrite).
 */
Macro *add_macro(MacroTable *tbl, const char *name);

/* ========== MACRO BODY BUILDING ========== */

/**
 * @brief Add line to macro body
 * @param m Pointer to Macro to modify
 * @param line Line of text to add (will be copied internally)
 * 
 * Appends a new line to the macro's body. The line string is copied
 * internally, so caller retains ownership. The body array is
 * automatically resized if needed.
 * 
 * Lines are stored in the order they are added and will be expanded
 * in the same order during macro invocation.
 * 
 */
void append_macro_line(Macro *m, const char *line);

/**
 * @brief Main macro preprocessing driver
 * @param in_path Path to input assembly file with macro definitions
 * @param tbl Pointer to MacroTable to populate with discovered macros
 * @return 0 on success, non-zero on error
 * 
 * Processes an assembly source file to extract macro definitions and
 * perform macro expansion. This function:
 * 
 * 1. **Reads the input file line by line**
 * 2. **Identifies macro definition blocks** (typically `macr`...`endmacr`)
 * 3. **Stores macro definitions** in the provided MacroTable
 * 4. **Expands macro invocations** by substituting macro body
 * 5. **Writes processed output** (implementation-dependent destination)
 * 
 */
 int process_macros(const char *in_path, MacroTable *tbl);
 
#endif