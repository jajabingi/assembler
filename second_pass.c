#include "second_pass.h"
#include "assembler.h"
static diag g_diag = {0, NULL};

/* -----------------------------------------------------------
   Base-4 helpers 
   ----------------------------------------------------------- */
char quad_letter(unsigned d) {
    return (char)(BASE4_FIRST_CHAR + (d & BASE4_DIGIT_MASK));
}

void to_base4_letters(unsigned value, int width, char *out){
    int i;
    if (!out || width <= 0) return;
    for (i = width - 1; i >= 0; --i) {
        out[i] = quad_letter(value & BASE4_DIGIT_MASK);
        value >>= 2; /* divide by 4 */
    }
    out[width] = '\0';
}

/* a=0, b=1, c=2, d=3; writes most-significant digit first (no fixed width) */
void to_base4_letters_header(unsigned value, char *out){
    char tmp[BASE4_TMP_MAX];
    int i = 0, j;

    if (!out) return;

    if (value == 0) {
        out[0] = BASE4_FIRST_CHAR; /* 'a' */
        out[1] = '\0';
        return;
    }

    /* collect least-significant base-4 digits into tmp */
    while (value > 0 && (size_t)i < (sizeof(tmp) - 1u)) {
        tmp[i++] = quad_letter(value & BASE4_DIGIT_MASK);
        value >>= 2; /* divide by 4 */
    }

    /* reverse into out (now MSB-first) */
    for (j = 0; j < i; ++j) out[j] = tmp[i - 1 - j];
    out[i] = '\0';
}

/* payload(8) + ARE(2) -> 5 letters (10-bit) */
void word10_to_letters_code(unsigned payload8, unsigned are2, char out5[WORD10_STR_LEN]){
    unsigned v10 = ((payload8 & PAYLOAD8_MASK) << 2) | (are2 & ARE2_MASK);
    (void)out5; /* signature promises enough room */
    to_base4_letters(v10, (int)WORD10_DIGITS, out5);
}

/* Data words are already 10 bits; ARE is not encoded in data lines */
void word10_to_letters_data(unsigned payload10, unsigned are2_unused, char out5[WORD10_STR_LEN]){
    unsigned v10 = (payload10 & WORD10_MASK);
    (void)are2_unused;
    to_base4_letters(v10, (int)WORD10_DIGITS, out5);
}


/* -----------------------------------------------------------
   Extern usage list
   ----------------------------------------------------------- */

void add_to_extern_usage_list(extern_usage **head, int addr){
    extern_usage *new_node;
    extern_usage *temp;

    new_node = (extern_usage *)malloc(sizeof(extern_usage));
    if (!new_node) {
        fprintf(stderr, "Memory allocation failed for extern_usage\n");
        exit(EXIT_FAILURE);
    }

    new_node->addr = addr;
    new_node->next = NULL;

    if (*head == NULL) {
        *head = new_node;
        return;
    }

    temp = *head;
    while (temp->next != NULL) temp = temp->next;
    temp->next = new_node;
}


/* -----------------------------------------------------------
   Label resolution (second pass)
   ----------------------------------------------------------- */

void resolve_labels_in_place(machine_word *head, table *symtab, extern_list *ext_list){
    machine_word *mw;
    for (mw = head; mw; mw = mw->next) {
        symbol_table *hit, *cur;
        extern_list *ext_ptr;

        if (mw->label[0] == '\0') continue; /* not a symbol word */

        hit = NULL;
        cur = symtab ? symtab->head : NULL;
        while (cur){
            if (strcmp(cur->key, mw->label) == 0){ hit = cur; break; }
            cur = cur->next;
        }

        /* Printed object format: payload MUST be 0 on symbol words */
        mw->value = 0u;

        if (hit){
            /* symbol known in our table → relocatable ('b') */
            mw->are   = ARE_REL;
            mw->value = hit->value;
        } else {
            /* search extern list */
            ext_ptr = ext_list;
            while (ext_ptr){
                if (strcmp(ext_ptr->labels, mw->label) == 0){
                    mw->are = ARE_EXT; /* last letter 'c' */
                    add_to_extern_usage_list(&(ext_ptr->addr_head), mw->address);
                    break;
                }
                ext_ptr = ext_ptr->next;
            }
            /* If not found in externs, mw->are should already reflect encoding rules
               (commonly absolute/zero); leave as-is to avoid masking earlier errors. */
        }
    }
}


