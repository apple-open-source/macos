/* Native support for Mac OS X for GDB, the GNU debugger.
   Copyright (C) 1997-2002,
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

#ifndef _NM_NEXTSTEP_H_
#define _NM_NEXTSTEP_H_

#define ATTACH_DETACH
#define ATTACH_NO_WAIT

struct target_waitstatus;

extern int child_wait (int, struct target_waitstatus *, void *);
#define CHILD_WAIT

#define FETCH_INFERIOR_REGISTERS

#define CHILD_PREPARE_TO_STORE() \
	read_register_bytes (0, (char *) NULL, REGISTER_BYTES)

#define DISABLE_UNSETTABLE_BREAK(addr) 1

extern int macosx_try_start_dyld ();

#define SOLIB_ADD(filename, from_tty, targ, loadsyms) \
  macosx_try_start_dyld ()

#define SOLIB_IN_DYNAMIC_LINKER(pid,pc) \
(0)

#define SOLIB_UNLOADED_LIBRARY_PATHNAME(pid) \
(0)

#define SOLIB_LOADED_LIBRARY_PATHNAME(pid) \
(0)

#define SOLIB_CREATE_CATCH_LOAD_HOOK(pid,tempflag,filename,cond_string) \
   error("catch of library loads/unloads not yet implemented on this platform")

#define SOLIB_CREATE_CATCH_UNLOAD_HOOK(pid,tempflag,filename,cond_string) \
   error("catch of library loads/unloads not yet implemented on this platform")

extern void macosx_add_shared_symbol_files ();
#define ADD_SHARED_SYMBOL_FILES(args, from_tty) \
  macosx_add_shared_symbol_files ()

enum ptracereq {
  PTRACE_TRACEME = 0,		/* 0, by tracee to begin tracing */
  PTRACE_CHILDDONE = 0,		/* 0, tracee is done with his half */
  PTRACE_PEEKTEXT,		/* 1, read word from text segment */
  PTRACE_PEEKDATA,		/* 2, read word from data segment */
  PTRACE_PEEKUSER,		/* 3, read word from user struct */
  PTRACE_POKETEXT,		/* 4, write word into text segment */
  PTRACE_POKEDATA,		/* 5, write word into data segment */
  PTRACE_POKEUSER,		/* 6, write word into user struct */
  PTRACE_CONT,			/* 7, continue process */
  PTRACE_KILL,			/* 8, terminate process */
  PTRACE_SINGLESTEP,		/* 9, single step process */
  PTRACE_ATTACH,		/* 10, attach to an existing process */
  PTRACE_DETACH,		/* 11, detach from a process */
  PTRACE_SIGEXC,		/* 12, signals as exceptions for current process */
  PTRACE_THUPDATE,		/* 13, signal for thread */
  PTRACE_ATTACHEXC		/* 14, attach to running process with signals as exceptions */
};

void macosx_enable_page_protection_events (int pid);
void macosx_enable_page_protection_events (int pid);
int macosx_insert_watchpoint (CORE_ADDR addr, size_t len, int type);
int macosx_remove_watchpoint (CORE_ADDR addr, size_t len, int type);
int macosx_stopped_by_watchpoint (struct target_waitstatus *w, int, int);

char **macosx_process_completer (char *text, char *word);

#define TARGET_HAS_HARDWARE_WATCHPOINTS

#define TARGET_CAN_USE_HARDWARE_WATCHPOINT(type, cnt, ot) \
macosx_can_use_hw_watchpoint(type, cnt, ot)

#define TARGET_RANGE_PROFITABLE_FOR_HW_WATCHPOINT(pid, start, len) \
macosx_range_profitable_for_hw_watchpoint (pid, start, len)

#define STOPPED_BY_WATCHPOINT(w) \
macosx_stopped_by_watchpoint (&w, stop_signal, stepped_after_stopped_by_watchpoint)

#undef HAVE_STEPPABLE_WATCHPOINT
#define HAVE_NONSTEPPABLE_WATCHPOINT
#undef HAVE_CONTINUABLE_WATCHPOINT

#define TARGET_ENABLE_HW_WATCHPOINTS(pid) \
macosx_enable_page_protection_events (pid)

#define TARGET_DISABLE_HW_WATCHPOINTS(pid) \
macosx_disable_page_protection_events (pid)

#define target_insert_watchpoint(addr, len, type) \
macosx_insert_watchpoint (addr, len, type)

#define target_remove_watchpoint(addr, len, type) \
macosx_remove_watchpoint (addr, len, type)

#define PROCESS_COMPLETER macosx_process_completer
#define PROCESS_COMPLETER_WORD_BREAK_CHARACTERS gdb_completer_filename_word_break_characters

#define NM_NEXTSTEP

#endif /* _NM_NEXTSTEP_H_ */
