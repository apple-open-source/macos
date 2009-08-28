#ifndef __GDB_MACOSX_TDEP_H__
#define __GDB_MACOSX_TDEP_H__

#if defined (TARGET_POWERPC)
#include "ppc-macosx-tdep.h"
#elif defined (TARGET_I386)
#include "i386-macosx-tdep.h"
#elif defined (TARGET_ARM)
#include "arm-macosx-tdep.h"
#else 
#error "Unrecognized target architecture"
#endif

#include "symtab.h"

struct internal_nlist;
struct external_nlist;
struct objfile;
extern enum gdb_osabi osabi_seen_in_attached_dyld;

void macosx_internalize_symbol (struct internal_nlist * in, int *sect_p,
                                struct external_nlist * ext, bfd *abfd);

const char *dyld_symbol_stub_function_name (CORE_ADDR pc);
CORE_ADDR dyld_symbol_stub_function_address (CORE_ADDR pc, const char **name);

CORE_ADDR macosx_skip_trampoline_code (CORE_ADDR pc);
int macosx_in_solib_return_trampoline (CORE_ADDR pc, char *name);
int macosx_in_solib_call_trampoline (CORE_ADDR pc, char *name);
int macosx_record_symbols_from_sect_p (bfd *abfd, unsigned char macho_type, 
				       unsigned char macho_sect);

char *macosx_locate_dsym (struct objfile *objfile);
struct objfile *macosx_find_objfile_matching_dsym_in_bundle (char *dsym_bundle_path, 
							     char **out_full_path);

char *macosx_kext_info (const char *filename,
                  const char **bundle_executable_name_from_plist,
                  const char **bundle_identifier_name_from_plist);

enum gdb_osabi
generic_mach_o_osabi_sniffer (bfd *abfd, enum bfd_architecture arch, 
			      unsigned long mach_32,
			      unsigned long mach_64,
			      int (*query_64_bit_fn) ());

int
fast_show_stack_trace_prologue (unsigned int count_limit, 
				unsigned int print_limit,
				unsigned int wordsize,
				CORE_ADDR *sigtramp_start_ptr,
				CORE_ADDR *sigtramp_end_ptr,
				unsigned int *count,
				struct frame_info **fi,
				void (print_fun) (struct ui_out * uiout, int frame_num,
						  CORE_ADDR pc, CORE_ADDR fp));

int
macosx_enable_exception_callback (enum exception_event_kind kind, int enable);
struct symtabs_and_lines *
macosx_find_exception_catchpoints (enum exception_event_kind kind,
                                   struct objfile *restrict_objfile);
struct exception_event_record *
macosx_get_current_exception_event ();

#endif /* __GDB_MACOSX_TDEP_H__ */
