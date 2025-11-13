#include "words.h"

static diag g_diag = {0, NULL};

/* 
 * reg_id:
 *   Convert a register token (e.g., "r3") to its numeric ID.
 *   Returns -1 if token is not a valid register.
 */
int reg_id(const char *tok)
{
    if (!is_register_token(tok)) return -1;
    return tok[1] - REGISTER_MIN_ID_CHAR;
}

/* 
 * trim_inplace:
 *   Removes leading and trailing whitespace in-place.
 *   Returns pointer to first non-space character 
 *   (may be '\0' if empty string).
 */
char *trim_inplace(char *str)
{
    char *end;

    if (!str) return str;

    while (*str && isspace((unsigned char)*str))
        str++;                /* skip leading */

    if (*str == '\0')
        return str;           /* string was all spaces */

    end = str + strlen(str) - 1;

    while (end > str && isspace((unsigned char)*end))
        end--;                /* trim trailing */

    *(end + 1) = '\0';        /* reterminate */
    return str;
}

/* 
 * split_operands:
 *   Splits an instruction line into source/dest operands.
 *   Handles skipping mnemonic and ignoring inline comments.
 *   Special handling: commas inside matrix brackets [ ] are ignored.
 *   Returns:
 *     0       → no operands
 *     OPS_ONE → one operand
 *     OPS_TWO → two operands
 *   Also sets *out_src and *out_dst if non-NULL.
 */
int split_operands(char *text, char **out_src, char **out_dst)
{
    char *p, *sc, *comma, *left, *right, *cursor;
    int comma_count;

    if (out_src) *out_src = NULL;
    if (out_dst) *out_dst = NULL;
    if (!text) return 0;

    /* strip comment */
    sc = strchr(text, COMMENT_CHAR);
    if (sc) *sc = '\0';

    p = trim_inplace(text);
    if (*p == '\0') return 0;

    /* skip mnemonic token */
    while (*p && !isspace((unsigned char)*p)) p++;
    p = trim_inplace(p);
    if (*p == '\0') return 0;

    /* count commas (ignore inside [ ]) */
    comma_count = 0;
    cursor = p;
    while (*cursor) {
        if (*cursor == MATRIX_BRACKET_OPEN) {
            const char *close = strchr(cursor + 1, MATRIX_BRACKET_CLOSE);
            if (!close) break;        /* malformed matrix */
            cursor = (char*)close + 1;
            continue;
        }
        if (*cursor == OPERAND_DELIMITER) comma_count++;
        cursor++;
    }
    if (comma_count > 1) return 0;    /* too many commas */

    /* single operand */
    comma = strchr(p, OPERAND_DELIMITER);
    if (!comma) {
        left = trim_inplace(p);
        if (*left) {
            if (out_src) *out_src = left;
            return OPS_ONE;
        }
        return 0;
    }

    /* two operands */
    *comma = '\0';
    left  = trim_inplace(p);
    right = trim_inplace(comma + 1);

    if (*left == '\0' || *right == '\0') return 0;

    if (out_src) *out_src = left;
    if (out_dst) *out_dst = right;
    return OPS_TWO;
}

/* 
 * create_data_word:
 *   Allocate and initialize a new data_word node 
 *   for the data image linked list.
 */
data_word *create_data_word(int value, int address)
{
    data_word *new_node = (data_word *)malloc(sizeof(data_word));
    if (!new_node) {
        fprintf(stdout, "Error: Memory allocation failed in create_data_word\n");
        return NULL;
    }
    new_node->address = address;
    new_node->value   = (unsigned short)value;
    new_node->next    = NULL;
    return new_node;
}

/* ------------------------------------------------------------------
   Emitters and low-level writers
   These build machine_word nodes for the instruction image.
------------------------------------------------------------------ */

/* 
 * put_word:
 *   Append a new machine_word node to instruction image.
 *   Stores IC, payload (8 bits), ARE bits, and optional label.
 */
