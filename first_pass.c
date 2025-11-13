#include "first_pass.h"
#include "words.h"
#include "assembler.h"


/* Global diagnostic system for error reporting across all assembler functions */
static diag g_diag = {0 , NULL};

/**
 * Validates that the sum of Instruction Counter and Data Counter doesn't exceed memory limits.
 * The assembler enforces an 8-bit address space limitation.
 */
int check_sum_limit(const char *file_am,       /* Source filename for error reporting */
                           long line_no,        /* Current line number */
                           const char *line_text, /* Full line text for error context */
                           long IC, long DC)    /* Current instruction and data counters */
{
    long sum = IC + DC;                         /* Total memory usage */
    int end_col;                               /* Column position for error underlining */

    /* Return success if within limits */
    if (sum < OB_SUM_LIMIT) return 1;

    /* Calculate column position for full-line error highlighting */
    end_col = (int)strlen(line_text);
    if (end_col < 1) end_col = 1;

    /* Report memory overflow error with detailed information */
    diag_error(&g_diag, "AS_SUM_GE_LIMIT",
               file_am, line_no, 1,
               line_text, 1, end_col,
               "IC (%ld) + DC (%ld) = %ld; must be < %d",
               IC, DC, sum, OB_SUM_LIMIT);
    return 0;
}

/* ----- Symbol Table Operations ----- */

/**
 * Linear search through symbol table linked list to find symbol by name.
 * Used for duplicate label detection and symbol resolution.
 */
symbol_table* find_symbol(symbol_table *head,  /* Head of symbol table linked list */
                          const char *name)    /* Symbol name to search for */
{
    symbol_table *current = head;
    while (current != NULL) {
        if (strcmp(current->key, name) == 0) {  /* Case-sensitive comparison */
            return current;                     /* Found - return symbol entry */
        }
        current = current->next;               /* Continue traversal */
    }
    return NULL;                              /* Not found */
}

/* ----- String Validation Helpers ----- */

/**
 * Checks if string matches register name pattern (r0-r7, case insensitive).
 * Used to prevent labels from conflicting with CPU registers.
 */
int is_register_name(const char *str)
{
    if (!str) return 0;
    
    return (str[0] == 'r' || str[0] == 'R') &&   /* Starts with 'r' or 'R' */
           (str[1] >= '0' && str[1] <= '7') &&   /* Digit 0-7 */
           (str[2] == '\0');                     /* Exactly 2 characters */
}

/**
 * Validates label name format: starts with letter, contains only alphanumeric characters.
 * This is the basic syntax validation for all identifier names.
 */
int is_alpha_num_label(const char *str)
{
    const char *ptr;

    /* Must start with a letter (a-z, A-Z) */
    if (str == NULL || !isalpha((unsigned char)*str)) {
        return 0;
    }

    /* Check remaining characters - must be alphanumeric */
    ptr = str + 1;
    while (*ptr) {
        if (!isalnum((unsigned char)*ptr)) {
            return 0;                          /* Invalid character found */
        }
        ptr++;
    }

    return 1;                                 /* All characters valid */
}

/**
 * Case-insensitive string comparison utility.
 * Used for matching mnemonics and directives regardless of case.
 */
int ieq(const char *str1, const char *str2)
{
    while (*str1 && *str2) {
        if (tolower((unsigned char)*str1) != tolower((unsigned char)*str2)) {
            return 0;                         /* Characters don't match */
        }
        str1++;
        str2++;
    }
    return (*str1 == '\0' && *str2 == '\0'); /* Both strings ended simultaneously */
}

/* ----- Label Validation Pipeline ----- */

/**
 * Comprehensive validation of a single label name against all rules:
 * - Non-empty and proper syntax
 * - Not a reserved mnemonic/directive  
 * - Not a register name (r0-r7)
 * Reports specific error codes for different validation failures.
 */
