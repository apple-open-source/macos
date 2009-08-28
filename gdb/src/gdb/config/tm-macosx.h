/* Target support for Mac OS X for GDB, the GNU debugger.
   Copyright (C) 1997-2002, 2005,
   Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _TM_NEXTSTEP_H_
#define _TM_NEXTSTEP_H_

#include "macosx-tdep.h"

#define BLOCK_ADDRESS_ABSOLUTE 1
#define BELIEVE_PCC_PROMOTION 1

#define INTERNALIZE_SYMBOL(intern, sect_p, extern, abfd) \
macosx_internalize_symbol (&intern, &sect_p, extern, abfd)

#define SOFUN_ADDRESS_MAYBE_MISSING
#define TEXT_SEGMENT_NAME "LC_SEGMENT.__TEXT"
#define TEXT_SECTION_NAME "LC_SEGMENT.__TEXT.__text"
#define COALESCED_TEXT_SECTION_NAME "LC_SEGMENT.__TEXT.__textcoal_nt"
#define DATA_SECTION_NAME "LC_SEGMENT.__DATA.__data"
#define BSS_SECTION_NAME "LC_SEGMENT.__DATA.__bss"

#define TM_NEXTSTEP 1
#define MACOSX_DYLD 1
#define ATTACH_DETACH
#define ATTACH_NO_WAIT

#define SOLIB_ADD(filename, from_tty, targ, loadsyms) \
  macosx_solib_add (filename, from_tty, targ, loadsyms)

#define SOLIB_IN_DYNAMIC_LINKER(pid,pc) 0

#define SOLIB_UNLOADED_LIBRARY_PATHNAME(pid) 0

#define SOLIB_LOADED_LIBRARY_PATHNAME(pid) 0

#define SOLIB_CREATE_CATCH_LOAD_HOOK(pid,tempflag,filename,cond_string) \
  error("catch of library loads/unloads not yet implemented on this platform")

#define SOLIB_CREATE_CATCH_UNLOAD_HOOK(pid,tempflag,filename,cond_string) \
  error("catch of library loads/unloads not yet implemented on this platform")

extern void macosx_add_shared_symbol_files ();
#define ADD_SHARED_SYMBOL_FILES(args, from_tty) \
  macosx_add_shared_symbol_files (args, from_tty)

/* Dummy definition */
const char *macosx_pc_solib (CORE_ADDR addr);
#define PC_SOLIB(addr) ((char *) macosx_pc_solib (addr))

#endif /* _TM_NEXTSTEP_H_ */
