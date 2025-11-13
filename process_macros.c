/**
 * @file process_macros.c
 * @brief Macro preprocessor implementation for assembler
 * 
 * This file implements a complete macro preprocessing system that:
 * 1. Reads .as assembly files
 * 2. Processes macro definitions (mcro...mcroend blocks)
 * 3. Expands macro invocations
 * 4. Outputs processed .am files ready for main assembler
 */

#include "process_macros.h"
static diag g_diag = {0, NULL};
/* ========== UTILITY FUNCTIONS ========== */

/**
 * @brief Safe string duplication with error handling
 * @param s String to duplicate (can be NULL)
 * @return Dynamically allocated copy of string
 * 
 * Creates a copy of the string in dynamic memory. Exits program
 * on allocation failure rather than returning NULL.
 */
char *str_duplicate(const char *s)
{
    size_t len;
    char *p;
    
    if (!s) return NULL;                    /* Handle NULL input gracefully */
    
    len = strlen(s) + 1;                    /* Include space for null terminator */
    p = (char*)malloc(len);
    
    if (!p) { 
        perror("malloc failed in str_duplicate"); 
        exit(EXIT_FAILURE);                 /* Fatal error - cannot continue */
    }
    
    memcpy(p, s, len);                      /* Copy including null terminator */
    return p;
}

/**
 * @brief Remove leading whitespace from string
 * @param s String to trim (modified in-place via return pointer)
 * @return Pointer to first non-whitespace character in s
 * 
 * Does not modify the original string, just returns a pointer
 * into it past any leading whitespace.
 */
char *ltrim(char *s) { 
    while (*s && isspace((unsigned char)*s)) s++; 
    return s; 
}

/**
 * @brief Remove trailing whitespace from string
 * @param s String to trim (modified in-place)
 * 
 * Modifies the string by replacing trailing whitespace with null terminators.
 */
void rtrim(char *s) {
    char *e;
    if (!s || !*s) return;                  /* Handle empty/NULL strings */
    
    e = s + strlen(s) - 1;                  /* Start from last character */
    while (e >= s && isspace((unsigned char)*e)) 
        *e-- = '\0';                        /* Replace whitespace with null */
}

/**
 * @brief Extract first whitespace-delimited token from line
 * @param line Source line to parse
 * @param buf Output buffer for token (must be large enough)
 * 
 * Skips leading whitespace, then copies characters until next whitespace
 * or end of string. Null-terminates the result.
 */
void first_token(const char *line, char *buf)
{
    /* Skip leading whitespace */
    while (*line && isspace((unsigned char)*line)) line++;
    
    /* Copy non-whitespace characters */
    while (*line && !isspace((unsigned char)*line)) 
        *buf++ = *line++;
    
    *buf = '\0';                            /* Null-terminate result */
}

/**
 * @brief Validate macro name syntax
 * @param name Proposed macro name to validate
 * @return 1 if valid, 0 if invalid
 * 
 * Valid macro names:
 * - Start with a letter (a-z, A-Z)
 * - Followed by any combination of letters, digits, underscores
 * - Cannot be empty
 */
int is_valid_macro_name(const char *name)
{
    const unsigned char *p = (const unsigned char*)name;
    
    if (!p || !*p) return 0;                /* Reject NULL or empty strings */
    
    if (!isalpha(*p)) return 0;             /* Must start with letter */
    
    p++;                                    /* Check remaining characters */
    while (*p) {
        if (!(isalnum(*p) || *p == '_')) return 0;  /* Only alphanumeric + underscore */
        p++;
    }
    return 1;                               /* All checks passed */
}

/**
 * @brief Extract macro name from "mcro <name>" line
 * @param trimmed_line_after_mcro Line with leading/trailing whitespace removed
 * @return Pointer to macro name within the line, or NULL if missing
 * 
 * Parses a line that starts with "mcro" and extracts the macro name.
 * The returned pointer points into the original line buffer.
 */
char *get_macro_name(char *trimmed_line_after_mcro)
{
    char *p = trimmed_line_after_mcro;
    
    /* Skip the "mcro" token itself */
    while (*p && !isspace((unsigned char)*p)) p++;
    
    /* Skip whitespace after "mcro" - we require at least one space */
    while (*p && isspace((unsigned char)*p)) p++;
    
    if (*p == '\0') return NULL;            /* No name found after mcro */
    
    rtrim(p);                               /* Remove trailing whitespace from name */
    return p;                               /* Name starts here */
}