int validate_label_name(const char *label_name,  /* Label to validate */
                        const char *file_name,   /* Source file for error reporting */
                        long line_no,            /* Line number for error reporting */
                        int col_start,           /* Starting column of label */
                        const char *line_text)   /* Full line for error context */
{
    int col_end   = col_start + (label_name ? (int)strlen(label_name) - 1 : 0);

    /* Check for empty label before colon */
    if (!label_name || *label_name == '\0') {
        diag_error(&g_diag, "AS001",
                   file_name, line_no, col_start,
                   line_text, col_start, col_start,
                   "empty label before ':'");
        return 0;
    }

    /* Validate basic syntax (alphanumeric, starts with letter) */
    if (!is_alpha_num_label(label_name)) {
        diag_error(&g_diag, "AS001",
                   file_name, line_no, col_start,
                   line_text, col_start, col_end,
                   "invalid label name '%s' (must start with a letter and be alphanumeric)",
                   label_name);
        return 0;
    }
    
    /* Check against reserved mnemonics and directives */
    if (is_reserved_mnemonic(label_name)) {
        diag_error(&g_diag, "AS015", file_name, line_no, col_start,
                   line_text, col_start, col_end,
                   "label name '%s' is a reserved mnemonic/directive", label_name);
        return 0;
    }
    
    /* Check against register names (r0-r7) */
    if (is_register_name(label_name)) {
        diag_error(&g_diag, "AS016", file_name, line_no, col_start,
                   line_text, col_start, col_end,
                   "label name '%s' conflicts with a register name", label_name);
        return 0;
    }
    
    return 1;  /* All validations passed */
}

/**
 * Detects and extracts label definitions from assembly line.
 * A label is an identifier followed by a colon (:).
 * Performs complete validation and returns allocated label name on success.
 */
int is_label_definition(const char *line_start,    /* Start of line to parse */
                        const char *full_line,     /* Complete line for error context */
                        const char *file_name,     /* Source filename */
                        long line_no,              /* Line number */
                        char **label_out)          /* Output: allocated label name */
{
    const char *p = line_start;                   /* Parse position */
    const char *q;                                /* End of potential label */
    size_t len;                                   /* Length of extracted label */
    int col_start;                               /* Column position for errors */

    if (label_out) *label_out = NULL;            /* Initialize output */

    /* Skip leading whitespace */
    while (*p && isspace((unsigned char)*p)) p++;

    /* Find token ending - must end with ':' to be a label */
    q = p;
    while (*q && !isspace((unsigned char)*q) && *q != ';' && *q != ':')
        q++;
    if (*q != ':')
        return 0;                               /* No colon found - not a label */

    /* Check label length limits */
    len = (size_t)(q - p);
    if (len > MAX_LABEL_LEN) {
        diag_error(&g_diag, "AS001", file_name, line_no, 1,
                   full_line, 1, (int)len,
                   "label too long (max %d chars)", MAX_LABEL_LEN);
        return 0;
    }

    /* Allocate and copy label name */
    *label_out = (char*)malloc(len + 1);
    if (!*label_out) return 0;                  /* Memory allocation failed */

    memcpy(*label_out, p, len);
    (*label_out)[len] = '\0';

    /* Validate the extracted label */
    col_start = (int)(p - full_line) + 1;
    return validate_label_name(*label_out, file_name, line_no, col_start, full_line);
}

/* ----- Command Classification System ----- */

/**
 * Matches token against instruction mnemonics using lookup table.
 * Returns specific instruction type or UNDEFINED if no match.
 */
CommandsTable match_instruction(const char *token)
{
    /* Static lookup table for all supported instructions */
    const struct {
        const char *name;      /* Mnemonic string */
        CommandsTable type;    /* Corresponding enum value */
    } instructions[] = {
        {"mov", MOV}, {"cmp", CMP}, {"add", ADD}, {"sub", SUB},
        {"not", NOT}, {"clr", CLR}, {"lea", LEA}, {"inc", INC},
        {"dec", DEC}, {"jmp", JMP}, {"bne", BNE}, {"red", RED},
        {"prn", PRN}, {"jsr", JSR}, {"rts", RTS}, {"stop", STOP}
    };
    
    size_t i;
    /* Linear search through instruction table (case-insensitive) */
    for (i = 0; i < (sizeof(instructions) / sizeof(instructions[0])); i++) {
        if (ieq(token, instructions[i].name)) {
            return instructions[i].type;
        }
    }
    return UNDEFINED;                          /* No match found */
}

