#ifndef __GDB_MACOSX_TDEP_H__
#define __GDB_MACOSX_TDEP_H__

struct internal_nlist;
struct external_nlist;

void macosx_internalize_symbol 
PARAMS ((struct internal_nlist *in, struct external_nlist *ext, bfd *abfd));

const char *dyld_symbol_stub_function_name (CORE_ADDR pc);
CORE_ADDR dyld_symbol_stub_function_address (CORE_ADDR pc, const char **name);

#endif /* __GDB_MACOSX_TDEP_H__ */
