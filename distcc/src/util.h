/* -*- c-file-style: "java"; indent-tabs-mode: nil -*-
 * 
 * distcc -- A simple distributed compiler system
 * $Id: util.h,v 1.1.1.1 2005/05/06 05:09:42 deatley Exp $ 
 *
 * Copyright (C) 2002 by Martin Pool <mbp@samba.org>
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

/* util.c */
int argv_contains(char **argv, const char *s);
int dcc_redirect_fd(int, const char *fname, int);
int str_startswith(const char *head, const char *worm);
char *dcc_gethostname(void);
int dcc_exit(int exitcode) NORETURN;
int dcc_getenv_bool(const char *name, int def_value);
int set_cloexec_flag (int desc, int value);
int dcc_ignore_sigpipe(int val);
int dcc_remove_if_exists(const char *fname);
char *dcc_abspath(const char *path, int path_len);

#define str_equal(a, b) (!strcmp((a), (b)))