/**
 * Matches token against directive names using lookup table.  
 * Handles both with and without leading dot (e.g., ".data" or "data").
 */
CommandsTable match_directive(const char *token)
{
    /* Static lookup table for all supported directives */
    const struct {
        const char *name;      /* Directive name (without dot) */
        CommandsTable type;    /* Corresponding enum value */
    } directives[] = {
        { "data",   DATA   },
        { "string", STRING },
        { "mat",    MATRIX },  /* Note: "mat" not "matrix" */
        { "entry",  ENTRY  },
        { "extern", EXTERN }
    };

    size_t i;
    /* Linear search through directive table (case-insensitive) */
    for (i = 0; i < (sizeof(directives) / sizeof(directives[0])); i++) {
        if (ieq(token, directives[i].name)) {
            return directives[i].type;
        }
    }
    return UNDEFINED;                          /* No match found */
}

/**
 * Central command classification function.
 * Extracts first token from line and determines if it's an instruction or directive.
 * Handles error reporting for malformed or unrecognized commands.
 */
CommandsTable get_command_type(const char *line,        /* Line to parse */
                               const char *file_name,   /* File for error reporting */
                               long line_no)            /* Line number for errors */
{
    const char *ptr;                          /* Parse position */
    char token[TOK_MAX_LEN];                 /* Extracted token buffer */
    size_t token_len = 0;                    /* Length of extracted token */
    const char *command_token;               /* Token after dot removal */
    CommandsTable result;                    /* Classification result */

    /* Validate input parameters */
    if (!line) {
        diag_error(&g_diag, "AS001",
                   file_name, line_no, 1,
                   NULL, 1, 1,
                   "NULL line passed to get_command_type");
        return UNDEFINED;
    }

    /* Skip leading whitespace to find first meaningful content */
    ptr = line;
    while (*ptr && isspace((unsigned char)*ptr)) ptr++;

    /* Handle empty lines and comments */
    if (*ptr == '\0' || *ptr == ';') {
        return UNDEFINED;                     /* Nothing to classify */
    }

    /* Extract first token, stopping at delimiters */
    while (*ptr && !isspace((unsigned char)*ptr) &&
           *ptr != ',' && *ptr != '[' && *ptr != ';' &&
           token_len + 1 < sizeof(token)) {
        token[token_len++] = (char)tolower((unsigned char)*ptr);
        ptr++;
    }
    token[token_len] = '\0';

    /* Ensure we extracted something */
    if (token_len == 0) {
        int len = line ? (int)strlen(line) : 0;
        diag_error(&g_diag, "AS002",
                   file_name, line_no, 1,
                   line, 1, len,
                   "could not extract command from line");
        return UNDEFINED;
    }

    /* Handle directive tokens (may start with dot) */
    command_token = (token[0] == '.') ? token + 1 : token;

    /* Try instruction lookup first */
    result = match_instruction(token);
    if (result != UNDEFINED) {
        return result;
    }

    /* Try directive lookup */
    result = match_directive(command_token);
    if (result != UNDEFINED) {
        return result;
    }

    /* Unknown command - report error */
    diag_error(&g_diag, "AS004",
               file_name, line_no, 1,
               line, 1, (int)strlen(line),
               "unrecognized command or directive '%s'", token);
    return UNDEFINED;
}

/**
 * Determines if a token is reserved (instruction, directive, or register).
 * Used during label validation to prevent conflicts with language keywords.
 */
int is_reserved_mnemonic(const char *token)
{
    CommandsTable t;

    if (token == NULL || *token == '\0')
        return 0;

    /* Check if it's an instruction mnemonic */
    t = match_instruction(token);
    if (t != UNDEFINED)
        return 1;

    /* Check if it's a directive name */
    t = match_directive(token);
    if (t != UNDEFINED)
        return 1;

    /* Check if it's a register name (r0-r7) */
    if (is_register_name(token))
        return 1;

    return 0;                                 /* Not reserved */
}

