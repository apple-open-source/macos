/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78; -*-
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

int dcc_r_file(int ifd, const char *filename, size_t);
int dcc_r_file_body(int ofd, int ifd, size_t len);
int dcc_r_fifo(int ifd, const char *fifo_name, size_t len);
int dcc_x_file(int ofd, const char *fname, const char *token, size_t *);
int dcc_x_file_timed(int ofd, const char *fname, const char *token,
                     size_t *size_out);

int dcc_r_file_timed(int ifd, const char *fname, size_t size);
