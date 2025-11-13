#include "opmodes.h"   
#include "diag.h"
#include <string.h>
#include <stdio.h>

static diag g_diag = {0 , NULL};

/* -----------------------------------------------------------
   Addressing-mode rule table (one constant per mnemonic)
   ----------------------------------------------------------- */
static const opcode_mode RULE_MOV  = { "mov",  OPS_TWO,  OPS_TWO,  AM_ALL,                   (AM_DIR|AM_MAT|AM_REG) };
static const opcode_mode RULE_CMP  = { "cmp",  OPS_TWO,  OPS_TWO,  AM_ALL,                   AM_ALL };
static const opcode_mode RULE_ADD  = { "add",  OPS_TWO,  OPS_TWO,  (AM_DIR|AM_MAT|AM_REG),   (AM_DIR|AM_MAT|AM_REG) };
static const opcode_mode RULE_SUB  = { "sub",  OPS_TWO,  OPS_TWO,  (AM_DIR|AM_MAT|AM_REG),   (AM_DIR|AM_MAT|AM_REG) };
static const opcode_mode RULE_LEA  = { "lea",  OPS_TWO,  OPS_TWO,  (AM_DIR|AM_MAT),          (AM_DIR|AM_MAT|AM_REG) };
static const opcode_mode RULE_CLR  = { "clr",  OPS_ONE,  OPS_ONE,  AM_NONE,                  (AM_DIR|AM_MAT|AM_REG) };
static const opcode_mode RULE_NOT  = { "not",  OPS_ONE,  OPS_ONE,  AM_NONE,                  (AM_DIR|AM_MAT|AM_REG) };
static const opcode_mode RULE_INC  = { "inc",  OPS_ONE,  OPS_ONE,  AM_NONE,                  (AM_DIR|AM_MAT|AM_REG) };
static const opcode_mode RULE_DEC  = { "dec",  OPS_ONE,  OPS_ONE,  AM_NONE,                  (AM_DIR|AM_MAT|AM_REG) };
static const opcode_mode RULE_JMP  = { "jmp",  OPS_ONE,  OPS_ONE,  AM_NONE,                  (AM_DIR|AM_MAT|AM_REG) };
static const opcode_mode RULE_BNE  = { "bne",  OPS_ONE,  OPS_ONE,  AM_NONE,                  (AM_DIR|AM_MAT|AM_REG) };
static const opcode_mode RULE_JSR  = { "jsr",  OPS_ONE,  OPS_ONE,  AM_NONE,                  (AM_DIR|AM_MAT|AM_REG) };
static const opcode_mode RULE_RED  = { "red",  OPS_ONE,  OPS_ONE,  AM_NONE,                  (AM_DIR|AM_MAT|AM_REG) };
static const opcode_mode RULE_PRN  = { "prn",  OPS_ONE,  OPS_ONE,  AM_NONE,                  AM_ALL };
static const opcode_mode RULE_RTS  = { "rts",  OPS_ZERO, OPS_ZERO, AM_NONE,                  AM_NONE };
static const opcode_mode RULE_STOP = { "stop", OPS_ZERO, OPS_ZERO, AM_NONE,                  AM_NONE };

/* -----------------------------------------------------------
   Map enum → rule (no assumptions on enum numeric values)
   ----------------------------------------------------------- */
const opcode_mode *opcode_rule(CommandsTable cmd) {
    switch (cmd) {
        case MOV:  return &RULE_MOV;
        case CMP:  return &RULE_CMP;
        case ADD:  return &RULE_ADD;
        case SUB:  return &RULE_SUB;
        case LEA:  return &RULE_LEA;
        case CLR:  return &RULE_CLR;
        case NOT:  return &RULE_NOT;
        case INC:  return &RULE_INC;
        case DEC:  return &RULE_DEC;
        case JMP:  return &RULE_JMP;
        case BNE:  return &RULE_BNE;
        case JSR:  return &RULE_JSR;
        case RED:  return &RULE_RED;
        case PRN:  return &RULE_PRN;
        case RTS:  return &RULE_RTS;
        case STOP: return &RULE_STOP;
        case DATA: case STRING: case MATRIX: case ENTRY: case EXTERN:
        default:   return NULL; /* not a code instruction */
    }
}

/* -----------------------------------------------------------
   Helpers
   ----------------------------------------------------------- */

/* build a CSV like "0,1,3" for allowed modes. */
void mask_to_csv(unsigned mask, char *out, size_t n) {
    size_t w = 0;
    int first = 1;
    unsigned m;

    if (!out || n == 0u) return;
    out[0] = '\0';

    for (m = AM_FIRST_BIT; m <= AM_LAST_BIT; ++m) {
        if (mask & AM_BIT(m)) {
            if (!first) {
                if (w + 1 < n) out[w++] = ',';
                else { out[n-1] = '\0'; return; }
            }
            if (w + 1 < n) out[w++] = (char)('0' + m);
            else { out[n-1] = '\0'; return; }
            first = 0;
        }
    }
    if (w < n) out[w] = '\0'; else out[n-1] = '\0';
}

