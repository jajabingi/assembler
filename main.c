/* main.c - entry point for the assembler (per-file isolation, always continue) */

#include "assembler.h"      /* IC_INIT_VALUE, MAX_CODE_IMAGE, types */
#include "process_macros.h" /* process_macros, MacroTable, init/free */
#include "first_pass.h"     /* first_pass, relocate_data, second_pass */
#include "diag.h"           /* diag, diag_init, diag_error */
#include "cleanup.h"        /* free_* helpers */
#include <stdio.h>

/* Global diag instance is defined in diag.c */
static diag g_diag = {0, NULL};

int main(int argc, char *argv[])
{
    int i;

    /* Initialize diagnostics */
    diag_init(&g_diag, stdout);

    /* Require at least one input filename (stem) */
    if (argc < 2) {
        diag_error(&g_diag, "AS000",
                   "<cmdline>", 1, 1,
                   NULL, 1, 1,
                   "Usage: %s <input_file> ...", (argc > 0 ? argv[0] : "assembler"));
        return 1;
    }

    /* Process each input file completely before moving to the next */
    for (i = 1; i < argc; i++) {
        const char   *stem = argv[i];

        /* -------- Per-file state (must not leak across files) -------- */
        MacroTable     macro_table;                  /* macros are per-file */
        table          symbol_table;                 /* symbol table per-file */
        data_word     *data_img   = NULL;            /* data image head */
        machine_word  *ic_image[MAX_CODE_IMAGE];     /* instruction image */
        entry_list    *ent_list   = NULL;            /* .entry list */
        extern_list   *ext_list   = NULL;            /* .extern list */
        long           IC = IC_INIT_VALUE;           /* instruction counter */
        long           DC = 0;                       /* data counter */
        int            j;

        for (j = 0; j < MAX_CODE_IMAGE; ++j) ic_image[j] = NULL;

        init_macro_table(&macro_table);
        init_symbol_table(&symbol_table);

        /* ---------------------- Macro expansion ---------------------- */
        if (process_macros(stem, &macro_table) != 0) {
            diag_error(&g_diag, "AS101", stem, 1, 1, NULL, 1, 1,
                       "macro processing failed for '%s'", stem);
            goto cleanup; /* proceed to next file */
        }
        printf("Macro processing completed successfully for %s.\n", stem);

        /* ------------------------ First pass ------------------------ */
        if (first_pass(stem, &symbol_table, &data_img, ic_image,
                       &IC, &DC, &ext_list, &ent_list) != 0) {
            diag_error(&g_diag, "AS102", stem, 1, 1, NULL, 1, 1,
                       "first pass failed for '%s'", stem);
            goto cleanup; /* proceed to next file */
        }
        printf("First pass completed successfully for %s.\n", stem);

        /* Fix data base after final IC is known */
        /*relocate_data(&symbol_table, data_img, IC);*/

cleanup:
        /* -------- Per-file teardown -------- */
        free_macro_table(&macro_table);
        free_code_image(ic_image, MAX_CODE_IMAGE);
        free_data_image(data_img);
        free_entry_list(ent_list);
        free_extern_list(ext_list);
        free_symbol_table(&symbol_table);

        /* loop continues automatically to next argv[i] */
    }

    /* Exit non-zero if any errors were reported in any file */
    return (g_diag.error_count > 0) ? 1 : 0;
}
