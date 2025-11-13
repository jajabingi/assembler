/* diag.c - diagnostic reporting (errors, warnings, info) */

#include "diag.h"
#include <string.h>  /* strlen is used for line highlighting */

/* Global diagnostic state (shared instance) */

/* Initialize a diagnostic context.
 * - d: pointer to diag struct
 * - stream: output file  */
void diag_init(diag *d, FILE *stream)
{
    if (!d || !stream) return;      
    d->error_count = 0;
    d->stream      = stream;         /* may also allow NULL => silent mode */
}

/* Print a diagnostic header before the actual message.
 * Format: file:line:col: severity [code]  */
void print_header(FILE *fp, diag_severity sev,
                         const char *code,
                         const char *file, long line, int col)
{
    fprintf(fp, "%s:%ld:%d: ", file ? file : "<input>", line, col);
    if (sev == DIAG_ERROR) fprintf(fp, "error: ");
    else                   fprintf(fp, "info: ");
    if (code && *code)     fprintf(fp, "[%s] ", code);
}

/* Core diagnostic function with a va_list (shared by info/error/report).
 * Prints header + formatted message + optional source underline. */
void diag_reportv(diag *d, diag_severity sev, const char *code,
                  const char *file, long line, int col,
                  const char *line_text, int col_start, int col_end,
                  const char *fmt, va_list args)
{
    FILE *fp = (d && d->stream) ? d->stream : stdout;

    /* Increment error counter only for real errors */
    if (sev == DIAG_ERROR && d) d->error_count++;

    /* Print header and formatted message */
    print_header(fp, sev, code, file, line, col);
    vfprintf(fp, fmt, args);
    fputc('\n', fp);

    /* If we have line text, print it and underline the error span */
    if (line_text && *line_text && col_start > 0 && col_end >= col_start) {
        int i; 
        fputs("  ", fp);
        fputs(line_text, fp);
        if (line_text[strlen(line_text) - 1] != '\n') fputc('\n', fp);

        fputs("  ", fp);
        for (i = 1; i < col_start; ++i) fputc(' ', fp);  /* indent until error */
        for (i = col_start; i <= col_end; ++i) fputc('^', fp);  /* underline */
        fputc('\n', fp);
    }
}

/* Convenience wrapper for "info"-level diagnostic messages. */
void diag_info(diag *d, const char *code,
               const char *file, long line, int col,
               const char *line_text, int cs, int ce,
               const char *fmt, ...)
{
    va_list args; va_start(args, fmt);
    diag_reportv(d, DIAG_INFO, code, file, line, col, line_text, cs, ce, fmt, args);
    va_end(args);
}

/* Convenience wrapper for "error"-level diagnostic messages.
 * Also increments the error counter. */
void diag_error(diag *d, const char *code,
                const char *file, long line, int col,
                const char *line_text, int cs, int ce,
                const char *fmt, ...)
{
    va_list args; va_start(args, fmt);
    diag_reportv(d, DIAG_ERROR, code, file, line, col, line_text, cs, ce, fmt, args);
    va_end(args);
}

/* General-purpose wrapper that allows caller to specify severity. */
void diag_report(diag *d, diag_severity sev, const char *code,
                 const char *file, long line, int col,
                 const char *line_text, int col_start, int col_end,
                 const char *fmt, ...)
{
    va_list args; va_start(args, fmt);
    diag_reportv(d, sev, code, file, line, col, line_text, col_start, col_end, fmt, args);
    va_end(args);
}