/* -----------------------------------------------------------
   .ob writer: base-4 ONLY (address & 5-letter code)
   data printed after code; data ARE=0 (absolute).
   ----------------------------------------------------------- */

int ensure_addr_width_ok(int addr_width, const char *stem) {
    if (addr_width <= 0 || addr_width + 1 > (int)ADDR_STR_MAX) {
        diag_error(&g_diag, "AS419",
                   stem, 0, DIAG_LEVEL_ERROR,
                   NULL, DIAG_COL_START, DIAG_COL_START,
                   "invalid address width %d (max supported %u)", addr_width, (unsigned)ADDR_STR_MAX - 1u);
        return 0;
    }
    return 1;
}

/* Write object file in base-4 letters format */
void write_ob_base4_only(const char *stem,
                         const machine_word *code_head,
                         const data_word *data_head,
                         long IC_final,
                         int addr_width,
                         long DC_final)
{
    char path[OUTPUT_PATH_CAP];
    FILE *out;
    const machine_word *mw;
    const data_word *dw;
    char addr_buf[ADDR_STR_MAX];
    char word_str[WORD10_STR_LEN];

    long code_words = (IC_final > IC_INIT_VALUE) ? (IC_final - IC_INIT_VALUE) : 0;
    long data_words = (DC_final > 0) ? DC_final : 0;
    long total_words = code_words + data_words;

    /* capacity check: 4^(addr_width) - 1 */
    long width_capacity = (long)1 << (2 * addr_width);
    long max_allowed = (width_capacity - 1 < OB_WORD_LIMIT)
                       ? width_capacity - 1 : OB_WORD_LIMIT;

    if (total_words > max_allowed) {
        diag_error(&g_diag, "AS_OB_TOO_LONG",
                   stem, 1, 1, "<object>", 1, 1,
                   "object has %ld words (code %ld + data %ld) but limit is %ld",
                   total_words, code_words, data_words, max_allowed);
        return;
    }

    /* build output path manually */
    strncpy(path, stem, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';
    strncat(path, ".ob", sizeof(path) - strlen(path) - 1);

    out = fopen(path, "w");
    if (!out) {
        /* no strerror() — just our own message */
        diag_error(&g_diag, "AS_FILE_OPEN",
                   stem, 1, 1, "<object>", 1, 1,
                   "failed to open output file '%s' for writing", path);
        return;
    }

    /* header (counts as base-4 strings) */
    to_base4_letters_header((unsigned)IC_final,  addr_buf);
    to_base4_letters_header((unsigned)DC_final,  word_str);
    fprintf(out, "\t%s\t%s\n", addr_buf, word_str);

    /* code section */
    mw = code_head;
    while (mw){
        to_base4_letters((unsigned)mw->address, addr_width, addr_buf);
        word10_to_letters_code((mw->value & PAYLOAD8_MASK),
                               (mw->are   & ARE2_MASK),
                               word_str);
        fprintf(out, "%s %s\n", addr_buf, word_str);
        mw = mw->next;
    }

    /* data section (ARE=0) */
    dw = data_head;
    while (dw){
        to_base4_letters((unsigned)dw->address, addr_width, addr_buf);
        word10_to_letters_data((unsigned)(dw->value), 0u, word_str);
        fprintf(out, "%s %s\n", addr_buf, word_str);
        dw = dw->next;
    }

    fclose(out);
}



/* -----------------------------------------------------------
   Entry label completion
   ----------------------------------------------------------- */

void complete_entry_labels(table *symtab, entry_list *ent_list){
    symbol_table *sym;
    while (ent_list){
        sym = symtab ? symtab->head : NULL;
        while (sym){
            if (strcmp(ent_list->labels, sym->key) == 0){
                ent_list->addr = sym->value;
                break;
            }
            sym = sym->next;
        }
        ent_list = ent_list->next;
    }
}


/* -----------------------------------------------------------
   Small helpers
   ----------------------------------------------------------- */

int has_any_extern_usage(const extern_list *xs) {
    const extern_list *e;
    for (e = xs; e; e = e->next) {
        if (e->addr_head) return 1; /* at least one address use */
    }
    return 0;
}

int path_too_long(const char *stem, const char *suffix, size_t cap) {
    size_t need = strlen(stem) + strlen(suffix) + 1; /* +NUL */
    return need > cap;
}


/* -----------------------------------------------------------
   .ext writer: only if there is at least one extern usage
   ----------------------------------------------------------- */

void write_ext_base4_only(const char *stem, extern_list *ext_list){
    char path[OUTPUT_PATH_CAP];
    FILE *out;
    char a4[EXT_ENT_ADDR_WIDTH + 1];
    extern_usage *temp;

    if (!ext_list || !has_any_extern_usage(ext_list)) return; /* nothing to write */

    if (path_too_long(stem, ".ext", sizeof(path))) {
        diag_error(&g_diag, "AS420",
                   stem, 0, DIAG_LEVEL_ERROR,
                   NULL, DIAG_COL_START, DIAG_COL_START,
                   "output path too long for '%s.ext'", stem);
        return;
    }

    sprintf(path, "%s.ext", stem);

    out = fopen(path, "w");
    if (!out) {
        diag_error(&g_diag, "AS421",
                   stem, 0, DIAG_LEVEL_ERROR,
                   NULL, DIAG_COL_START, DIAG_COL_START,
                   "cannot create output file '%s'", path);
        return;
    }

    while (ext_list) {
        temp = ext_list->addr_head;
        while (temp) {
            to_base4_letters((unsigned)temp->addr, EXT_ENT_ADDR_WIDTH, a4);
            fprintf(out, "%s %s\n", ext_list->labels, a4);
            temp = temp->next;
        }
        ext_list = ext_list->next;
    }

    fclose(out);
}


/* -----------------------------------------------------------
   .ent writer: only if there is at least one entry
   ----------------------------------------------------------- */

void write_ent_base4_only(const char *stem, entry_list *ent_list){
    char path[OUTPUT_PATH_CAP];
    FILE *out;
    char a4[EXT_ENT_ADDR_WIDTH + 1];

    if (!ent_list) return; /* nothing to write */

    if (path_too_long(stem, ".ent", sizeof(path))) {
        diag_error(&g_diag, "AS422",
                   stem, 0, DIAG_LEVEL_ERROR,
                   NULL, DIAG_COL_START, DIAG_COL_START,
                   "output path too long for '%s.ent'", stem);
        return;
    }

    sprintf(path, "%s.ent", stem);

    out = fopen(path, "w");
    if (!out) {
        diag_error(&g_diag, "AS423",
                   stem, 0, DIAG_LEVEL_ERROR,
                   NULL, DIAG_COL_START, DIAG_COL_START,
                   "cannot create output file '%s'", path);
        return;
    }

    while (ent_list) {
        to_base4_letters((unsigned)ent_list->addr, EXT_ENT_ADDR_WIDTH, a4);
        fprintf(out, "%s %s\n", ent_list->labels, a4);
        ent_list = ent_list->next;
    }

    fclose(out);
}


/* -----------------------------------------------------------
   Second pass driver
   ----------------------------------------------------------- */

int second_pass(const char *stem,
                table *symtab,
                machine_word *code_head,
                data_word *data_head,
                long IC_final,
                long DC_final /* unused here, printed in header */,
                extern_list *ext_list,
                entry_list *ent_list)
{
    /* Defensive: if any errors exist, don’t proceed */
    if (g_diag.error_count > 0) {
        diag_error(&g_diag, "AS050",
                   stem, 0, DIAG_LEVEL_ERROR,
                   NULL, DIAG_COL_START, DIAG_COL_START,
                   "second pass skipped due to previous errors");
        return -1;
    }

    resolve_labels_in_place(code_head, symtab, ext_list);

    complete_entry_labels(symtab, ent_list);

    /* OB: use default width unless you pass a different one elsewhere */
    write_ob_base4_only(stem, code_head, data_head, IC_final, OB_ADDR_WIDTH_DEFAULT, DC_final);
    write_ent_base4_only(stem, ent_list);
    write_ext_base4_only(stem, ext_list);

    printf("\n\n");
    print_machine_word_list(code_head);
    printf("\n\n");
    return 0;
}

