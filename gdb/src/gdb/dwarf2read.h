#ifndef DWARF2READ_H
#define DWARF2READ_H 1

#include "bfd.h"

extern asection *dwarf_frame_section;
extern asection *dwarf_eh_frame_section;

/* APPLE LOCAL debug map take a bfd parameter */
char *dwarf2_read_section (struct objfile *, bfd *, asection *);

/* When expanding a psymtab to a symtab we get the
   addresses of all the symbols in the executable (the "final"
   addresses) and the minimal symbols (linker symbols, etc) from
   the .o file and create a table of these address tuples (plus the
   symbol name) to allow for fixing up all the addresses in the .o
   file's DWARF.  NB: I don't think I actually use the symbol name
   once this array is created, just the address tuples.  But for
   now I'll keep it around to aid in debugging.  */

struct oso_final_addr_tuple {
  /* Linker symbol name aka minsym aka physname */
  char *name;
  /* Start address in the .o file */

  CORE_ADDR oso_low_addr;
  /* End address in the .o file (the same as the start address of the
     next highest oso_final_addr_tuple).  */
  CORE_ADDR oso_high_addr;

  /* Low address in the final, linked image */
  CORE_ADDR final_addr;
  /* Whether this function is present in the final executable or not.  */
  int present_in_final;
};


/* This array is sorted by OSO_ADDR so that we can do quick lookups of 
   addresses we find in the .o file DWARF entries.  */

struct oso_to_final_addr_map {
  struct partial_symtab *pst;
  int entries;
  struct oso_final_addr_tuple *tuples;
  
  /* PowerPC has a "-mlong-branch" option that generates trampoline code for
     long branches. This trampoline code follows the code for a function. The 
     address range for the function in the .o DWARF currently spans this
     trampoline code. The address range for the function in the linked 
     executable can be smaller than the address range in the .o DWARF when 
     our newest linker determines that the long branch trampoline code is not
     needed. In such cases, the trampoline code gets stripped, and the branch 
     to the trampoline code gets fixed to branch directly to its destination. 
     This optimization can cause the size of the function to be reduced in
     the final executable and in the debug map. There didn't seem to be any
     easy way to propagate the function size (N_FUN end stab with no name) 
     found in the debug map up to the debug map address translation functions
     whilst only using only minimal and partial symbols. Minimal symbols are 
     made from the normal nlist entries (non-STAB) and these nlist entries 
     have no size (even though the min symbols in gdb have a size member that
     we could use). Partial symbols for functions get made from debug map 
     N_FUN entries and the ending N_FUN entry that contains the new, 
     and possibly smaller, function size get used only to set the max 
     partial symtab address. Partial symbols in gdb also don't have a size 
     member. The oso_to_final_addr_map.tuples array is sorted by OSO_LOW_ADDR. 
     The FINAL_ADDR_INDEX member was added so we can quickly search the 
     oso_to_final_addr_map.tuples array by FINAL_ADDR. The FINAL_ADDR_INDEX
     contains zero based indexes into the oso_to_final_addr_map.tuples array
     and gets created in CONVERT_OSO_MAP_TO_FINAL_MAP. When translating a high
     pc address in TRANSLATE_DEBUG_MAP_ADDRESS, we can shorten a function's 
     address range by making sure the next FINAL_ADDR is not less than our
     current value for our translated high pc.  */

  /* In short: An array of element index values sorted by final address.  */
  int *final_addr_index;
  
  /* Reuse the above for the common symbols where the .o file has no
     address (just 0x0) -- so for COMMON_PAIRS we're storing the
     symbol name and the final address.  This array is sorted by
     the symbol name. */

  int common_entries;
  struct oso_final_addr_tuple *common_pairs;
};

int translate_debug_map_address (struct oso_to_final_addr_map *,
                                 CORE_ADDR, CORE_ADDR *, int);

#endif /* DWARF2READ_H */
