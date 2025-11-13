#ifndef WORDS_H
#define WORDS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "assembler.h"
#include "diag.h"
#include "opmodes.h"
#include "second_pass.h" 



#define DIAG_LEVEL_ERROR 1
#define DIAG_COL_START  1
#define PAYLOAD8_MASK   0xFFu
#define ARE2_MASK       0x03u
#define OPCODE4_MASK    0x0Fu
#define SRC_DST2_MASK   0x03u
#define SRC2_SHIFT      2
#define OPC4_SHIFT      4
#define REG_NIBBLE_MASK 0x0Fu
#define REG_HIGH_SHIFT  4


/* =========================
 * Base-4 & word layout (no magic numbers)
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

/* Optional debug prints */
#define PASS2_DEBUG             0


/* =========================
 * Text/lex helpers
 * ========================= */

/**
 * trim_inplace
 * Remove leading/trailing ASCII whitespace in-place.
 * @param str  Mutable C string (may be NULL).
 * @return     Pointer to first non-space char within the same buffer (or "\0").
 */
char *trim_inplace(char *str);

/**
 * split_operands
 * Split the operand portion of an instruction line into up to two tokens.
 * Honors matrix brackets so commas inside LABEL[rA][rB] don’t split.
 * @param text     Mutable line (comment stripped by ';' / COMMENT_CHAR).
 * @param out_src  Out: source token pointer (may be NULL).
 * @param out_dst  Out: destination token pointer (may be NULL).
 * @return         0=no operands, 1=single-operand (dest), 2=src+dest (OPS_*).
 */
int split_operands(char *text, char **out_src, char **out_dst);


/* =========================
 * Data image utilities
 * ========================= */

/**
 * create_data_word
 * Allocate and return a data_word node.
 * @param value   10-bit payload (caller ensures proper range/mask).
 * @param address Logical address to store in the node.
 * @return        Newly allocated node or NULL on allocation failure.
 */
data_word *create_data_word(int value, int address);


/* =========================
 * Encoders & emitters (machine words)
 * ========================= */

/**
 * put_word
 * Append a machine word node to the image list.
 * @param IC      Absolute address to assign to the word.
 * @param payload 8-bit payload (masked inside).
 * @param are     ARE field (2 bits).
 * @param head    In/out list head.
 * @param label   Optional symbol label for later resolution (may be NULL).
 */
void put_word(long IC, unsigned payload, are_t are, machine_word **head, char label[]);

/**
 * toBinaryFirstWord
 * Encode the first instruction word: [ opcode(4) | src(2) | dest(2) ].
 * @param op      Opcode enum.
 * @param src     Addressing mode of source operand.
 * @param dest    Addressing mode of destination operand.
 * @param IC      In/out instruction counter (incremented).
 * @param head    In/out machine-word list head.
 */
void toBinaryFirstWord(CommandsTable op, addr_mode src, addr_mode dest, long *IC, machine_word **head);

/**
 * parse_imm8
 * Parse immediate literal of the form "#<signed_decimal>" into 8-bit range.
 * @param tok      Token string starting with IMMEDIATE_PREFIX (e.g., '#').
 * @param out_val  Out: parsed int on success.
 * @return         1 on success, 0 on failure (syntax or out-of-range).
 */
int parse_imm8(const char *tok, int *out_val);

/**
 * emit_addr_word
 * Emit an address word containing an 8-bit address and ARE.
 * @param symbol Optional label to attach to the word (for extern tracking).
 */
void emit_addr_word(char symbol[], unsigned addr8, are_t are, long *IC, machine_word **img);

/**
 * emit_imm_word
 * Emit an absolute immediate 8-bit word (#number).
 */
void emit_imm_word(int val, long *IC, machine_word **img);

/**
 * reg_nibble_single
 * Map a register id to its 4-bit nibble code (implementation-specific).
 */
unsigned reg_nibble_single(int r);

/**
 * reg_nibble_pair
 * Pack two register codes into a single byte (hi: rA, lo: rB).
 * Note: Your ISA-specific mapping is handled inside.
 */
unsigned reg_nibble_pair(int rA, int rB);

/**
 * emit_regcode_pair
 * Emit a combined register word for two-register forms (src,dst).
 */
void emit_regcode_pair(int rA, int rB, long *IC, machine_word **img);

/**
 * emit_regcode_single_dest
 * Emit a register word when only the destination register appears.
 */
void emit_regcode_single_dest(int r, long *IC, machine_word **img);

/**
 * emit_regcode_single_src
 * Emit a register word when only the source register appears.
 */
void emit_regcode_single_src(int r, long *IC, machine_word **img);

/**
 * emit_symbol_addr
 * Emit a placeholder/relocatable/external address word for a symbol.
 * If symbol is in table: ARE_REL + value; if external: ARE_EXT; else ABS+0.
 */
void emit_symbol_addr(const char *name, table *symtab, long *IC, machine_word **image);


/* =========================
 * Base-4 formatting (object file view)
 * ========================= */

/* =========================
 * Line-level encoder and pass driver
 * ========================= */

/**
 * process_commands_words
 * Encode a single instruction line (no leading label) into machine words.
 * Validates addressing modes using opmodes rules before emitting.
 * @param cmd        Opcode.
 * @param line       Mutable line to parse (operands only; comments ok).
 * @param IC         In/out instruction counter (incremented by emitted words).
 * @param image      In/out machine-word list head.
 * @param symtab_ptr Symbol table (for direct/matrix base or externs).
 * @param file_name  Source file name (for diagnostics).
 * @param line_no    1-based line number (for diagnostics).
 * @return           0 on success, -1 on fatal/alloc error.
 */
int process_commands_words(CommandsTable cmd,
                           char line[],
                           long *IC,
                           machine_word **image,
                           table *symtab_ptr,
                           const char *file_name,
                           long line_no);


#endif