/* ----- Symbol Table and Memory Management ----- */

/**
 * Relocates data symbols and addresses after instruction section is finalized.
 * In two-pass assembly, data follows instructions, so data addresses must be 
 * adjusted by the final instruction counter value.
 */
void relocate_data(table *symtab,             /* Symbol table to update */
                   data_word *data_img,       /* Data image to relocate */
                   long finalIC)              /* Final instruction counter value */
{
    symbol_table *cur_sym;                   /* Current symbol being processed */
    data_word *cur_data;                     /* Current data word being processed */

    /* Relocate all DATA_SYMBOL entries in symbol table */
    for (cur_sym = symtab->head; cur_sym != NULL; cur_sym = cur_sym->next) {
        if (cur_sym->type == DATA_SYMBOL) {
            cur_sym->value += finalIC;        /* Adjust address by instruction size */
        }
    }

    /* Relocate addresses in data image linked list */
    for (cur_data = data_img; cur_data != NULL; cur_data = cur_data->next) {
        cur_data->address += finalIC;         /* Adjust address by instruction size */
    }
}

/**
 * Processes .extern directive by extracting label name and adding to extern list.
 * Simple tokenization approach using strtok.
 */
void handle_extern(char line[],               /* Line containing extern directive */
                   extern_list **ext_list)    /* External symbols list to update */
{
    char line_copy[MAX_LINE_LEN];            /* Safe copy for tokenization */
    char *label_name;                        /* Extracted label name */

    /* Create safe copy since strtok modifies the string */
    strncpy(line_copy, line, sizeof(line_copy) - 1);
    line_copy[sizeof(line_copy) - 1] = '\0';

    /* Extract first token as label name */
    label_name = strtok(line_copy, "\t \n");
    if (label_name) {
        add_extern(ext_list, label_name);    /* Add to external symbols list */
    }
}

/**
 * Processes .entry directive by extracting label name and adding to entry list.
 * Similar to handle_extern but for symbols to be exported.
 */
void handle_entry(char line[],               /* Line containing entry directive */
                  entry_list **ent_list)     /* Entry symbols list to update */
{
    char line_copy[MAX_LINE_LEN] = {0};     /* Safe copy for tokenization */
    const char *label_name;                  /* Extracted label name */

    strcpy(line_copy, line);

    /* Extract first token as label name */
    label_name = strtok(line_copy, "\t \n");
    if (label_name) {
        add_entry(ent_list, label_name, 0);  /* Add to entry list (address TBD) */
    }
}

/**
 * Initializes processing state for a new source file.
 * Sets up symbol table and resets counters to initial values.
 */
void initialize_file_processing(table *symbol_table,  /* Symbol table to initialize */
                                long *IC_out,         /* Instruction counter to reset */
                                long *DC_out)         /* Data counter to reset */
{
    symbol_table->head = NULL;               /* Empty symbol table */
    symbol_table->size = 0;                  /* No symbols yet */
    *IC_out = IC_INIT_VALUE;                 /* Start instructions at base address */
    *DC_out = 0;                             /* Start data at zero offset */
}

/**
 * Determines what type of symbol a label should be based on the command it labels.
 * Used for proper symbol table categorization and address calculation.
 */
symbol_type determine_symbol_type(CommandsTable cmd)
{
    switch (cmd) {
        case DATA:                           /* Data allocation directives */
        case STRING:
        case MATRIX:
            return DATA_SYMBOL;              /* Symbol points to data */
        case ENTRY:
            return ENTRY_SYMBOL;             /* Symbol for export */
        case EXTERN:
            return EXTERNAL_SYMBOL;          /* External symbol reference */
        default:
            return CODE_SYMBOL;              /* All instructions create code symbols */
    }
}