void put_word(long IC, unsigned payload, are_t are, machine_word **head, char label[])
{
    machine_word *mw = (machine_word *)malloc(sizeof(machine_word));
    machine_word *temp;
    unsigned packed;

    if (!mw) { perror("malloc"); exit(EXIT_FAILURE); }

    mw->address = (int)IC;
    packed      = payload & PAYLOAD8_MASK;
    mw->value   = packed;
    mw->are     = are;

    if (label != NULL) {
        strncpy(mw->label, label, MAX_LABEL_LEN - 1);
        mw->label[MAX_LABEL_LEN - 1] = '\0';
    } else {
        mw->label[0] = '\0';
    }

    mw->next = NULL;

    if (*head == NULL) {
        *head = mw;
    } else {
        temp = *head;
        while (temp->next != NULL) temp = temp->next;
        temp->next = mw;
    }
}

/* First instruction word: [ opcode(4) | src(2) | dest(2) ] */
void toBinaryFirstWord(CommandsTable op, addr_mode src, addr_mode dest,
                       long *IC, machine_word **head)
{
    machine_word *mw = (machine_word *)malloc(sizeof(machine_word));
    machine_word *temp;
    unsigned opc, s2, d2, packed;

    if (!mw) { perror("malloc"); exit(EXIT_FAILURE); }

    opc = ((unsigned)op) & OPCODE4_MASK;
    s2  = to2bits(src)   & SRC_DST2_MASK;
    d2  = to2bits(dest)  & SRC_DST2_MASK;

    mw->address = (int)(*IC);
    packed = ((opc << OPC4_SHIFT) | (s2 << SRC2_SHIFT) | d2) & PAYLOAD8_MASK;
    mw->value = packed;
    mw->are   = ARE_ABS;
    mw->label[0] = '\0';
    mw->next  = NULL;

    if (*head == NULL) *head = mw;
    else {
        temp = *head;
        while (temp->next != NULL) temp = temp->next;
        temp->next = mw;
    }
    (*IC)++;
}

/* 
 * parse_imm8:
 *   Parse an immediate operand (#N).
 *   Returns 1 on success, fills *out_val.
 *   Returns 0 if invalid syntax or out of range.
 */
int parse_imm8(const char *tok, int *out_val)
{
    char *end;
    long v;

    if (!tok || tok[0] != IMMEDIATE_PREFIX) return 0;

    v = strtol(tok + 1, &end, 10);
    if (end == tok + 1) return 0;  /* no digits */

    while (*end && isspace((unsigned char)*end)) end++;
    if (*end != '\0') return 0;    /* junk after number */

    if (v < (long)IMM8_MIN || v > (long)IMM8_MAX) return 0;

    if (out_val) *out_val = (int)v;
    return 1;
}

/* Emit one word holding an address (with ARE + optional symbol) */
void emit_addr_word(char symbol[], unsigned addr8, are_t are, long *IC, machine_word *img[])
{
    put_word(*IC, addr8, are, img, symbol);
    (*IC)++;
}

/* Emit one word holding an immediate (#N) */
void emit_imm_word(int val, long *IC, machine_word *img[])
{
    unsigned payload = (unsigned)(val) & PAYLOAD8_MASK;
    put_word(*IC, payload, ARE_ABS, img, NULL);
    (*IC)++;
}

/* Register encoding helpers */
unsigned reg_nibble_single(int r) { return (unsigned)(r) & REG_NIBBLE_MASK; }

unsigned reg_nibble_pair(int rA, int rB)
{
    unsigned a = ((unsigned)rA >> 1) & 3u;
    unsigned b = ((unsigned)rB >> 1) & 3u;
    return (a << SRC2_SHIFT) | b;
}

