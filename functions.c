#include "assembler.h"
static diag g_diag = {0, NULL};
/* === Linked List Management (Creations) === */

entry_list *create_entry_node(const char *label, int addr)
{
    entry_list *node = (entry_list *)malloc(sizeof(entry_list));
    
    /* Exit immediately on memory allocation failure - critical error */
    if (!node)
    {
        perror("malloc failed");
        exit(1);
    }
    
    /* Copy label string safely, ensuring null termination */
    strncpy(node->labels, label, sizeof(node->labels) - 1);
    node->labels[sizeof(node->labels) - 1] = '\0'; /* Force null termination */
    
    /* Initialize node data and link pointer */
    node->addr = addr;
    node->next = NULL;
    
    return node;
}

void add_entry(entry_list **head, const char *label, int addr)
{
    entry_list *new_node;
    entry_list *temp;
    
    /* Create new entry node with provided data */
    new_node = create_entry_node(label, addr);

    /* Handle empty list - make new node the head */
    if (*head == NULL){
        *head = new_node;
        return;
    }
    
    /* Traverse to end of list to append new node */
    temp = *head;
    while (temp->next != NULL){
        temp = temp->next;
    }

    /* Link new node at end of list */
    temp->next = new_node;
}

extern_list *create_extern_node(const char *label)
{
    extern_list *node = (extern_list *)malloc(sizeof(extern_list));
    
    /* Exit on memory allocation failure - critical error */
    if (!node){
        perror("malloc failed");
        exit(1);
    }
    
    /* Copy label string safely with guaranteed null termination */
    strncpy(node->labels, label, sizeof(node->labels) - 1);
    node->labels[sizeof(node->labels) - 1] = '\0';
    
    /* Initialize extern node - no addresses recorded yet */
    node->addr_head = NULL; 
    node->next = NULL;
    
    return node;
}

void add_extern(extern_list **head, const char *label)
{
    extern_list *new_node;
    extern_list *temp;

    /* Create new extern node for this label */
    new_node = create_extern_node(label);

    /* Handle empty list case */
    if (*head == NULL){
        *head = new_node;
        return;
    }
    
    /* Find end of list and append new node */
    temp = *head;
    while (temp->next != NULL)
        temp = temp->next;
    temp->next = new_node;
}

void add_data_word_to_end(data_word **head, data_word *new_node)
{
    data_word *temp;
    
    /* Guard against invalid parameters */
    if (head == NULL || new_node == NULL){
        return;
    }
    
    /* Ensure new node is properly terminated */
    new_node->next = NULL;
    
    /* Handle empty list - new node becomes head */
    if (*head == NULL){
        *head = new_node;
    }
    else
    {
        /* Traverse to end and append new node */
        temp = *head;
        while (temp->next != NULL){
            temp = temp->next;
        }
        temp->next = new_node;
    }
}




/* === Symbol Table Management === */

int add_table_item(table *tab, const char *key, long value, symbol_type type)
{
    symbol_table *curr;
    symbol_table *new_entry;

    /* First check if key already exists - update if found */
    curr = tab->head;
    while (curr){
        if (strcmp(curr->key, key) == 0){
            /* Update existing entry with new value and type */
            curr->value = value;
            curr->type = type;
            return 1; /* Success - updated existing entry */
        }
        curr = curr->next;
    }

    /* Key not found - create new entry */
    new_entry = malloc(sizeof(symbol_table));
    if (new_entry == NULL)
    {
        printf(" Malloc failed for symbol_table\n");
        exit(1); /* Critical memory error */
    }

    /* Allocate memory for key string copy */
    new_entry->key = malloc(strlen(key) + 1);
    if (new_entry->key == NULL)
    {
        free(new_entry); /* Clean up partial allocation */
        return 0; /* Memory allocation failed */
    }
    strcpy(new_entry->key, key); /* Copy key string */

    /* Initialize new symbol entry */
    new_entry->value = value;
    new_entry->type = type;
    
    /* Insert at head of list for O(1) insertion */
    new_entry->next = tab->head;
    tab->head = new_entry;
    tab->size++; /* Update table size counter */

    return 1; /* Success - added new entry */
}

/* ---- Small Parser Helpers ---- */

