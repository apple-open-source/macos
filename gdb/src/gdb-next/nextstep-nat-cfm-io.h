#include "symtab.h"

bfd *pef_inferior_bfd PARAMS ((const char *name, CORE_ADDR addr, CORE_ADDR len));
void pef_read PARAMS ((const char *name, CORE_ADDR addr, CORE_ADDR len));