/* Emit word encoding a register pair (src+dest in one word) */
void emit_regcode_pair(int rA, int rB, long *IC, machine_word *img[])
{
    unsigned payload = ((unsigned)(rA & REG_NIBBLE_MASK) << REG_HIGH_SHIFT)
                     | (unsigned)(rB & REG_NIBBLE_MASK);
    put_word(*IC, payload, ARE_ABS, img, NULL);
    (*IC)++;
}

/* Emit word with single register in dest field */
void emit_regcode_single_dest(int r, long *IC, machine_word *img[])
{
    unsigned payload = ((unsigned)r) & REG_NIBBLE_MASK;
    put_word(*IC, payload, ARE_ABS, img, NULL);
    (*IC)++;
}

/* Emit word with single register in src field */
void emit_regcode_single_src(int r, long *IC, machine_word *img[])
{
    unsigned payload = (((unsigned)r) & REG_NIBBLE_MASK) << REG_HIGH_SHIFT;
    put_word(*IC, payload, ARE_ABS, img, NULL);
    (*IC)++;
}

/* 
 * emit_symbol_addr:
 *   Emit an address word for a symbol.
 *   Looks up the symbol in the table:
 *     - EXTERNAL → ARE_EXT
 *     - CODE/DATA/ENTRY → ARE_REL
 *     - otherwise → ARE_ABS
 *   If symbol not found, payload=0, ARE=ABS (placeholder).
 */
void emit_symbol_addr(const char *name, table *symtab, long *IC, machine_word **image)
{
    unsigned payload = 0u;
    are_t are = ARE_ABS; /* default placeholder */
    symbol_table *cur;

    if (symtab && name) {
        cur = symtab->head;
        while (cur) {
            if (strcmp(cur->key, name) == 0) {
                payload = (unsigned)(cur->value) & PAYLOAD8_MASK;
                switch (cur->type) {
                case EXTERNAL_SYMBOL: are = ARE_EXT; break;
                case CODE_SYMBOL:
                case DATA_SYMBOL:
                case ENTRY_SYMBOL:    are = ARE_REL; break;
                default:              are = ARE_ABS; break;
                }
                break; /* found */
            }
            cur = cur->next;
        }
    }

    emit_addr_word((char *)name, payload, are, IC, image);
}



/**
 * Main per-line instruction encoder for the second pass of the assembler.
 * Takes a parsed instruction line (WITHOUT leading label) and generates 
 * corresponding machine code words in the instruction image.
 * 
 * 
 * Encoding Process:
 * 1. Parse operands and determine addressing modes
 * 2. Validate opcode/operand compatibility 
 * 3. Emit first word (opcode + addressing mode bits)
 * 4. Emit additional words for each operand based on addressing mode
 * 5. Handle special optimizations (e.g., register-register operations)
 */
