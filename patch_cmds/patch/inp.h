/* inputting files to be patched */

/* $Id: inp.h,v 1.1.1.3 2003/05/08 18:38:02 rbraun Exp $ */

/* Copyright (C) 1986, 1988 Larry Wall
   Copyright (C) 1991, 1992, 1993, 1997, 1998, 1999, 2002 Free
   Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.
   If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

XTERN LINENUM input_lines;		/* how long is input file in lines */

char const *ifetch (LINENUM, int, size_t *);
void get_input_file (char const *, char const *);
void re_input (void);
void scan_input (char *);