/* String duplication function - allocates memory for copy */
char *my_strdup(const char *s)
{
    char *p = malloc(strlen(s) + 1); /* Allocate space for string + null terminator */
    if (p)
        strcpy(p, s); /* Copy string if allocation succeeded */
    return p; /* Return copy or NULL if malloc failed */
}

const char* skip_ws(const char *p) {
    /* Advance pointer past all whitespace characters */
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

void strip_inline_comment(char *line) {
    char *sc = strchr(line, ';'); /* Find semicolon comment marker */
    if (sc) *sc = '\0'; /* Truncate line at comment start */
}

int parse_bracketed_pos_int(const char **pp, int *out_val) {
    const char *p = *pp, *endptr;
    long v;

    /* Skip whitespace and look for opening bracket */
    p = skip_ws(p);
    if (*p != '[') return 0; /* No bracket found */
    p++;
    
    /* Skip whitespace inside brackets */
    p = skip_ws(p);

    /* Parse integer value */
    v = strtol(p, (char**)&endptr, 10);
    if (endptr == p || v <= 0) return 0; /* No valid positive integer found */
    p = endptr;

    /* Skip whitespace and look for closing bracket */
    p = skip_ws(p);
    if (*p != ']') return 0; /* Missing closing bracket */
    p++;

    /* Update caller's pointer and output value */
    *pp = p;
    *out_val = (int)v;
    return 1; /* Success */
}

int in_data_range(long v) {
    /* Check if value fits in allowed data word range */
    return (v >= (long)DATA_MIN && v <= (long)DATA_MAX);
}

/* Convert addressing mode enum to 2-bit encoding for machine code */
unsigned int to2bits(addr_mode m)
{
    switch (m) {
        case ADDR_IMMEDIATE: return BITS_IMMEDIATE; /* #value addressing */
        case ADDR_DIRECT:    return BITS_DIRECT;    /* label addressing */
        case ADDR_MATRIX:    return BITS_MATRIX;    /* matrix[reg][reg] addressing */
        case ADDR_REGISTER:  return BITS_REGISTER;  /* register addressing */
        default:             return BITS_IMMEDIATE; /* Default fallback for unknown modes */
    }
}

int col_from_ptr(const char *line_start, const char *p) {
    /* Calculate column number from pointer difference (1-based indexing) */
    return (line_start && p && p >= line_start) ? (int)(p - line_start) + 1 : 1;
}

/* === Directive Parsers === */

int process_data_directive_at(char line[], long *DC, data_word **data_image, long *IC,
                              const char *file_name, long line_no)
{
    const char *p;
    const char *directive_start;
    char *endptr;
    long value;
    data_word *node;
    int expect_value = 1; /* State machine: 1 = expecting number, 0 = expecting comma */
    const char *last_comma = NULL; /* Track last comma position for error reporting*/

    /* Remove any comments from the line to avoid parsing them as data */
    strip_inline_comment(line);
    
    /* Find the .data directive in the line */
    directive_start = strstr(line, ".data");
    if (!directive_start) return -1; /* Not a .data directive */

    /* Start parsing after the .data keyword */
    p = directive_start + strlen(".data");

    /* Parse comma-separated list of integers */
    while (*p != '\0') {
        /* Skip whitespace before next token */
        p = skip_ws(p);
        if (*p == '\0') break; /* End of line reached */

        if (expect_value) {
            const char *num_begin = p; /* Remember start of number for error reporting */

            /* Error: comma without preceding number */
            if (*p == ',') {
                int c = col_from_ptr(line, p);
                diag_error(&g_diag, "AS310", file_name, line_no, c,
                           line, c, c, "missing number before comma");
                return -1;
            }

            /* Parse the integer value */
            value = strtol(p, &endptr, 10);
            
            /* Error: no valid number found */
            if (endptr == p) {
                int c = col_from_ptr(line, p);
                diag_error(&g_diag, "AS311", file_name, line_no, c,
                           line, c, c, "invalid number in .data directive");
                return -1;
            }

            /* Validate that the value fits in the allowed data range */
            if (!in_data_range(value)) {
                int cs = col_from_ptr(line, num_begin);
                int ce = col_from_ptr(line, endptr - 1);
                diag_error(&g_diag, "AS312", file_name, line_no, cs,
                           line, cs, ce, "value %ld out of data range [%d..%d]",
                           value, DATA_MIN, DATA_MAX);
                return -1;
            }

            /* Create data word and add it to the data image */
            node = create_data_word((int)value, (*DC) + (*IC));
            add_data_word_to_end(data_image, node);
            (*DC)++; /* Increment data counter */
            
            p = endptr; /* Move past the parsed number */
            expect_value = 0; /* Next token should be comma or end of line */
        } else {
            /* We're expecting a comma between numbers */
            if (*p != ',') {
                int c = col_from_ptr(line, p);
                diag_error(&g_diag, "AS313", file_name, line_no, c,
                           line, c, c, "missing comma between numbers");
                return -1;
            }
            
            /* Record comma position and move past it */
            last_comma = p;
            p++;
            expect_value = 1; /* Next token should be a number */
        }
    }

    /* Error: line ended with a comma (trailing comma not allowed) */
    if (expect_value && last_comma != NULL) {
        int c = col_from_ptr(line, last_comma);
        diag_error(&g_diag, "AS314", file_name, line_no, c,
                   line, c, c, "trailing comma at end of .data directive");
        return -1;
    }

    return 0; /* Success */
}

int process_string_directive_at(char line[], long *DC, data_word **data_image, long *IC,
                                const char *file_name, long line_no)
{
    const char *p;
    const char *directive_start;
    const char *str_start;
    data_word *null_word;

    /* Defensive programming: verify all required pointers are valid */
    if (!line || !DC || !data_image || !IC) {
        const char *lt = line ? line : "";
        diag_error(&g_diag, "AS040", file_name, line_no, 1,
                   lt, 1, 1, "internal API misuse in .string handler");
        return -1;
    }

    /* Remove any inline comments to avoid parsing them as string content */
    strip_inline_comment(line);
    
    /* Locate the .string directive in the line */
    directive_start = strstr(line, ".string");
    if (!directive_start) return -1; /* Not a .string directive */

    /* Move past the .string keyword and skip whitespace */
    p = directive_start + strlen(".string");
    p = skip_ws(p);

    /* String must start with opening quote */
    if (*p != '"') {
        int c = col_from_ptr(line, p);
        diag_error(&g_diag, "AS320", file_name, line_no, c,
                   line, c, c, ".string directive expects an opening '\"'");
        return -1;
    }

    p++;                /* Skip opening quote */
    str_start = p;      /* Remember start of string content for error reporting */

    /* Process each character in the string until closing quote or end of line */
    while (*p != '\0' && *p != '"') {
        /* Create data word for this character (cast to unsigned char for proper byte handling) */
        data_word *new_word = create_data_word((int)(unsigned char)*p, (*DC) + (*IC));
        
        /* Handle memory allocation failure */
        if (!new_word) {
            int c = col_from_ptr(line, str_start);
            diag_error(&g_diag, "AS040", file_name, line_no, c,
                       line, c, c, "memory allocation failed while emitting .string");
            return -1;
        }
        
        /* Add character to data image and advance counters */
        add_data_word_to_end(data_image, new_word);
        (*DC)++;
        p++;
    }

    /* Error: string not properly terminated with closing quote */
    if (*p != '"') {
        int cs = col_from_ptr(line, str_start);
        int ce = (int)strlen(line);
        diag_error(&g_diag, "AS321", file_name, line_no, cs,
                   line, cs, ce, "unterminated string literal");
        return -1;
    }

    p++; /* Skip closing quote */

    /* Add null terminator to complete the C-style string */
    null_word = create_data_word(0, (*DC) + (*IC));
    if (!null_word) {
        int c = col_from_ptr(line, str_start);
        diag_error(&g_diag, "AS040", file_name, line_no, c,
                   line, c, c, "memory allocation failed while emitting .string terminator");
        return -1;
    }
    
    add_data_word_to_end(data_image, null_word);
    (*DC)++;

    return 0; /* Success */
}

int process_matrix_directive_at(char line[],
                                long *DC, data_word **data_image, long *IC,
                                const char *file_name, long line_no)
{
    const char *p, *mat;
    const char *dims_begin = NULL, *dims_end = NULL; /* Track dimension specification for errors */
    const char *last_comma = NULL; /* Track comma positions for trailing comma detection */
    const char *num_begin;
    const char *look;
    char *endptr;
    int rows, cols;
    long total_values, produced; /* Track expected vs actual initializers */
    long value;
    data_word *node;
    int cs, ce; /* Column start/end for error reporting */
    int expect_value; /* State machine for parsing comma-separated values */

    /* Defensive: validate all required pointers */
    if (!line || !DC || !data_image || !IC) {
        diag_error(&g_diag, "AS040",
                   file_name, line_no, 1,
                   NULL, 1, 1,
                   "internal API misuse in .mat handler");
        return -1;
    }

    /* Strip comments to avoid parsing them as matrix data */
    strip_inline_comment(line);

    /* Find the .mat directive in the line */
    p = line;
    mat = strstr(p, ".mat");
    if (!mat) return -1; /* Not a .mat directive */
    
    /* Move past .mat keyword and skip whitespace */
    p = mat + 4;
    p = skip_ws(p);

    /* Parse matrix dimensions: .mat[rows][cols] */
    dims_begin = p; /* Remember start for error reporting */
    
    /* Parse [rows] dimension */
    if (!parse_bracketed_pos_int(&p, &rows)) {
        int c = col_from_ptr(line, p);
        diag_error(&g_diag, "AS301",
                   file_name, line_no, c,
                   line, c, c,
                   ".mat expects '[rows]'");
        return -1;
    }
    
    /* Parse [cols] dimension */
    if (!parse_bracketed_pos_int(&p, &cols)) {
        int c = col_from_ptr(line, p);
        diag_error(&g_diag, "AS302",
                   file_name, line_no, c,
                   line, c, c,
                   ".mat expects '[cols]' after rows");
        return -1;
    }
    dims_end = p; /* Remember end of dimensions for error reporting */

    /* Check for integer overflow in matrix size calculation */
    if (rows > 0 && cols > 0) {
        if ((long)rows > (LONG_MAX_L / (long)cols)) {
            cs = col_from_ptr(line, dims_begin);
            ce = col_from_ptr(line, (dims_end ? dims_end - 1 : dims_begin));
            diag_error(&g_diag, "AS303",
                       file_name, line_no, cs,
                       line, cs, ce,
                       "matrix dimensions overflow capacity (%d x %d)", rows, cols);
            return -1;
        }
    }

    /* Calculate total number of elements needed */
    total_values = (long)rows * (long)cols;
    p = skip_ws(p);
    produced = 0; /* Track how many initializers we've processed */

    /* Process initializer list if present */
    if (*p != '\0') {
        expect_value = 1; /* Start expecting a value */
        
        while (*p != '\0') {
            /* Skip whitespace before next token */
            p = skip_ws(p);
            if (*p == '\0') break;

            if (expect_value) {
                /* Error: consecutive commas (missing value between them) */
                if (*p == ',') {
                    int c = col_from_ptr(line, p);
                    diag_error(&g_diag, "AS304",
                               file_name, line_no, c,
                               line, c, c,
                               "missing value between commas");
                    return -1;
                }

                /* Parse the numeric initializer */
                num_begin = p; /* Remember start for error reporting */
                value = strtol(p, &endptr, 10);

                /* Error: invalid number format */
                if (endptr == p) {
                    int c = col_from_ptr(line, p);
                    diag_error(&g_diag, "AS305",
                               file_name, line_no, c,
                               line, c, c,
                               "invalid number in .mat initializer");
                    return -1;
                }

                /* Calculate column positions for error reporting */
                cs = col_from_ptr(line, num_begin);
                ce = col_from_ptr(line, endptr - 1);

                /* Validate value is in allowed range */
                if (!in_data_range(value)) {
                    diag_error(&g_diag, "AS306",
                               file_name, line_no, cs,
                               line, cs, ce,
                               "value %ld out of data range [%d..%d]", value, DATA_MIN, DATA_MAX);
                    return -1;
                }

                /* Error: too many initializers provided */
                if (produced >= total_values) {
                    diag_error(&g_diag, "AS307",
                               file_name, line_no, cs,
                               line, cs, ce,
                               "too many initializers for %dx%d matrix", rows, cols);
                    return -1;
                }

                /* Create and store the data word */
                node = create_data_word((int)value, (*DC) + (*IC));
                add_data_word_to_end(data_image, node);
                (*DC)++;
                produced++;

                p = endptr; /* Move past parsed number */
                expect_value = 0; /* Next should be comma or end */
            } else {
                /* We're expecting a comma separator */
                if (*p == ',') {
                    last_comma = p;
                    p++; /* Move past comma */

                    /* Look ahead to check for trailing comma */
                    look = skip_ws(p);
                    if (*look == '\0') {
                        int c = col_from_ptr(line, last_comma);
                        diag_error(&g_diag, "AS309",
                                   file_name, line_no, c,
                                   line, c, c,
                                   "trailing comma in .mat initializer list");
                        return -1;
                    }
                    expect_value = 1; /* Next should be a value */
                } else {
                    /* Error: unexpected character after number */
                    int c = col_from_ptr(line, p);
                    diag_error(&g_diag, "AS308",
                               file_name, line_no, c,
                               line, c, c,
                               "unexpected character '%c' after initializer, expected comma", *p);
                    return -1;
                }
            }
        }
    }

    /* Fill remaining matrix elements with zeros if not enough initializers provided */
    while (produced < total_values) {
        node = create_data_word(0, (*DC) + (*IC));
        add_data_word_to_end(data_image, node);
        (*DC)++;
        produced++;
    }

    return 0; /* Success */
}

/* === Directive Processing Wrapper === */

int process_data_string_mat(CommandsTable commands_table, char line[], long *DC, data_word **data_image, long *IC,
                            long line_no , const char *file_am)
{    
    /* Route to appropriate directive processor based on command type */
    /* This centralizes directive handling and ensures consistent parameter passing */
    switch (commands_table)
    {
    case DATA:
        /* Process .data directive - handles numeric data declarations */
        return process_data_directive_at(line, DC, data_image, IC, file_am, line_no);
    case STRING:
        /* Process .string directive - handles string literal declarations */
        return process_string_directive_at(line, DC, data_image, IC, file_am, line_no);
    case MATRIX:
        /* Process .matrix directive - handles matrix data structure declarations */
        return process_matrix_directive_at(line, DC, data_image, IC, file_am, line_no);
    default:
        /* Unknown directive type - should not occur in normal operation */
        return -1;
    }
}

/* Main validation function - simplified and more readable */
boolean looks_like_matrix(const char *operand)
{
    matrix_parse_result_t result;
    
    /* Guard against NULL input */
    if (!operand) return FALSE;
    
    /* Delegate to full parser - if it succeeds, this looks like a matrix */
    result = parse_matrix_operand(operand);
    return (result.error == MATRIX_ERROR_NONE) ? TRUE : FALSE;
}

/* Improved addressing mode detection */
addr_mode get_addr_method(const char *operand)
{
    const char *trimmed;
    
    /* Handle NULL operand */
    if (!operand) return ADDR_NONE;
    
    /* Skip leading whitespace to get to actual content */
    trimmed = operand;
    while (*trimmed && isspace((unsigned char)*trimmed)) trimmed++;
    if (*trimmed == '\0') return ADDR_NONE; /* Empty or whitespace-only */

    /* Check for immediate addressing: #value */
    if (*trimmed == IMMEDIATE_PREFIX) return ADDR_IMMEDIATE;
    
    /* Check for simple register: r0, r1, etc. */
    if (is_register_token(trimmed)) return ADDR_REGISTER;

    /* Check for well-formed matrix operand: LABEL[rI][rJ] */
    if (looks_like_matrix(trimmed)) return ADDR_MATRIX;

    /* If contains brackets but not well-formed, still treat as matrix for error reporting */
    /* This allows proper error messages instead of treating as direct addressing */
    if (strchr(trimmed, '[')) return ADDR_MATRIX;

    /* Default to direct addressing for labels and other identifiers */
    return ADDR_DIRECT;
}

/* Enhanced matrix splitting with comprehensive error reporting */
int split_matrix_ex(const char *token,
                    char *out_label, size_t label_capacity,
                    char reg_a[REGBUF_MAX], char reg_b[REGBUF_MAX],
                    const char *filename, long line_number,
                    const char *line_text, int column_base)
{
    matrix_parse_result_t result;
    
    /* Initialize all output buffers to empty strings */
    if (out_label && label_capacity > 0) out_label[0] = '\0';
    if (reg_a) reg_a[0] = '\0';
    if (reg_b) reg_b[0] = '\0';

    /* Guard against NULL input */
    if (!token) return 0;

    /* Parse the matrix operand using the comprehensive parser */
    result = parse_matrix_operand(token);
    
    /* Report any parsing errors with detailed context information */
    if (result.error != MATRIX_ERROR_NONE) {
        report_matrix_error(&result, filename, line_number, line_text, column_base, token);
        return 0; /* Indicate failure */
    }

    /* Copy parsed components to output buffers safely */
    if (out_label && label_capacity > 0) {
        /* Copy label part (everything before first '[') */
        copy_slice_safe(result.label_start, result.label_end, out_label, label_capacity);
    }

    if (reg_a) {
        /* Copy first register (content of first []) */
        copy_slice_safe(result.reg1_start, result.reg1_end, reg_a, REGBUF_MAX);
    }
    
    if (reg_b) {
        /* Copy second register (content of second []) */
        copy_slice_safe(result.reg2_start, result.reg2_end, reg_b, REGBUF_MAX);
    }

    return 1; /* Indicate success */
}

int is_register_token(const char *token)
{
    /* Guard against NULL input */
    if (!token) return 0;

    /* Length guards: register must be exactly 2 characters (e.g., "r0", "r7") */
    if (token[0] == '\0') return 0; /* empty string */
    if (token[1] == '\0') return 0; /* only one character */
    if (token[2] != '\0') return 0; /* more than 2 characters */

    /* Syntax validation: first char must be 'r', second must be digit 0-7 */
    if (!IS_REGISTER_PREFIX(token[0])) return 0; /* Not starting with 'r' */
    if (!IS_VALID_REGISTER_CHAR(token[1])) return 0; /* Not valid register digit */

    return 1; /* Valid register format */
}

boolean is_immediate(const char *operand)
{
    /* Check if operand starts with immediate prefix character (typically '#') */
    return (operand && (*operand == IMMEDIATE_PREFIX)) ? TRUE : FALSE;
}

/* ===== STATIC HELPER FUNCTIONS ===== */

int find_bracket_pair(const char *start, const char **open, const char **close)
{
    const char *open_bracket;
    const char *close_bracket;
    
    /* Guard against NULL input */
    if (!start) return 0;
    
    /* Find opening bracket '[' starting from given position */
    open_bracket = strchr(start, MATRIX_BRACKET_OPEN);
    if (!open_bracket) return 0; /* No opening bracket found */
    
    /* Find closing bracket ']' after the opening bracket */
    close_bracket = strchr(open_bracket + 1, MATRIX_BRACKET_CLOSE);
    if (!close_bracket) return 0; /* No matching closing bracket */
    
    /* Return bracket positions through output parameters */
    if (open) *open = open_bracket;
    if (close) *close = close_bracket;
    return 1; /* Success - found matching pair */
}

int locate_all_brackets(const char *token, const char **b1, const char **b2, 
                              const char **b3, const char **b4)
{
    /* Find first bracket pair [reg1] in the token */
    if (!find_bracket_pair(token, b1, b2)) return 0;
    
    /* Find second bracket pair [reg2] starting after first closing bracket */
    if (!find_bracket_pair(*b2 + 1, b3, b4)) return 0;
    
    /* Verify proper ordering: [ ] [ ] (no overlapping or reversed brackets) */
    return (*b1 < *b2 && *b2 < *b3 && *b3 < *b4) ? 1 : 0;
}

void trim_whitespace(const char **start, const char **end)
{
    const char *s = *start;
    const char *e = *end;
    
    /* Move start pointer forward past any leading whitespace */
    while (s < e && isspace((unsigned char)*s)) s++;
    
    /* Move end pointer backward past any trailing whitespace */
    while (e > s && isspace((unsigned char)*(e-1))) e--;
    
    /* Update the pointers to the trimmed range */
    *start = s;
    *end = e;
}

register_validity_t validate_register_slice(const char *start, const char *end)
{
    size_t length = (size_t)(end - start);
    
    /* Empty slice is not a register */
    if (length == 0) return REG_INVALID_NOT_REGISTER;
    
    /* Check if it starts with register prefix 'r' */
    if (IS_REGISTER_PREFIX(*start)) {
        /* Valid register must be exactly 2 chars: 'r' + valid digit */
        if (length == 2 && IS_VALID_REGISTER_CHAR(start[1])) {
            return REG_VALID; /* Valid register like r0, r1, ..., r7 */
        }
        /* Has 'r' prefix but wrong format (r8, rx, r12, etc.) */
        return REG_INVALID_BAD_REGISTER;
    }
    
    /* Doesn't start with 'r', so not attempting to be a register */
    return REG_INVALID_NOT_REGISTER;
}

void copy_slice_safe(const char *start, const char *end, char *destination, size_t capacity)
{
    size_t length = (size_t)(end - start);
    
    /* Ensure we don't overflow the destination buffer */
    if (length >= capacity) length = capacity - 1;
    
    /* Copy the slice content */
    memcpy(destination, start, length);
    
    /* Null-terminate the destination string */
    destination[length] = '\0';
}

matrix_parse_result_t parse_matrix_operand(const char *operand)
{
    matrix_parse_result_t result;
    const char *b1, *b2, *b3, *b4; /* Bracket positions: [ ] [ ] */
    const char *p;
    size_t label_length;
    register_validity_t reg1_validity, reg2_validity;
    
    /* Initialize result structure to clean state */
    result.label_start = NULL;
    result.label_end = NULL;
    result.reg1_start = NULL;
    result.reg1_end = NULL;
    result.reg2_start = NULL;
    result.reg2_end = NULL;
    result.error = MATRIX_ERROR_NONE;
    result.error_pos = NULL;
    
    /* Find all four bracket positions: LABEL[reg1][reg2] */
    /*                                       ^b1 ^b2^b3^b4 */
    if (!locate_all_brackets(operand, &b1, &b2, &b3, &b4)) {
        result.error = MATRIX_ERROR_NO_BRACKETS;
        result.error_pos = operand;
        return result;
    }
    
    /* Extract and validate the label part (everything before first '[') */
    result.label_start = operand;
    result.label_end = b1;
    label_length = (size_t)(b1 - operand);
    
    /* Label cannot be empty */
    if (label_length == 0) {
        result.error = MATRIX_ERROR_EMPTY_LABEL;
        result.error_pos = operand;
        return result;
    }
    
    /* Label cannot exceed maximum allowed length */
    if (label_length > MAX_LABEL_LEN) {
        result.error = MATRIX_ERROR_LABEL_TOO_LONG;
        result.error_pos = operand;
        return result;
    }
    
    /* Extract and validate first register [reg1] */
    result.reg1_start = b1 + 1; /* Start after '[' */
    result.reg1_end = b2;       /* End at ']' */
    trim_whitespace(&result.reg1_start, &result.reg1_end);
    
    /* First register index cannot be empty */
    if (result.reg1_start >= result.reg1_end) {
        result.error = MATRIX_ERROR_EMPTY_INDEX;
        result.error_pos = b1;
        return result;
    }
    
    /* Validate that first index is a proper register */
    reg1_validity = validate_register_slice(result.reg1_start, result.reg1_end);
    if (reg1_validity != REG_VALID) {
        result.error = (reg1_validity == REG_INVALID_BAD_REGISTER) ? 
                      MATRIX_ERROR_INVALID_REGISTER : MATRIX_ERROR_NON_REGISTER;
        result.error_pos = result.reg1_start;
        return result;
    }
    
    /* Check for unwanted content between ][  (should be clean transition) */
    p = b2 + 1; /* Start after first ']' */
    while (p < b3 && isspace((unsigned char)*p)) p++; /* Skip whitespace */
    if (p != b3) {
        /* Found non-whitespace content between brackets */
        result.error = MATRIX_ERROR_JUNK_BETWEEN_BRACKETS;
        result.error_pos = b2 + 1;
        return result;
    }
    
    /* Extract and validate second register [reg2] */
    result.reg2_start = b3 + 1; /* Start after second '[' */
    result.reg2_end = b4;       /* End at second ']' */
    trim_whitespace(&result.reg2_start, &result.reg2_end);
    
    /* Second register index cannot be empty */
    if (result.reg2_start >= result.reg2_end) {
        result.error = MATRIX_ERROR_EMPTY_INDEX;
        result.error_pos = b3;
        return result;
    }
    
    /* Validate that second index is a proper register */
    reg2_validity = validate_register_slice(result.reg2_start, result.reg2_end);
    if (reg2_validity != REG_VALID) {
        result.error = (reg2_validity == REG_INVALID_BAD_REGISTER) ? 
                      MATRIX_ERROR_INVALID_REGISTER : MATRIX_ERROR_NON_REGISTER;
        result.error_pos = result.reg2_start;
        return result;
    }
    
    /* All validation passed - matrix operand is well-formed */
    result.error = MATRIX_ERROR_NONE;
    return result;
}

void report_matrix_error(const matrix_parse_result_t *result,
                         const char *filename,
                         long line_number,
                         const char *line_text,
                         int column_base,
                         const char *token)

{
    int start_col, end_col;
    
    /* Generate appropriate error message based on the specific error type */
    switch (result->error) {
        case MATRIX_ERROR_NO_BRACKETS:
            /* Missing brackets entirely - not a matrix format */
            diag_error(&g_diag, AS_E_MAT_BRACKETS, filename, line_number, column_base,
                      line_text, column_base, column_base,
                      "matrix operand must be of form LABEL[rI][rJ]");
            break;
            
        case MATRIX_ERROR_EMPTY_LABEL:
            /* Found brackets but no label before them: [r0][r1] */
            diag_error(&g_diag, AS_E_MAT_BRACKETS, filename, line_number, column_base,
                      line_text, column_base, COLUMN_AT(result->error_pos, column_base, token),
                      "missing label before '[' in matrix operand");
            break;
            
        case MATRIX_ERROR_LABEL_TOO_LONG:
            /* Label exceeds maximum allowed length */
            diag_error(&g_diag, "AS020", filename, line_number, column_base,
                      line_text, column_base, COLUMN_AT(result->label_end - 1, column_base, token),
                      "label too long (max %d chars)", MAX_LABEL_LEN);
            break;
            
        case MATRIX_ERROR_EMPTY_INDEX:
            /* Empty brackets found: LABEL[] or LABEL[r0][] */
            start_col = COLUMN_AT(result->error_pos, column_base, token);
            end_col = (result->error_pos == result->reg1_start - 1) ? 
                     COLUMN_AT(result->reg1_end, column_base, token) :
                     COLUMN_AT(result->reg2_end, column_base, token);
            diag_error(&g_diag, AS_E_MAT_EMPTY_INDEX, filename, line_number, start_col,
                      line_text, start_col, end_col,
                      "invalid index format: empty '[]'");
            break;
            
        case MATRIX_ERROR_INVALID_REGISTER:
            /* Register format attempted but invalid: r8, rx, r12, etc. */
            start_col = COLUMN_AT(result->error_pos, column_base, token);
            end_col = (result->error_pos == result->reg1_start) ? 
                     COLUMN_AT(result->reg1_end - 1, column_base, token) :
                     COLUMN_AT(result->reg2_end - 1, column_base, token);
            diag_error(&g_diag, AS_E_MAT_BAD_REG, filename, line_number, start_col,
                      line_text, start_col, end_col,
                      "invalid register in index (expected r0..r7)");
            break;
            
        case MATRIX_ERROR_NON_REGISTER:
            /* Non-register used as index: LABEL[x][y], LABEL[123][r0], etc. */
            start_col = COLUMN_AT(result->error_pos, column_base, token);
            end_col = (result->error_pos == result->reg1_start) ? 
                     COLUMN_AT(result->reg1_end - 1, column_base, token) :
                     COLUMN_AT(result->reg2_end - 1, column_base, token);
            diag_error(&g_diag, AS_E_MAT_NON_REG, filename, line_number, start_col,
                      line_text, start_col, end_col,
                      "non-register used as matrix index (expected r0..r7)");
            break;
            
        case MATRIX_ERROR_JUNK_BETWEEN_BRACKETS:
            /* Unexpected content between ][: LABEL[r0]junk[r1] */
            start_col = COLUMN_AT(result->error_pos, column_base, token);
            end_col = COLUMN_AT(result->reg2_start - 2, column_base, token); /* Position before second '[' */
            diag_error(&g_diag, AS_E_MAT_BETWEEN_BRACKETS, filename, line_number, start_col,
                      line_text, start_col, end_col,
                      "invalid index format between brackets (expected ...][...)");
            break;
            
        case MATRIX_ERROR_MISSING_CLOSE_BRACKET:
        case MATRIX_ERROR_MISSING_SECOND_OPEN:
        case MATRIX_ERROR_MISSING_SECOND_CLOSE:
        default:
            /* These errors should not occur with current bracket detection logic */
            /* Reserved for future enhancements or edge cases */
            break;
    }
}