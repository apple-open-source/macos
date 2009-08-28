/* APPLE LOCAL file Darwin */
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

struct target_waitstatus;
struct target_ops;

extern int child_wait (int, struct target_waitstatus *, void *);
#define CHILD_WAIT

#define FETCH_INFERIOR_REGISTERS

#define DISABLE_UNSETTABLE_BREAK(addr) 1

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

#ifndef MACOSX_ACTUAL_HARDWARE_WATCHPOINTS_ARE_SUPPORTED

#define TARGET_HAS_HARDWARE_WATCHPOINTS

int macosx_can_use_hw_watchpoint (int type, int cnt, int ot);
#define TARGET_CAN_USE_HARDWARE_WATCHPOINT(type, cnt, ot) \
  macosx_can_use_hw_watchpoint(type, cnt, ot)

int macosx_region_ok_for_hw_watchpoint (CORE_ADDR start, LONGEST len);
#define TARGET_REGION_OK_FOR_HW_WATCHPOINT(start, len) \
  macosx_region_ok_for_hw_watchpoint (start, len)

int macosx_stopped_by_watchpoint (struct target_waitstatus *w, int, int);
#define STOPPED_BY_WATCHPOINT(w) \
  macosx_stopped_by_watchpoint (&w, stop_signal, stepped_after_stopped_by_watchpoint)

#undef HAVE_STEPPABLE_WATCHPOINT
#define HAVE_NONSTEPPABLE_WATCHPOINT 1
#undef HAVE_CONTINUABLE_WATCHPOINT

void macosx_enable_page_protection_events (int pid);
#define TARGET_ENABLE_HW_WATCHPOINTS(pid) \
  macosx_enable_page_protection_events (pid)

void macosx_disable_page_protection_events (int pid);
#define TARGET_DISABLE_HW_WATCHPOINTS(pid) \
  macosx_disable_page_protection_events (pid)

int macosx_insert_watchpoint (CORE_ADDR addr, size_t len, int type);
#define target_insert_watchpoint(addr, len, type) \
  macosx_insert_watchpoint (addr, len, type)

int macosx_remove_watchpoint (CORE_ADDR addr, size_t len, int type);
#define target_remove_watchpoint(addr, len, type) \
  macosx_remove_watchpoint (addr, len, type)

#endif /* MACOSX_ACTUAL_HARDWARE_WATCHPOINTS_ARE_SUPPORTED */


char **macosx_process_completer (char *text, char *word);
#define PROCESS_COMPLETER \
  macosx_process_completer

#define PROCESS_COMPLETER_WORD_BREAK_CHARACTERS \
  gdb_completer_filename_word_break_characters

#define NM_NEXTSTEP

#endif /* _NM_NEXTSTEP_H_ */
