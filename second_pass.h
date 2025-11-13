#ifndef SECOND_PASS_H
#define SECOND_PASS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "assembler.h"
#include "diag.h"

/* =========================
 * Base-4 & word layout
 * ========================= */
#define BASE4_FIRST_CHAR        'a'      /* a=0, b=1, c=2, d=3 */
#define BASE4_RADIX             4u
#define BASE4_DIGIT_MASK        0x3u     /* 2 bits per digit */
#define BASE4_SHIFT_BITS        2

/* 10-bit instruction/data word (payload+ARE) */
#define WORD10_TOTAL_BITS       10u
#define WORD10_DIGITS           5u       /* 10 bits / 2 = 5 base-4 letters */
#define WORD10_STR_LEN          (WORD10_DIGITS + 1u)

#define PAYLOAD8_MASK           0xFFu
#define ARE2_MASK               0x03u
#define WORD10_MASK             0x3FFu

/* Address widths & buffers */
#define OB_ADDR_WIDTH_DEFAULT   4        /* e.g., 'bcba' */
#define EXT_ENT_ADDR_WIDTH      4
#define ADDR_STR_MAX            32       /* address buffer cap incl. NUL */

/* Paths & temps */
#define OUTPUT_PATH_CAP         512
/* max base-4 digits for sizeof(unsigned)*8 bits is bits/2, +1 for NUL */
#define BASE4_TMP_MAX           (((sizeof(unsigned)*8) + 1u)/2u + 1u)


#define DIAG_LEVEL_ERROR        1

#define DIAG_COL_START          1

#define OB_WORD_LIMIT 255  

/* =========================
 * Function prototypes
 * ========================= */

/**
 * to_base4_letters
 * Convert an unsigned value to fixed-width base-4 using letters (a=0..d=3).
 * @param value  Value to encode (lower width*2 bits are used).
 * @param width  Number of base-4 letters to emit.
 * @param out    Caller-provided buffer of at least width+1 bytes.
 */
void to_base4_letters(unsigned value, int width, char *out);

/**
 * to_base4_letters_header
 * Convert an unsigned value to variable-width base-4 (no zero-padding).
 * @param value  Value to encode.
 * @param out    Caller-provided buffer (enough for BASE4_TMP_MAX).
 */
void to_base4_letters_header(unsigned value, char *out);

/**
 * word10_to_letters_code
 * Convert payload(8)+ARE(2) into 5 base-4 letters (“object code” style).
 * @param payload8  Low 8 bits.
 * @param are2      Low 2 bits.
 * @param out5      Buffer of size WORD10_STR_LEN (5+1).
 */
void word10_to_letters_code(unsigned payload8, unsigned are2, char out5[WORD10_STR_LEN]);

/**
 * word10_to_letters_data
 * Convert a 10-bit data payload into 5 base-4 letters (ARE not encoded).
 * @param payload10 10-bit value.
 * @param are2_unused Ignored (kept for symmetry with _code).
 * @param out5      Buffer of size WORD10_STR_LEN (5+1).
 */
void word10_to_letters_data(unsigned payload10, unsigned are2_unused, char out5[WORD10_STR_LEN]);


/* =========================
 * Extern/entry resolution and writers
 * ========================= */

/**
 * add_to_extern_usage_list
 * Append an address to an extern label’s usage chain.
 */
void add_to_extern_usage_list(extern_usage **head, int addr);

/**
 * resolve_labels_in_place
 * Resolve symbol words inside the code image (sets value/ARE, logs extern uses).
 */
void resolve_labels_in_place(machine_word *head, table *symtab, extern_list *ext_list);

/**
 * write_ob_base4_only
 * Write the object file (.ob): base-4 address and 5-letter code/data lines.
 * Header prints IC_final and DC_final in base-4 (variable width).
 */
void write_ob_base4_only(const char *stem,
                         const machine_word *code_head,
                         const data_word *data_head,
                         long IC_final,
                         int addr_width,
                         long DC_final);

/**
 * write_ext_base4_only
 * Write the extern file (.ext) only if there is at least one extern usage.
 */
void write_ext_base4_only(const char *stem, extern_list *ext_list);

/**
 * write_ent_base4_only
 * Write the entry file (.ent) only if there is at least one entry symbol.
 */
void write_ent_base4_only(const char *stem, entry_list *ent_list);

/**
 * complete_entry_labels
 * Fill the address for each entry label from the symbol table.
 */
void complete_entry_labels(table *symtab, entry_list *ent_list);




/**
 * second_pass
 * Run label resolution and write .ob/.ent/.ext outputs if no prior errors.
 * @return 0 on success, -1 if skipped due to existing errors.
 */
int second_pass(const char *stem,
                table *symtab,
                machine_word *code_head,
                data_word *data_head,
                long IC_final,
                long DC_final,
                extern_list *ext_list,
                entry_list *ent_list);



#endif /* SECOND_PASS_H */
