/* -*- c-file-style: "java"; indent-tabs-mode: nil -*-
 * 
 * distcc -- A simple distributed compiler system
 * $Header: /cvs/repository/devenv/pbxdev/distcc/src/tempfile.h,v 1.3 2003/04/05 00:46:32 rwill Exp $ 
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

/* tempfile.c */
int dcc_mkfifo(const char *fifo_name);
const char *dcc_get_tempdir(void);
int dcc_setup_tempdir(void);
void dcc_cleanup_tempfiles(void);
char *dcc_make_tmpnam(const char *, const char *suffix);

