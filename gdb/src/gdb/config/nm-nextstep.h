/* Native support for NextStep/Rhapsody for GDB, the GNU debugger.
   Copyright (C) 1997
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

extern int next_pidget (int tpid);
#define PIDGET(pid) \
  next_pidget (pid)

#define ATTACH_DETACH
#define ATTACH_NO_WAIT

struct target_waitstatus;
extern int child_wait (int, struct target_waitstatus *);
#define CHILD_WAIT

#define FETCH_INFERIOR_REGISTERS

#define CHILD_PREPARE_TO_STORE() \
	read_register_bytes (0, (char *) NULL, REGISTER_BYTES)

#define DISABLE_UNSETTABLE_BREAK(addr) 1

extern void next_mach_try_start_dyld ();

#define SOLIB_ADD(filename, from_tty, targ) \
  next_mach_try_start_dyld ()

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

extern void next_mach_add_shared_symbol_files ();
#define ADD_SHARED_SYMBOL_FILES(args, from_tty) \
  next_mach_add_shared_symbol_files ()

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
  PTRACE_GETREGS,		/* 12, get all registers */
  PTRACE_SETREGS,		/* 13, set all registers */
  PTRACE_GETFPREGS,		/* 14, get all floating point regs */
  PTRACE_SETFPREGS,		/* 15, set all floating point regs */
  PTRACE_READDATA,		/* 16, read data segment */
  PTRACE_WRITEDATA,		/* 17, write data segment */
  PTRACE_READTEXT,		/* 18, read text segment */
  PTRACE_WRITETEXT,		/* 19, write text segment */
  PTRACE_GETFPAREGS,		/* 20, get all fpa regs */
  PTRACE_SETFPAREGS,		/* 21, set all fpa regs */
};

#if 0
#define TARGET_HAS_HARDWARE_WATCHPOINTS

#define TARGET_CAN_USE_HARDWARE_WATCHPOINT(type, cnt, ot) \
  ((type) == bp_hardware_watchpoint)

#define STOPPED_BY_WATCHPOINT(W) \
  ((W).kind == TARGET_WAITKIND_STOPPED \
   && (W).value.sig == TARGET_SIGNAL_TRAP \
   && next_mach_stopped_by_watchpoint (W))

#undef HAVE_STEPPABLE_WATCHPOINT

int next_mach_insert_watchpoint (CORE_ADDR addr, size_t len, int type);
#define target_insert_watchpoint(addr, len, type) \
next_mach_insert_watchpoint (addr, len, type)

int next_mach_remove_watchpoint (CORE_ADDR addr, size_t len, int type);
#define target_remove_watchpoint(addr, len, type) \
next_mach_remove_watchpoint (addr, len, type)
#endif /* 0 */

char **next_process_completer (char *text, char *word);
#define PROCESS_COMPLETER next_process_completer
#define PROCESS_COMPLETER_WORD_BREAK_CHARACTERS gdb_completer_filename_word_break_characters

#define NM_NEXTSTEP

#endif /* _NM_NEXTSTEP_H_ */
