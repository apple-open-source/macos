/* Remote target communications for serial-line targets in custom GDB protocol
   Copyright 1999 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#ifndef REMOTE_H
#define REMOTE_H

#include <sys/time.h>

/* FIXME?: move this interface down to tgt vector) */

/* Read a packet from the remote machine, with error checking, and
   store it in BUF.  BUF is expected to be of size PBUFSIZ.  If
   FOREVER, wait forever rather than timing out; this is used while
   the target is executing user code.  */

extern void getpkt (char *buf, long sizeof_buf, int forever);

/* Send a packet to the remote machine, with error checking.  The data
   of the packet is in BUF.  The string in BUF can be at most PBUFSIZ
   - 5 to account for the $, # and checksum, and for a possible /0 if
   we are debugging (remote_debug) and want to print the sent packet
   as a string */

extern int putpkt (char *buf);

/* Send HEX encoded string to the target console. (gdb_stdtarg) */

extern void remote_console_output (char *);


/* FIXME: cagney/1999-09-20: The remote cisco stuff in remote.c needs
   to be broken out into a separate file (remote-cisco.[hc]?).  Before
   that can happen, a remote protocol stack framework needs to be
   implemented. */

extern void remote_cisco_objfile_relocate (bfd_signed_vma text_off,
					   bfd_signed_vma data_off,
					   bfd_signed_vma bss_off);

extern void async_remote_interrupt_twice (void *arg);

extern int remote_write_bytes (CORE_ADDR memaddr, const gdb_byte *myaddr, 
			       int len);

extern int remote_read_bytes (CORE_ADDR memaddr, char *myaddr, int len);

extern void (*deprecated_target_resume_hook) (void);
extern void (*deprecated_target_wait_loop_hook) (void);

/* APPLE LOCAL
   When in mi mode, we track the time it takes to complete each command
   and we track how many remote protocol packets were used to complete
   that command.
   The remote protocol can have multiple commands "in flight" at once
   and we don't have access to those nested structures here in remote.c.
   So we use a simple stack mechanism where CURRENT_REMOTE_STATS points
   to the currently active mi command and mi-main.c is responsible for
   updating this as we start/finish mi commands.  */

extern struct remote_stats *current_remote_stats;

struct remote_stats {
  int pkt_sent;
  int pkt_recvd;
  int acks_sent;
  int acks_recvd;
  int assigned_to_global;

  /* The mi token (sequence #) for this command, if available, as a
     string of numeral digits, truncated to 15 characters if longer than
     that.  */
  char mi_token[16];

  /* pktstart is a convenience place for all the packet sending
     routines to remember the start time before the packet goes out.  */
  struct timeval pktstart;

  /* total time spent communicating/waiting for responses from the remote
     stub.  */
  struct timeval totaltime;
};

extern uint64_t total_packets_sent;
extern uint64_t total_packets_received;

#endif
