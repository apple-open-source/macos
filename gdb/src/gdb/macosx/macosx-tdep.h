#ifndef __GDB_MACOSX_TDEP_H__
#define __GDB_MACOSX_TDEP_H__

#if defined (TARGET_POWERPC)
#include "ppc-macosx-tdep.h"
#elif defined (TARGET_I386)
#include "i386-macosx-tdep.h"
#endif

struct internal_nlist;
struct external_nlist;
extern enum gdb_osabi osabi_seen_in_attached_dyld;

void macosx_internalize_symbol (struct internal_nlist * in, int *sect_p,
                                struct external_nlist * ext, bfd *abfd);

const char *dyld_symbol_stub_function_name (CORE_ADDR pc);
CORE_ADDR dyld_symbol_stub_function_address (CORE_ADDR pc, const char **name);

CORE_ADDR macosx_skip_trampoline_code (CORE_ADDR pc);
int macosx_in_solib_return_trampoline (CORE_ADDR pc, char *name);
int macosx_in_solib_call_trampoline (CORE_ADDR pc, char *name);
int macosx_record_symbols_from_sect_p (bfd *abfd, unsigned char macho_type, unsigned char macho_sect);

#endif /* __GDB_MACOSX_TDEP_H__ */
