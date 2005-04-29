#ifndef __GDB_MACOSX_NAT_DYLD_IO_H__
#define __GDB_MACOSX_NAT_DYLD_IO_H__

#include "symtab.h"

bfd *inferior_bfd (const char *name, CORE_ADDR addr, CORE_ADDR offset,
                   CORE_ADDR len);

#endif /* __GDB_MACOSX_NAT_DYLD_IO_H__ */