/**
 * Adds a validated label to the symbol table with appropriate address calculation.
 * Code symbols get current IC value, data symbols get projected final address.
 */
void process_valid_label(table *symbol_table,    /* Symbol table to update */
                        const char *label_name,   /* Validated label name */
                        symbol_type sym_type,     /* Type of symbol */
                        long IC_value,            /* Current instruction counter */
                        long DC_value)            /* Current data counter */
{
    if (sym_type == CODE_SYMBOL) {
        /* Code labels get current instruction address */
        add_table_item(symbol_table, label_name, IC_value, CODE_SYMBOL);
    } else if (sym_type == DATA_SYMBOL) {
        /* Data labels get projected address (IC + current DC offset) */
        long abs_data_addr = IC_value + DC_value;
        add_table_item(symbol_table, label_name, abs_data_addr, DATA_SYMBOL);
    }
}

/* ----- Directive Argument Processing ----- */

/**
 * Comprehensive handler for .entry and .extern directive arguments.
 * Performs complete validation:
 * - Extracts label from argument list
 * - Validates label syntax and checks for reserved names  
 * - Ensures no extra arguments present
 * - Adds validated label to appropriate list
 */
int handle_directive_argument(const char *directive_name,  /* "entry" or "extern" */
                             const char *arg,              /* Arguments after directive */
                             const char *file_am,          /* Source filename */
                             long line_no,                 /* Line number */
                             int col_end,                  /* End column of directive */
                             const char *line_buffer,      /* Full line text */
                             entry_list **ent_list,       /* Entry symbols list */
                             extern_list **ext_list,      /* External symbols list */
                             diag *dctx)                   /* Diagnostic context */
{
    const char *p = arg;                     /* Current parse position */
    const char *q;                           /* End of current token */
    const char *r;                           /* Position for trailing junk check */
    int line_len = line_buffer ? (int)strlen(line_buffer) : 0;
    int col;                                 /* Column position for errors */
    int tok_len;                             /* Length of extracted token */
    char label[MAX_LABEL_LEN + 1];          /* Buffer for label name */

    /* Calculate initial column position for error reporting */
    col = col_end + 1;                      /* Default: just after directive */
    if (p && line_buffer && p >= line_buffer && p <= line_buffer + line_len) {
        col = (int)(p - line_buffer) + 1;   /* More precise if arg is within line */
    }

    /* Skip leading whitespace before label */
    while (p && *p && isspace((unsigned char)*p)) { 
        p++; 
        col++; 
    }

    /* Check for missing argument */
    if (!p || *p == '\0' || *p == ';') {
        diag_error(dctx,
                   (strcmp(directive_name, "entry") == 0) ? "AS011" : "AS012",
                   file_am, line_no, col,
                   line_buffer, col, col,
                   "missing label after '.%s'", directive_name);
        return 1;
    }

    /* Extract label token - stop at whitespace, semicolon, or comma */
    q = p;
    while (*q && !isspace((unsigned char)*q) && *q != ';' && *q != ',') q++;
    tok_len = (int)(q - p);

    /* Validate token was extracted */
    if (tok_len <= 0) {
        diag_error(dctx, (strcmp(directive_name, "entry") == 0) ? "AS011" : "AS012",
                   file_am, line_no, col,
                   line_buffer, col, col,
                   "missing label after '.%s'", directive_name);
        return 1;
    }

    /* Check label length limits */
    if (tok_len > MAX_LABEL_LEN) {
        int cs = col, ce = col + tok_len - 1;
        diag_error(dctx, "AS013",
                   file_am, line_no, cs,
                   line_buffer, cs, ce,
                   "label is too long (max %d)", MAX_LABEL_LEN);
        return 1;
    }

    /* Copy token into buffer */
    memcpy(label, p, (size_t)tok_len);
    label[tok_len] = '\0';

    /* Comprehensive label validation */
    if (!is_alpha_num_label(label) || is_reserved_mnemonic(label) || is_register_name(label)) {
        int cs = col, ce = col + tok_len - 1;
        diag_error(dctx, "AS014",
                   file_am, line_no, cs,
                   line_buffer, cs, ce,
                   "invalid label name '%s' for .%s", label, directive_name);
        return 1;
    }

    /* Check for trailing junk after the label */
    r = q;
    while (*r && isspace((unsigned char)*r)) r++;   /* Skip whitespace */
    if (*r && *r != ';') {                          /* Something other than comment */
        int cs = (line_buffer && r >= line_buffer) ? (int)(r - line_buffer) + 1 : col;
        diag_error(dctx, "AS015",
                   file_am, line_no, cs,
                   line_buffer, cs, cs,
                   "unexpected characters after label in '.%s'", directive_name);
        return 1;
    }

    /* All validations passed - add to appropriate list */
    if (strcmp(directive_name, "entry") == 0) {
        add_entry(ent_list, label, 0);              /* Address will be resolved later */
    } else { /* "extern" */
        add_extern(ext_list, label);                /* External reference */
    }

    return 0;                                       /* Success */
}