/* Map addr_mode -> bit in AM_* mask (robust to enum changes) */
unsigned int mode_bit(addr_mode m) {
    switch (m) {
        case ADDR_IMMEDIATE: return AM_IMM;
        case ADDR_DIRECT:    return AM_DIR;
        case ADDR_MATRIX:    return AM_MAT;
        case ADDR_REGISTER:  return AM_REG;
        default:             return AM_NONE; 
    }
}

/* Validate a single operand against the rule’s mask.
   Returns 1 if error was reported, 0 otherwise. */
int validate_operand(const opcode_mode *rule,
                            addr_mode mode,
                            int is_source,
                            const char *file_name,
                            long line_no,
                            const char *full_line_text)
{
    const char *operand_type_str = is_source ? "source" : "destination";
    unsigned int required_mask = is_source ? rule->src_mask : rule->dst_mask;
    char allowed_modes_str[AM_MASK_CSV_MAX_CHARS];
    int line_len = full_line_text ? (int)strlen(full_line_text) : 0;

    if (mode == ADDR_NONE) {
        diag_error(&g_diag, "AS023", file_name, line_no, DIAG_LEVEL_ERROR,
                   full_line_text, DIAG_COL_START, line_len,
                   "missing %s operand for '%s'", operand_type_str, rule->name);
        return 1;
    }

    /* Single source of truth: the bitmask. */
    if ((required_mask & mode_bit(mode)) == 0u) {
        mask_to_csv(required_mask, allowed_modes_str, sizeof(allowed_modes_str));
        diag_error(&g_diag, "AS022", file_name, line_no, DIAG_LEVEL_ERROR,
                   full_line_text, DIAG_COL_START, line_len,
                   "illegal addressing mode for %s of '%s' (allowed: %s)",
                   operand_type_str, rule->name, allowed_modes_str);
        return 1;
    }

    return 0;
}

/* -----------------------------------------------------------
   Public validator (returns 1 if any error was reported)
   ----------------------------------------------------------- */
int validate_modes_for_opcode(CommandsTable cmd,
                              int operand_count,
                              addr_mode src_mode,
                              addr_mode dst_mode,
                              const char *file_name,
                              long line_no,
                              const char *full_line_text)
{
    const opcode_mode *rule;
    int line_len = full_line_text ? (int)strlen(full_line_text) : 0;
    int had_error = 0;

    rule = opcode_rule(cmd);

    /* Missing rule is an internal error: report once and stop validation. */
    if (!rule) {
        diag_error(&g_diag, "AS001", file_name, line_no, DIAG_LEVEL_ERROR,
                   full_line_text, DIAG_COL_START, line_len,
                   "internal error: no validation rule for opcode id %d", (int)cmd);
        return 1;
    }

    /* 1) Operand count */
    if ((unsigned)operand_count < rule->min_ops || (unsigned)operand_count > rule->max_ops) {
        if (rule->min_ops == rule->max_ops) {
            diag_error(&g_diag, "AS003", file_name, line_no, DIAG_LEVEL_ERROR,
                       full_line_text, DIAG_COL_START, line_len,
                       "wrong operand count for '%s' (got %d, expected %u)",
                       rule->name, operand_count, rule->min_ops);
        } else {
            diag_error(&g_diag, "AS003", file_name, line_no, DIAG_LEVEL_ERROR,
                       full_line_text, DIAG_COL_START, line_len,
                       "wrong operand count for '%s' (got %d, expected %u..%u)",
                       rule->name, operand_count, rule->min_ops, rule->max_ops);
        }
        return 1; /* don’t cascade mode errors when count is wrong */
    }

    /* 2) Mode legality according to actual operand_count */
    switch (operand_count) {
        case OPS_ZERO:
            /* e.g., rts/stop — nothing to validate */
            break;

        case OPS_ONE:
            /* For single-operand ops, ensure we didn’t accidentally parse a source. */
            if (src_mode != ADDR_NONE) {
                diag_error(&g_diag, "AS024", file_name, line_no, DIAG_LEVEL_ERROR,
                           full_line_text, DIAG_COL_START, line_len,
                           "unexpected source operand for single-operand instruction '%s'", rule->name);
                had_error = 1;
            }
            /* Single operand is treated as destination. */
            if (validate_operand(rule, dst_mode, 0 /* dest */,
                                 file_name, line_no, full_line_text))
                had_error = 1;
            break;

        case OPS_TWO:
            if (validate_operand(rule, src_mode, 1 /* src */,
                                 file_name, line_no, full_line_text))
                had_error = 1;
            if (validate_operand(rule, dst_mode, 0 /* dest */,
                                 file_name, line_no, full_line_text))
                had_error = 1;
            break;

        default:
            /* Should be unreachable due to earlier count check, but guard anyway. */
            diag_error(&g_diag, "AS003", file_name, line_no, DIAG_LEVEL_ERROR,
                       full_line_text, DIAG_COL_START, line_len,
                       "unsupported operand count %d for '%s'", operand_count, rule->name);
            had_error = 1;
            break;
    }

    return had_error; /* 0 = OK, 1 = at least one error reported */
}
