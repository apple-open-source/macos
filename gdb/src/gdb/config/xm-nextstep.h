/* Host support for NextStep/Rhapsody for GDB, the GNU debugger.
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

#ifndef _XM_NEXTSTEP_H_
#define _XM_NEXTSTEP_H_

/* Always include mach.h first so that __MACH30__ gets defined otherwise
   it doesn't always get defined and some files get compiled as if they
   were 2.5 based and all hell breaks loose.
   Including mach.h usually pulls in everything else (except cthreads),
   but on 2.5 it doesn't get the other ones explicitly included here which
   are needed by a few of the nextstep-nat-*.c files. */

#include <mach/mach.h>

#include <mach/mach_error.h>
#include <mach/notify.h>
#include <mach/exception.h>
#include <mach/mig_errors.h>

#if defined (__MACH30__)

typedef mach_port_t port_t;

typedef mach_msg_header_t msg_header_t;
typedef mach_port_t port_set_name_t;
typedef mach_port_type_t port_type_t;

#define PORT_NULL MACH_PORT_NULL

#define RCV_TIMED_OUT MACH_RCV_TIMED_OUT
#define RCV_TIMEOUT MACH_RCV_TIMED_OUT
#define RCV_LARGE MACH_RCV_TOO_LARGE
#define RCV_TOO_LARGE MACH_RCV_TOO_LARGE
#define RCV_INVALID_PORT MACH_RCV_INVALID_NAME

#define SEND_INVALID_PORT MACH_SEND_INVALID_RIGHT /* check this one */
#define SEND_TIMED_OUT MACH_SEND_TIMED_OUT

#define port_allocate mach_port_allocate
#define port_set_allocate mach_port_allocate
#define port_deallocate mach_port_deallocate
#define port_set_deallocate mach_port_deallocate
#define port_set_add(task, port_set, port) mach_port_move_member(task, port, port_set)
#define port_type mach_port_type
#define task_self mach_task_self
#define task_by_unix_pid task_for_pid

#endif /* __MACH30__ */

#if (!defined (_NSIG) && defined (NSIG))
#define _NSIG NSIG
#endif

#if (NS_TARGET_MAJOR < 5)
#undef HAVE_TERMIOS_H
#endif

#include <limits.h>

#define ADDITIONAL_OPTIONS \
  {"connect",     1, 0, 18},

#define ADDITIONAL_OPTION_CASES \
  case 18: \
    connect_to (optarg); \
    break;

extern void next_resize_window_handler (int signal);
extern void next_resize_window (int *width, int *height);

#define	SIGWINCH_HANDLER next_resize_window_handler

#define	SIGWINCH_HANDLER_BODY \
void next_resize_window_handler (int signal) \
{ \
  next_resize_window (&lines_per_page, &chars_per_line); \
}

char *strchr (const char *s, int c);
char *strpbrk (const char *s1, const char *s2);

extern void xfree (void *v);
extern void xmfree (void *md, void *v);

#define free xfree
#define mfree xmfree

#endif /* _XM_NEXTSTEP_H_ */