int first_pass(const char *file_stem, table *symbol_table, data_word **data_img, 
               machine_word **ic_image, long *IC_out, long *DC_out, 
               extern_list **ext_list, entry_list **ent_list)
{
    /* File I/O variables */
    char file_am[MAX_LINE_LEN];      /* Assembled filename with .am extension */
    FILE *source_file;               /* File handle for reading assembly source */
    char line_buffer[MAX_LINE_LEN];  /* Buffer for current line being processed */
    long line_no = 0;                /* Current line number (1-based) for error reporting */
    
    /* Line parsing state */
    int  skip_white = 0;             /* Index to skip leading whitespace in current line */
    char *label_name = NULL;         /* Dynamically allocated label name if found */
    int  has_label = 0;              /* Boolean: does current line have a label definition */
    CommandsTable cmd = UNDEFINED;   /* Classified command type (instruction/directive) */
    symbol_type   sym_type = NONE_SYMBOL; /* Symbol category for label (CODE/DATA) */

    /* Token extraction and position tracking */
    const char *current_pos = NULL;  /* Pointer to current parsing position in line */
    const char *token_end   = NULL;  /* Pointer to end of extracted token */
    char  token[TOK_MAX_LEN];        /* Buffer for current token (mnemonic/directive) */
    size_t token_len = 0;            /* Length of extracted token */
    int col_start = 0, col_end = 0;  /* Column positions for error underlining */

    /* Error handling strategy - accumulate errors, don't bail early */
    int had_error = 0;               /* Local error flag for this function */
    int rc = 0;                      /* Return code from processing functions */

    /* Per-iteration variables declared in loop for clarity */
    const char *temp_pos = NULL;     /* Temporary position pointer for token parsing */
    const char *space_pos = NULL;    /* Position of first space/tab in token */
    const char *colon_pos = NULL;    /* Position of colon (for label validation) */
    const char *arg = NULL;          /* Pointer to directive arguments */

    /* Build full filename by appending .am extension */
    strcpy(file_am, file_stem);
    strcat(file_am, FILE_EXT_AM);
    source_file = fopen(file_am, "r");
    if (!source_file) {
        diag_error(&g_diag, "AS040",
                file_am, DEFAULT_LINE_NO, DEFAULT_COL,
                NULL, DEFAULT_LINE_NO, DEFAULT_COL,
                "cannot open source file '%s'", file_am);
        return 1;
    }

    /* Initialize processing counters and symbol table */
    initialize_file_processing(symbol_table, IC_out, DC_out);

    /* Main parsing loop - process each line of assembly source */
    while (fgets(line_buffer, sizeof(line_buffer), source_file)) {
        /* Reset per-iteration state */
        skip_white = 0;
        line_no++;

        /* Skip leading whitespace to find first meaningful character */
        MOVE_TO_NOT_WHITE(line_buffer, skip_white);
        current_pos = line_buffer + skip_white;

        /* Skip empty lines and comment lines (starting with semicolon) */
        if (*current_pos == '\0' || *current_pos == ';') {
            continue;
        }

        /* PHASE 1: Label Detection and Validation */
        /* Check if line starts with a label definition (identifier followed by colon) */
        has_label = is_label_definition(
                        line_buffer + skip_white, /* parse from here */
                        line_buffer,              /* full line for context/underline */
                        file_am,                  /* file name */
                        line_no,                  /* current line number */
                        &label_name               /* out: heap-allocated label */
                    );

        /* Handle invalid label syntax - colon present but label validation failed */
        if (!has_label) {
            space_pos = strpbrk(current_pos, " \t;");  /* Find first delimiter */
            colon_pos = strchr(current_pos, ':');      /* Find colon */

            /* Set space_pos to end of string if no delimiter found */
            if (!space_pos) {
                space_pos = current_pos + strlen(current_pos);
            }

            /* If colon appears before any space, it's likely an invalid label */
            if (colon_pos && colon_pos < space_pos) {
                col_start = (int)(current_pos - line_buffer) + 1;
                col_end   = (int)(colon_pos   - line_buffer);
                diag_error(&g_diag, "AS001",
                           file_am, line_no, col_start,
                           line_buffer, col_start, col_end,
                           "invalid label name (must start with a letter, max %d chars, "
                           "alphanumeric only, not a reserved opcode/directive or register name r0..r7)",
                           MAX_LABEL_LEN);
                had_error = 1;  /* Continue processing to find more errors */
            }
        }

        /* If valid label found, advance past it and check for duplicates */
        if (has_label) {
            /* Skip past the label name and colon */
            while (line_buffer[skip_white] && line_buffer[skip_white] != ':') {
                skip_white++;
            }
            if (line_buffer[skip_white] == ':') {
                skip_white++;  /* Skip the colon itself */
            }
            /* Skip whitespace after colon */
            while (isspace((unsigned char)line_buffer[skip_white])) {
                skip_white++;
            }
            current_pos = line_buffer + skip_white;

            /* Check if this label name already exists in symbol table */
            if (find_symbol(symbol_table->head, label_name)) {
                /* Calculate approximate column position of label for error reporting */
                int approx_start = (int)((current_pos - line_buffer) - (int)strlen(label_name)) - 1;
                if (approx_start < 1) approx_start = 1;

                col_start = approx_start;
                col_end   = col_start + (int)strlen(label_name) - 1;

                diag_error(&g_diag, "AS020",
                           file_am, line_no, col_start,
                           line_buffer, col_start, col_end,
                           "duplicate label '%s'", label_name);

                free(label_name);  /* Clean up duplicate label */
                label_name = NULL;
                had_error = 1;
            }
        }

        /* PHASE 2: Token Extraction (mnemonic or directive) */
        /* Extract first token after label, stopping at delimiters */
        token_len = 0;
        token[0] = '\0';
        temp_pos = current_pos;

        /* Build token character by character, converting to lowercase */
        while (*temp_pos &&
               !isspace((unsigned char)*temp_pos) &&
               *temp_pos != ',' && *temp_pos != '[' && *temp_pos != ';' &&
               token_len + 1 < sizeof(token))
        {
            token[token_len++] = (char)tolower((unsigned char)*temp_pos);
            temp_pos++;
        }
        token[token_len] = '\0';
        token_end = temp_pos;  /* Remember where token ended for argument parsing */

        /* Calculate column positions for error reporting */
        col_start = (int)(current_pos - line_buffer) + 1;
        col_end   = col_start + (int)token_len - 1;

        /* PHASE 3: Command Classification */
        /* Determine what type of instruction or directive this is */
        cmd = get_command_type((char *)current_pos, file_am, line_no);

        /* Handle unrecognized mnemonics or directives */
        if (cmd == UNDEFINED) {
            int start = col_start;
            int end   = col_end;
            if (end < start) end = start;  /* Ensure valid range */

            if (token_len > 0) {
                diag_error(&g_diag, "AS002",
                           file_am, line_no, start,
                           line_buffer, start, end,
                           "unknown mnemonic or directive '%s'", token);
            } else {
                diag_error(&g_diag, "AS002",
                           file_am, line_no, start,
                           line_buffer, start, start,
                           "unknown or missing mnemonic/directive");
            }

            /* Clean up and continue to next line */
            if (label_name) { free(label_name); label_name = NULL; }
            had_error = 1;
            continue;
        }

        /* Determine symbol type based on command type (for label classification) */
        sym_type = determine_symbol_type(cmd);

        /* PHASE 4: Label Registration */
        /* Add valid label to symbol table (except for .entry/.extern which are special) */
        if (has_label && (cmd != ENTRY && cmd != EXTERN) && label_name) {
            process_valid_label(symbol_table, label_name, sym_type, *IC_out, *DC_out);
            free(label_name);  /* Label ownership transferred to symbol table */
            label_name = NULL;
        }

        /* PHASE 5: Instruction/Directive Processing */
        switch (cmd) {
        /* Machine code instructions - generate binary and update IC */
        case MOV: case CMP: case ADD: case SUB: case NOT: case CLR:
        case LEA: case INC: case DEC: case JMP: case BNE: case RED:
        case PRN: case JSR: case RTS: case STOP:
            /* Parse operands and generate machine code words */
            process_commands_words(cmd, (char *)current_pos, IC_out, ic_image,
                                   symbol_table, file_am, line_no);

            /* Enforce memory limit: total program size must fit in 8-bit addressing */
            if (!check_sum_limit(file_am, line_no, line_buffer, *IC_out, *DC_out)) {
                had_error = 1;
            }
            break;

        /* Data allocation directives - store data and update DC */
        case DATA: case STRING: case MATRIX:
            rc = process_data_string_mat(cmd, (char *)current_pos, DC_out, data_img, IC_out, line_no , file_am);
            if (rc != 0) {
                had_error = 1;  /* Data processing error - continue to find more */
            }

            /* Check memory limits after data allocation */
            if (!check_sum_limit(file_am, line_no, line_buffer, *IC_out, *DC_out)) {
                had_error = 1;
            }
            break;

        /* Entry directive - mark symbols for export to other files */
        case ENTRY:
            arg = token_end;
            while (*arg && isspace((unsigned char)*arg)) arg++;  /* Skip to argument */
            (void)handle_directive_argument("entry", arg, file_am, line_no,
                                            col_end, line_buffer, ent_list,
                                            ext_list, &g_diag);
            break;

        /* Extern directive - declare symbols defined in other files */
        case EXTERN:
            arg = token_end;
            while (*arg && isspace((unsigned char)*arg)) arg++;  /* Skip to argument */
            (void)handle_directive_argument("extern", arg, file_am, line_no,
                                            col_end, line_buffer, ent_list,
                                            ext_list, &g_diag);
            break;

        case UNDEFINED:
        default:
            /* Already handled above */
            break;
        }

        /* Safety cleanup - ensure no memory leaks if label processing was interrupted */
        if (label_name) { 
            free(label_name); 
            label_name = NULL; 
        }
    }

    /* Debug output - only print if no errors found to avoid noise */
    if (g_diag.error_count == 0 && !had_error) {
        printf("\n\n");
        print_machine_word_list(*ic_image);  /* Show generated machine code */
        printf("\n\n");

        print_entry_list(*ent_list);         /* Show symbols marked for export */
        printf("\n\n");

        print_extern_list(*ext_list);        /* Show external symbol references */
        printf("\n\n");

        print_data_word_list(*data_img);     /* Show allocated data */
        printf("\n\n");
    }

    /* Automatically proceed to second pass if first pass was completely clean */
    if (g_diag.error_count == 0 && !had_error) {
        second_pass(file_stem, symbol_table, *ic_image, *data_img,
                    (*IC_out - IC_INIT_VALUE), *DC_out, *ext_list, *ent_list);
    }

    fclose(source_file);
    
    /* Return 0 for success, 1 if any errors were found */
    return (g_diag.error_count == 0 && !had_error) ? 0 : 1;
}