#include <mach-o/nlist.h>
#include <stuff/bool.h>

/*
 * Data structures to perform selective stripping of symbol table entries.
 */
struct symbol_list {
    char *name;		/* name of the global symbol */
    struct nlist *sym;	/* pointer to the nlist structure for this symbol */
    enum bool seen;	/* set if the symbol is seen in the input file */
};

__private_extern__ void setup_symbol_list(
    char *file,
    struct symbol_list **list,
    unsigned long *size);

__private_extern__ int symbol_list_bsearch(
    const char *name,
    const struct symbol_list *sym);
