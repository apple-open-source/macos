/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2002, 2003 by Martin Pool <mbp@samba.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/* exec.c */
int dcc_redirect_fds(const char *stdin_file,
                     const char *stdout_file,
                     const char *stderr_file);
void dcc_execvp(char **argv) NORETURN;

int dcc_spawn_child(char **argv, pid_t *pidptr,
                    const char *, const char *, const char *);

int dcc_collect_child(pid_t pid, int *wait_status, long *, long *);
int dcc_critique_status(int s, const char *, const char *hostname);
void dcc_note_execution(const char *hostname, char **argv);
int dcc_report_rusage(const char *command,
                      long utime_usec,
                      long stime_usec);

void dcc_setpgid(pid_t pid, pid_t pgid);
void dcc_reset_signal(int whichsig);

#ifndef W_EXITCODE
#  define W_EXITCODE(exit, signal) ((exit)<<8 | (signal))
#endif 

int dcc_recursion_safeguard(void);
