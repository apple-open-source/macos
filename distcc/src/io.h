/* -*- c-file-style: "java"; indent-tabs-mode: nil -*-
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

int dcc_writex(int fd, const void *buf, size_t len);

int dcc_r_token(int ifd, char *token);

int dcc_readx(int fd, void *buf, size_t len);
int dcc_read_int(int fd, /*@out@*/ unsigned * v);
int dcc_pump_readwrite(int ofd, int ifd, size_t n);
int dcc_pump_sendfile(int ofd, int ifd, size_t n);
int dcc_r_str_alloc(int fd, int len, char **buf);

int tcp_cork_sock(int fd, int corked);
int dcc_close(int fd);
