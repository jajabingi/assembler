#include "assembler.h"


/* ---------- פונקציה עזר להדפסת מספר בבינארי ---------- */
 void print_binary(unsigned int value, int bits) {
    int i;
    for (i = bits - 1; i >= 0; i--) {
        putchar((value >> i) & 1 ? '1' : '0');
    }
}

/* ---------- הדפסת data_word ---------- */
void print_data_word_list(const data_word *head) {
    const data_word *current = head;

    while (current != NULL) {
        printf("Addr: %d | Value: %5hu (0x%04X) | Bin: ", 
               current->address, current->value, current->value);
        print_binary(current->value, 16);
        putchar('\n');
        current = current->next;
    }
}

/* ---------- הדפסת machine_word ---------- */
void print_machine_word_list(const machine_word *head) {
    const machine_word *current;
    unsigned int combined; /* 10 bits: value<<2 | are */

    current = head;
    while (current != NULL) {
        combined = (((unsigned int)current->value) & 0xFFu) << 2;
        combined |= ((unsigned int)current->are) & 0x3u;

        printf("Addr: %d | Bits10: ", current->address);
        print_binary(combined, 10);              /* מדפיס 10 ביטים עם אפסים מובילים */
        printf(" | Label: %s\n", current->label);

        current = current->next;
    }
}

/* ---------- הדפסת entry_list ---------- */
void print_entry_list(const entry_list *head)
{
    const entry_list *current = head;

    while (current != NULL)
    {
        printf("Label: %-30s | Addr: %d\n", current->labels, current->addr);
        current = current->next;
    }
}

/* ---------- הדפסת extern_usage ---------- */
void print_extern_usage(const extern_usage *head)
{
    const extern_usage *current = head;

    printf("  [Extern Usage Addresses]\n");
    while (current != NULL)
    {
        printf("    -> Addr: %d\n", current->addr);
        current = current->next;
    }
}

/* ---------- הדפסת extern_list ---------- */
void print_extern_list(const extern_list *head)
{
    const extern_list *current = head;

    while (current != NULL)
    {
        printf("Label: %-30s\n", current->labels);
        print_extern_usage(current->addr_head); /* מדפיס גם את רשימת הכתובות */
        current = current->next;
    }
}



void print_symbol_table(const table *tab)
{
    symbol_table *curr = tab->head;

    printf("Symbol Table (%d entries):\n", tab->size);
    printf("--------------------------------------------------\n");
    printf("%-20s | %-10s | %-10s\n", "Label", "Value", "Type");
    printf("--------------------------------------------------\n");

    while (curr != NULL)
    {
        const char *type_str;

        switch (curr->type)
        {
        case CODE_SYMBOL:
            type_str = "CODE";
            break;
        case DATA_SYMBOL:
            type_str = "DATA";
            break;
        case EXTERNAL_SYMBOL:
            type_str = "EXTERNAL";
            break;
        case ENTRY_SYMBOL:
            type_str = "ENTRY";
            break;
        default:
            type_str = "UNKNOWN";
            break;
        }

        printf("%-20s | %-10ld | %-10s\n", curr->key, curr->value, type_str);
        curr = curr->next;
    }

    printf("--------------------------------------------------\n");
}