int process_commands_words(CommandsTable cmd,        /* Instruction opcode type */
                           char line[],              /* Instruction line (no label) */
                           long *IC,                 /* Instruction Counter - updated as words emitted */
                           machine_word **image,     /* Instruction image - linked list of generated words */
                           table *symtab_ptr,        /* Symbol table for address resolution */
                           const char *file_name,    /* Source filename for error reporting */
                           long line_no)             /* Line number for error reporting */
{
    int rc = -1;                             /* Return code - 0 for success, -1 for error */
    char *copy = NULL;                       /* Working copy of input line for parsing */
    char *src_tok = NULL;                    /* Source operand token */
    char *dst_tok = NULL;                    /* Destination operand token */
    int operand_count = 0;                   /* Number of operands found (0, 1, or 2) */
    addr_mode src_mode = ADDR_NONE;          /* Source operand addressing mode */
    addr_mode dst_mode = ADDR_NONE;          /* Destination operand addressing mode */

    /* Column positions for precise error reporting */
    int col_src_base = 0;                    /* Starting column of source operand */
    int col_dst_base = 0;                    /* Starting column of destination operand */

    /* Input validation */
    if (!line || !IC || !image) return -1;

    /* Create working copy for tokenization (preserves original for error reporting) */
    copy = my_strdup(line);
    if (!copy) return -1;

    /* PHASE 1: Operand Parsing and Classification */
    /* Extract operands from instruction line, handling comma separation */
    operand_count = split_operands(copy, &src_tok, &dst_tok);

    /* Calculate column positions for error reporting (relative to original line) */
    if (src_tok) col_src_base = 1 + (int)(src_tok - copy);
    if (dst_tok) col_dst_base = 1 + (int)(dst_tok - copy);

    /* Determine addressing modes for each operand */
    if (operand_count == OPS_TWO) {
        src_mode = get_addr_method(src_tok);     /* Classify source operand */
        dst_mode = get_addr_method(dst_tok);     /* Classify destination operand */
    } else if (operand_count == OPS_ONE) {
        dst_mode = get_addr_method(src_tok);     /* Single operand is treated as destination */
    }

    /* PHASE 2: Instruction Validation */
    /* Validate that the opcode supports the given addressing modes and operand count */
    if (validate_modes_for_opcode(cmd, operand_count, src_mode, dst_mode,
                                  file_name, line_no, copy)) {
        goto out;                            /* Validation failed - abort encoding */
    }

    /* PHASE 3: Machine Code Generation */
    /* Emit the first word containing opcode and addressing mode information */
    toBinaryFirstWord(cmd, src_mode, dst_mode, IC, image);

    /* OPTIMIZATION: Special case for register-to-register operations */
    /* When both operands are registers, they can be packed into a single additional word */
    if (operand_count == OPS_TWO && src_mode == ADDR_REGISTER && dst_mode == ADDR_REGISTER) {
        int rS = reg_id(src_tok);            /* Extract source register number */
        int rD = reg_id(dst_tok);            /* Extract destination register number */
        
        /* Validate register extraction succeeded */
        if (rS < 0 || rD < 0) {
            int LL = (int)strlen(line);
            diag_error(&g_diag, "AS023", file_name, line_no, DIAG_LEVEL_ERROR,
                       line, DIAG_COL_START, LL, "invalid register in 'rS,rD' pair");
            goto out;
        }
        
        /* Emit combined register word: source register in high nibble, dest in low nibble */
        put_word(*IC, ((unsigned)(rS & REG_NIBBLE_MASK) << REG_HIGH_SHIFT)
                      | (unsigned)(rD & REG_NIBBLE_MASK), ARE_ABS, image, NULL);
        (*IC)++;
        rc = 0;                              /* Success - optimization path complete */
        goto out;
    }

    /* PHASE 4: Source Operand Encoding (for two-operand instructions) */
    if (operand_count == OPS_TWO) {
        switch (src_mode) {
        case ADDR_IMMEDIATE: {
            /* Immediate addressing: #number -> encode number directly */
            int v;
            if (!parse_imm8(src_tok, &v)) {  /* Extract numeric value from #number */
                int LL = (int)strlen(line);
                diag_error(&g_diag, "AS023", file_name, line_no, DIAG_LEVEL_ERROR,
                           line, DIAG_COL_START, LL, 
                           "invalid immediate literal '%s' (expect #number)", src_tok);
                goto out;
            }
            emit_imm_word(v, IC, image);     /* Emit word with immediate value */
        } break;

        case ADDR_DIRECT:
            /* Direct addressing: symbol -> emit address word with symbol resolution */
            emit_symbol_addr(src_tok, symtab_ptr, IC, image);
            break;

        case ADDR_REGISTER: {
            /* Register addressing: rN -> emit register code */
            int r = reg_id(src_tok);         /* Extract register number from rN */
            if (r < 0) {
                int LL = (int)strlen(line);
                diag_error(&g_diag, "AS023", file_name, line_no, DIAG_LEVEL_ERROR,
                           line, DIAG_COL_START, LL, "invalid source register '%s'", src_tok);
                goto out;
            }
            emit_regcode_single_src(r, IC, image);  /* Emit word with source register code */
        } break;

        case ADDR_MATRIX: {
            /* Matrix addressing: BASE[rA][rB] -> emit base address + register pair */
            char base[64], ra[REGBUF_MAX], rb[REGBUF_MAX];  /* Parsed components */
            int rA, rB;                      /* Register numbers */

            /* Parse matrix syntax: extract base symbol and two register indices */
            if (!split_matrix_ex(src_tok, base, sizeof base, ra, rb,
                                file_name, line_no, copy, col_src_base)) {
                /* split_matrix_ex reports detailed syntax errors */
                goto out;
            }

            /* Extract register numbers from register names */
            rA = reg_id(ra); 
            rB = reg_id(rb);
            if (rA < 0 || rB < 0) {
                int LL = (int)strlen(line);
                diag_error(&g_diag, "AS023", file_name, line_no, DIAG_LEVEL_ERROR,
                        line, DIAG_COL_START, LL,
                        "invalid matrix index register(s) '%s','%s'", ra, rb);
                goto out;
            }

            /* Matrix addressing requires two words: */
            emit_symbol_addr(base, symtab_ptr, IC, image);  /* 1. Base address word */
            emit_regcode_pair(rA, rB, IC, image);           /* 2. Index register pair word */
        } break;

        default:
            break;
        }
    }

    /* PHASE 5: Destination Operand Encoding (for all instructions with operands) */
    if (operand_count >= OPS_ONE) {
        /* For single operand instructions, src_tok contains the destination */
        const char *tok = (operand_count == OPS_TWO ? dst_tok : src_tok);
        
        switch (dst_mode) {
        case ADDR_IMMEDIATE: {
            /* Immediate destination (rare, but supported by some instructions like CMP) */
            int v;
            if (!parse_imm8(tok, &v)) {
                int LL = (int)strlen(line);
                diag_error(&g_diag, "AS023", file_name, line_no, DIAG_LEVEL_ERROR,
                           line, DIAG_COL_START, LL, 
                           "invalid immediate literal '%s' (expect #number)", tok);
                goto out;
            }
            emit_imm_word(v, IC, image);
        } break;

        case ADDR_DIRECT:
            /* Direct destination: symbol address */
            emit_symbol_addr((char *)tok, symtab_ptr, IC, image);
            break;

        case ADDR_REGISTER: {
            /* Register destination */
            int r = reg_id(tok);
            if (r < 0) {
                int LL = (int)strlen(line);
                diag_error(&g_diag, "AS023", file_name, line_no, DIAG_LEVEL_ERROR,
                           line, DIAG_COL_START, LL, "invalid destination register '%s'", tok);
                goto out;
            }
            emit_regcode_single_dest(r, IC, image);  /* Emit word with destination register code */
        } break;

        case ADDR_MATRIX: {
            /* Matrix destination: BASE[rA][rB] */
            char base[64], ra[REGBUF_MAX], rb[REGBUF_MAX];
            int rA, rB;

            /* Parse matrix syntax */
            if (!split_matrix_ex(tok, base, sizeof base, ra, rb,
                                file_name, line_no, copy, col_dst_base)) {
                goto out; /* Error already reported */
            }

            /* Validate register components */
            rA = reg_id(ra); 
            rB = reg_id(rb);
            if (rA < 0 || rB < 0) {
                int LL = (int)strlen(line);
                diag_error(&g_diag, "AS023", file_name, line_no, DIAG_LEVEL_ERROR,
                        line, DIAG_COL_START, LL,
                        "invalid matrix index register(s) '%s','%s'", ra, rb);
                goto out;
            }

            /* Emit matrix addressing words */
            emit_symbol_addr(base, symtab_ptr, IC, image);   /* Base address */
            emit_regcode_pair(rA, rB, IC, image);            /* Index registers */
        } break;

        default:
            break;
        }
    }

    rc = 0;                                  /* Success - all operands encoded */

out:
    /* Cleanup: free working copy */
    if (copy) free(copy);
    return rc;
}
