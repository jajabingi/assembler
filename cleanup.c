/* cleanup.c - per-file resource cleanup helpers */

#include "cleanup.h"
#include <stdlib.h>

/* === Linked List Management (Memory Freeing) === */
/* Frees all allocated machine_word entries in the instruction image array. */
void free_code_image(machine_word *ic_image[], int size) {
    int i;

    if (!ic_image) return;

    for (i = 0; i < size; i++) {
        if (ic_image[i] != NULL) {
            /* If machine_word has dynamically allocated subfields,
               free them here before freeing the struct itself. */
            free(ic_image[i]);
            ic_image[i] = NULL;  /* reset pointer after free */
        }
    }
}


/* Frees all nodes in an entry_list. */
void free_entry_list(entry_list *head) {
    entry_list *current = head;
    entry_list *next;
    
    /* Walk through list, freeing each node safely */
    while (current != NULL) {
        next = current->next; /* Save next pointer before freeing current */
        free(current);
        current = next;
    }
}

/* Frees all nodes in an extern_list. (Assumes addr_list is also freed). */
void free_extern_list(extern_list *head) {
    extern_list *current = head;
    extern_list *next;
    
    /* Free each extern node in the list */
    while (current != NULL) {
        next = current->next; /* Preserve next pointer */
        /* Note: If addr_head points to allocated memory, it must be freed here too. */
        /* free_addr_list(current->addr_head); */
        free(current);
        current = next;
    }
}

/* Frees all nodes in a data_word list. */
void free_data_image(data_word *head) {
    data_word *current = head;
    data_word *next;
    
    /* Free entire chain of data words */
    while (current != NULL) {
        next = current->next; /* Save next before freeing current */
        free(current);
        current = next;
    }
}

/* Frees all nodes and associated keys in a symbol table. */
void free_symbol_table(table *tab) {
    symbol_table *current;
    symbol_table *next;
    
    /* Handle NULL table gracefully */
    if (!tab) return;
    
    /* Free each symbol entry and its dynamically allocated key */
    current = tab->head;
    while (current != NULL) {
        next = current->next; /* Save next pointer */
        free(current->key);   /* Free dynamically allocated key string */
        free(current);        /* Free the symbol table entry itself */
        current = next;
    }
    
    /* Reset table to empty state */
    tab->head = NULL;
    tab->size = 0;
}


/* initialize an empty symbol table wrapper */
void init_symbol_table(table *symtab) {
    if (!symtab) return;
    symtab->head = NULL;
    symtab->size = 0;
}
