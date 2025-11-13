/* cleanup.h - per-file resource cleanup helpers (no static) */
#ifndef CLEANUP_H
#define CLEANUP_H

#include "assembler.h"

void init_symbol_table(table *symtab);
/* Free an instruction image array (size = MAX_CODE_IMAGE).
   Frees each non-NULL machine_word* and NULLs slots. */
void free_code_image(machine_word *ic_image[], int size);

/* Free linked list of data words: assumes node has a 'next' field. */
void free_data_image(data_word *head);

/* Free the symbol table; implement according to your table structure.
   If your table is a linked list, this walks and frees nodes.
   If your table is a hash table, adjust the buckets loop accordingly. */
void free_symbol_table(table *symtab);

/* Free linked list of .entry records: assumes node has a 'next' field. */
void free_entry_list(entry_list *ents);

/* Free linked list of .extern usages: assumes node has a 'next' field. */
void free_extern_list(extern_list *exts);

#endif /* CLEANUP_H */
