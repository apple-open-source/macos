#ifndef _STUFF_SYMBOL_H_
#define _STUFF_SYMBOL_H_

#include "stuff/target_arch.h"
#include <mach-o/nlist.h>

struct symbol {
    char *name;
    char *indr_name;
    nlist_t nl;
};

#endif /* _STUFF_SYMBOL_H_ */