/* ========== MACRO TABLE MANAGEMENT ========== */

/**
 * @brief Initialize empty macro table
 * @param tbl Table to initialize
 */
void init_macro_table(MacroTable *tbl)
{
    tbl->items = NULL; 
    tbl->n_macros = 0; 
    tbl->cap_macros = 0;
}

/**
 * @brief Free all memory used by macro table
 * @param tbl Table to deallocate
 */
void free_macro_table(MacroTable *tbl)
{
    size_t i, j;
    
    /* Free each macro */
    for (i = 0; i < tbl->n_macros; i++) {
        Macro *m = &tbl->items[i];
        
        free(m->name);                      /* Free macro name */
        
        /* Free all body lines */
        for (j = 0; j < m->n_body; j++) 
            free(m->body[j]);
            
        free(m->body);                      /* Free body array */
    }
    
    free(tbl->items);                       /* Free main array */
    
    /* Reset table to empty state */
    tbl->items = NULL; 
    tbl->n_macros = tbl->cap_macros = 0;
}

/**
 * @brief Find macro by name in table
 * @param tbl Table to search
 * @param name Name to find (case-sensitive)
 * @return Pointer to Macro if found, NULL otherwise
 */
Macro *find_macro(const MacroTable *tbl, const char *name)
{
    size_t i;
    
    /* Linear search through all macros */
    for (i = 0; i < tbl->n_macros; i++)
        if (strcmp(tbl->items[i].name, name) == 0)
            return &tbl->items[i];
            
    return NULL;                            /* Not found */
}

/**
 * @brief Add new macro to table
 * @param tbl Table to add to
 * @param name Name of new macro (will be copied)
 * @return Pointer to new Macro structure
 */
Macro *add_macro(MacroTable *tbl, const char *name)
{
    Macro *m;
    
    /* Grow array if needed */
    if (tbl->n_macros == tbl->cap_macros) {
        size_t new_cap = tbl->cap_macros ? tbl->cap_macros * 2 : 4;  /* Double or start with 4 */
        Macro *ni = (Macro*)realloc(tbl->items, new_cap * sizeof(Macro));
        
        if (!ni) { 
            perror("realloc failed in add_macro"); 
            exit(EXIT_FAILURE); 
        }
        
        tbl->items = ni; 
        tbl->cap_macros = new_cap;
    }
    
    /* Initialize new macro */
    m = &tbl->items[tbl->n_macros++];
    m->name = str_duplicate(name);          /* Copy name */
    m->body = NULL;                         /* Empty body initially */
    m->n_body = 0; 
    m->cap_body = 0;
    
    return m;
}

/**
 * @brief Add line to macro body
 * @param m Macro to modify
 * @param line Line to add (will be copied)
 */
void append_macro_line(Macro *m, const char *line)
{
    /* Grow body array if needed */
    if (m->n_body == m->cap_body) {
        size_t nc = m->cap_body ? m->cap_body * 2 : 8;    /* Double or start with 8 */
        char **nb = (char**)realloc(m->body, nc * sizeof(char*));
        
        if (!nb) { 
            perror("realloc failed in append_macro_line"); 
            exit(EXIT_FAILURE); 
        }
        
        m->body = nb; 
        m->cap_body = nc;
    }
    
    /* Add the line (copy it) */
    m->body[m->n_body++] = str_duplicate(line);
}

/* ========== MAIN MACRO PROCESSING FUNCTION ========== */

/**
 * @brief Main macro preprocessing driver
 * @param in_path Base path for input file (without extension)
 * @param tbl Macro table to populate
 * @return 0 on success, -1 on error
 * 
 * Processing flow:
 * 1. Read <in_path>.as file
 * 2. Process macro definitions and expansions
 * 3. Write result to <in_path>.am file
 * 4. Use temporary file to ensure atomicity
 */
