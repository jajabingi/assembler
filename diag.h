/**
 * @file diag.h
 * @brief Diagnostic and error reporting system for assembler
 * 
 * This header provides a comprehensive diagnostic system for reporting
 * errors, warnings, and informational messages during assembly processing.
 * Supports detailed error location reporting with line/column information.
 */

#ifndef DIAG_H
#define DIAG_H
#include <stdio.h>
#include <stdarg.h>

/* ========== DIAGNOSTIC SEVERITY LEVELS ========== */

/**
 * @brief Diagnostic message severity levels
 * 
 * Defines the importance/severity of diagnostic messages.
 * Only errors are counted in the error counter.
 */
typedef enum {
    DIAG_INFO  = 0,  /**< Informational message (not counted as error) */
    DIAG_ERROR = 1   /**< Error message (increments error counter) */
} diag_severity;

/* ========== DIAGNOSTIC SYSTEM STATE ========== */

/**
 * @brief Diagnostic system context structure
 * 
 * Maintains state for the diagnostic reporting system including
 * error counting and output stream configuration.
 */
typedef struct diag {
    int   error_count;  /**< Total number of errors reported (INFO messages not counted) */
    FILE *stream;       /**< Output stream for diagnostic messages */
} diag;

/**
 * @brief Global diagnostic instance
 * 
 * Single global instance of the diagnostic system.
 * Must be defined once in a .c file and initialized before use.
 */

/* ========== INITIALIZATION ========== */

/**
 * @brief Initialize diagnostic system
 * @param d Pointer to diagnostic structure to initialize
 * @param stream Output stream for messages 
 * 
 * Sets up the diagnostic system with specified output stream
 * and resets error counter to zero.
 */
void diag_init(diag *d, FILE *stream);

/* ========== CORE REPORTING FUNCTIONS ========== */

/**
 * @brief Core diagnostic reporting function (va_list variant)
 * @param d Diagnostic system instance
 * @param sev Message severity level
 * @param code Error/diagnostic code identifier
 * @param file Source filename where issue occurred
 * @param line Line number in source file (1-based)
 * @param col Column number in line (1-based)
 * @param line_text Full text of the problematic source line
 * @param col_start Starting column for error highlighting (1-based)
 * @param col_end Ending column for error highlighting (1-based)
 * @param fmt Printf-style format string for diagnostic message
 * @param args Variable argument list for format string
 * 
 * Low-level reporting function that takes a va_list for arguments.
 * Provides detailed location information with line highlighting.
 * Automatically increments error_count for DIAG_ERROR severity.
 */
void diag_reportv(diag *d, diag_severity sev, const char *code,
                  const char *file, long line, int col,
                  const char *line_text, int col_start, int col_end,
                  const char *fmt, va_list args);

/**
 * @brief Main diagnostic reporting function (variadic)
 * @param d Diagnostic system instance
 * @param sev Message severity level
 * @param code Error/diagnostic code identifier
 * @param file Source filename where issue occurred
 * @param line Line number in source file (1-based)
 * @param col Column number in line (1-based)  
 * @param line_text Full text of the problematic source line
 * @param col_start Starting column for error highlighting (1-based)
 * @param col_end Ending column for error highlighting (1-based)
 * @param fmt Printf-style format string for diagnostic message
 * @param ... Variable arguments for format string
 * 
 * Convenience wrapper around diag_reportv() that accepts variable arguments
 * directly. This is the primary interface for reporting diagnostics.
 * 
 * Example usage:
 * @code
 * diag_report(&g_diag, DIAG_ERROR, "E001", "prog.asm", 42, 15,
 *            "    mov r1, #256", 12, 16, 
 *            "Immediate value %d out of range [%d, %d]", 256, -128, 127);
 * @endcode
 */
void diag_report(diag *d, diag_severity sev, const char *code,
                 const char *file, long line, int col,
                 const char *line_text, int col_start, int col_end,
                 const char *fmt, ...);

/* ========== CONVENIENCE WRAPPERS ========== */

/**
 * @brief Report informational diagnostic message
 * @param d Diagnostic system instance
 * @param code Diagnostic code identifier (e.g., "I001")
 * @param file Source filename
 * @param line Line number (1-based)
 * @param col Column number (1-based)
 * @param line_text Source line text
 * @param cs Starting column for highlighting (1-based)
 * @param ce Ending column for highlighting (1-based)
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string
 * 
 * Convenience wrapper for reporting informational messages.
 * Automatically sets severity to DIAG_INFO.
 * Does not increment error counter.
 * 
 * Use for non-critical notifications, warnings, or debug information.
 */
void diag_info(diag *d, const char *code, const char *file, long line, int col,
               const char *line_text, int cs, int ce, const char *fmt, ...);

/**
 * @brief Report error diagnostic message
 * @param d Diagnostic system instance  
 * @param code Error code identifier (e.g., "E001", "E042")
 * @param file Source filename where error occurred
 * @param line Line number (1-based)
 * @param col Column number (1-based)
 * @param line_text Full source line text
 * @param cs Starting column for error highlighting (1-based)
 * @param ce Ending column for error highlighting (1-based)
 * @param fmt Printf-style format string for error description
 * @param ... Variable arguments for format string
 * 
 * Convenience wrapper for reporting error messages.
 * Automatically sets severity to DIAG_ERROR and increments error counter.
 * 
 * Use for syntax errors, semantic errors, and other critical issues
 * that prevent successful assembly.
 * 
 * Example usage:
 * @code
 * diag_error(&g_diag, "E003", filename, line_num, col_pos,
 *           source_line, start_col, end_col,
 *           "Undefined symbol '%s'", symbol_name);
 * @endcode
 */
void diag_error(diag *d, const char *code, const char *file, long line, int col,
                const char *line_text, int cs, int ce, const char *fmt, ...);

#endif