/* Format strings.
   Copyright (C) 2001-2003 Free Software Foundation, Inc.
   Written by Bruno Haible <haible@clisp.cons.org>, 2001.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* Specification.  */
#include "format.h"

/* Table of all format string parsers.  */
struct formatstring_parser *formatstring_parsers[NFORMATS] =
{
  /* format_c */		&formatstring_c,
  /* format_objc */		&formatstring_objc,
  /* format_sh */		&formatstring_sh,
  /* format_python */		&formatstring_python,
  /* format_lisp */		&formatstring_lisp,
  /* format_elisp */		&formatstring_elisp,
  /* format_librep */		&formatstring_librep,
  /* format_smalltalk */	&formatstring_smalltalk,
  /* format_java */		&formatstring_java,
  /* format_awk */		&formatstring_awk,
  /* format_pascal */		&formatstring_pascal,
  /* format_ycp */		&formatstring_ycp,
  /* format_tcl */		&formatstring_tcl,
  /* format_perl */		&formatstring_perl,
  /* format_perl_brace */	&formatstring_perl_brace,
  /* format_php */		&formatstring_php,
  /* format_gcc_internal */	&formatstring_gcc_internal,
  /* format_qt */		&formatstring_qt
};