int process_macros(const char *in_path, MacroTable *tbl)
{
    /* File paths */
    char file_am[MAX_LINE_LEN];             /* Output: <path>.am */
    char file_as[MAX_LINE_LEN];             /* Input: <path>.as */
    char tmp_am[MAX_LINE_LEN + 8];          /* Temp: <path>.am.tmp */
    
    FILE *fin = NULL, *fout = NULL;         /* Input and output files */
    
    /* Processing state */
    char line[MAX_LINE_LEN];                /* Current line buffer */
    Macro *cur = NULL;                      /* Currently-being-defined macro (NULL = not in macro) */
    char *p;                                /* Trimmed view of current line */
    char tok[MAX_LINE_LEN];                 /* First token of line */
    Macro *invoked;                         /* Macro being invoked */
    size_t i;
    char *name;
    long line_no = 0;                       /* Current line number (1-based) */
    int start_errs;                         /* Error count at start */
    int rc;

    /* Build file paths */
    strcpy(file_as, in_path);
    strcat(file_as, ".as");                 /* Input file */

    strcpy(file_am, in_path);
    strcat(file_am, ".am");                 /* Final output file */

    strcpy(tmp_am, in_path);
    strcat(tmp_am, ".am.tmp");              /* Temporary output file */

    /* Open input file */
    fin = fopen(file_as, "r");
    if (!fin) {
        diag_error(&g_diag, MC_FILE_OPEN, file_as, 1, 1,
                   "<open>", 1, 1, "failed to open '%s' for reading", file_as);
        return -1;
    }

    /* Open temporary output file */
    fout = fopen(tmp_am, "w");
    if (!fout) {
        diag_error(&g_diag, MC_FILE_OPEN, file_as, 1, 1,
                   "<open>", 1, 1, "failed to create temporary '%s'", tmp_am);
        fclose(fin);
        return -1;
    }

    /* Remember error count so we can detect new errors */
    start_errs = g_diag.error_count;

    /* ========== MAIN PROCESSING LOOP ========== */
    while (fgets(line, sizeof(line), fin)) {
        int had_overflow = 0;

        line_no++;

        /* Check for too many lines in file */
        if (line_no > MAX_SOURCE_LINES) {
            diag_error(&g_diag, MC_FILE_TOO_LONG, file_as, line_no, 1, line, 1, (int)strlen(line),
                      "file exceeds maximum lines (%d)", MAX_SOURCE_LINES);
        }

        /* Detect line overflow: no newline found and not at EOF */
        if (!strchr(line, '\n') && !feof(fin)) {
            int ch;
            /* Drain the rest of the overlong line */
            while ((ch = fgetc(fin)) != '\n' && ch != EOF) { /* drain */ }
            had_overflow = 1;
        }

        /* Clean up the line: remove leading/trailing whitespace */
        p = ltrim(line);                    /* Skip leading whitespace */
        rtrim(p);                           /* Remove trailing whitespace */
        first_token(p, tok);                /* Extract first token */

        /* Report line overflow error */
        if (had_overflow) {
            const char *ctx = p[0] ? p : line;    /* Use trimmed line or original for context */
            int len = (int)strlen(ctx);
            if (len == 0) len = (MAX_LINE_LEN - 1);
            diag_error(&g_diag, MC_LINE_TOO_LONG, file_as, line_no, 1,
                       ctx, 1, len, "line exceeds maximum length (%d chars)", (int)(MAX_LINE_LEN - 1));
        }

        /* ========== CASE 1: INSIDE MACRO DEFINITION ========== */
        if (cur) {
            /* Check for end of macro */
            if (strcmp(tok, "mcroend") == 0) {
                /* Validate no extra text after mcroend */
                char *after = p + 7;        /* Skip "mcroend" (7 characters) */
                while (*after && isspace((unsigned char)*after)) after++;
                
                if (*after != '\0') {       /* Found garbage after mcroend */
                    int offs = (int)(after - p) + 1;    /* Column offset */
                    int len2 = (int)strlen(p);
                    diag_error(&g_diag, MC_GARBAGE_AFTER_END, file_as, line_no, 1,
                               p, offs, len2, "extraneous text after 'mcroend'");
                }
                
                cur = NULL;                 /* End macro definition */
            } else {
                /* Add line to current macro body */
                append_macro_line(cur, p);
            }
            continue;                       /* Skip to next line */
        }

        /* ========== CASE 2: NOT INSIDE MACRO DEFINITION ========== */

        /* Check for malformed "mcro" without space (like "mcroXYZ") */
        if (strncmp(p, "mcro", 4) == 0 && 
            strcmp(tok, "mcro") != 0 &&     /* Not exactly "mcro" */
            strcmp(tok, "mcroend") != 0) {  /* Not "mcroend" */
            
            int len3 = (int)strlen(p);
            int underline_end = (len3 >= 4) ? 4 : len3 ? len3 : 1;
            diag_error(&g_diag, MC_NO_SPACE_AFTER, file_as, line_no, 1,
                       p, 1, underline_end, "missing space after 'mcro' before macro name");
            continue;
        }

        /* ========== CASE 3: NEW MACRO DEFINITION ========== */
        if (strcmp(tok, "mcro") == 0) {
            /* Extract macro name from the line */
            name = get_macro_name(p);
            
            /* Validate name exists */
            if (!name || *name == '\0') {
                int len4 = (int)strlen(p);
                diag_error(&g_diag, MC_NO_NAME, file_as, line_no, 1,
                           p, 1, (len4 ? len4 : 1), "macro name is missing after 'mcro'");
                continue;
            }
            
            /* Validate name length */
            if ((int)strlen(name) > MACRO_NAME_MAX) {
                diag_error(&g_diag, MC_NAME_TOO_LONG, file_as, line_no, 1,
                           p, 1, (int)strlen(p), "macro name too long (max %d)", MACRO_NAME_MAX);
                continue;
            }
            
            /* Validate name syntax */
            if (!is_valid_macro_name(name)) {
                diag_error(&g_diag, MC_NAME_BAD_SYNTAX, file_as, line_no, 1,
                           p, 1, (int)strlen(p), "invalid macro name syntax");
                continue;
            }
            
            /* Check if name is reserved */
            if (is_reserved_mnemonic(name) || is_register_name(name) ||
                strcmp(name, "mcro") == 0 || strcmp(name, "mcroend") == 0) {
                diag_error(&g_diag, MC_NAME_RESERVED, file_as, line_no, 1,
                           p, 1, (int)strlen(p), "macro name '%s' is reserved", name);
                continue;
            }
            
            /* Check for duplicate definition */
            if (find_macro(tbl, name)) {
                diag_error(&g_diag, MC_NAME_DUP, file_as, line_no, 1,
                           p, 1, (int)strlen(p), "macro '%s' already defined", name);
                continue;
            }
            
            /* All validations passed - create the macro */
            cur = add_macro(tbl, name);
            continue;
        }

        /* ========== CASE 4: MACRO INVOCATION ========== */
        if (tok[0] != '\0') {               /* Non-empty first token */
            invoked = find_macro(tbl, tok);
            if (invoked) {
                /* Expand macro: write all body lines to output */
                for (i = 0; i < invoked->n_body; i++)
                    fprintf(fout, "%s\n", invoked->body[i]);
                continue;
            }
        }

        /* ========== CASE 5: NORMAL LINE ========== */
        /* Not a macro definition or invocation - copy as-is */
        fprintf(fout, "%s\n", p);
    }

    /* ========== POST-PROCESSING ========== */

    /* Check for unterminated macro (EOF while inside definition) */
    if (cur != NULL) {
        diag_error(&g_diag, MC_UNTERMINATED_MACRO, file_as, line_no, 1,
                   "", 1, 1, "unterminated macro: missing 'mcroend'");
    }

    /* Close files */
    fclose(fin);
    fclose(fout);

    /* If any errors occurred during processing, clean up and fail */
    if (g_diag.error_count > start_errs) {
        remove(tmp_am);                     /* Delete temporary file */
        return -1;                          /* Indicate failure */
    }

    /* Success: atomically replace output file */
    remove(file_am);                        /* Remove old .am file (ignore errors) */
    rc = rename(tmp_am, file_am);           /* Rename temp to final name */
    
    if (rc != 0) {
        diag_error(&g_diag, MC_FILE_RENAME, file_as, 1, 1,
                   "<rename>", 1, 1, "failed to rename '%s' -> '%s'", tmp_am, file_am);
        remove(tmp_am);                     /* Clean up temp file */
        return -1;
    }

    return 0;                               /* Success! */
}