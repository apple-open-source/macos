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

#ifndef _FORMAT_H
#define _FORMAT_H

#include <stdbool.h>

#include "pos.h"	/* Get lex_pos_ty.  */
#include "message.h"	/* Get NFORMATS.  */


#ifdef __cplusplus
extern "C" {
#endif


/* This structure describes a format string parser for a language.  */
struct formatstring_parser
{
  /* Parse the given string as a format string.
     Return a freshly allocated structure describing
       1. the argument types/names needed for the format string,
       2. the total number of format directives.
     Return NULL if the string is not a valid format string. In this case,
     also set *invalid_reason to an error message explaining why.  */
  void * (*parse) (const char *string, char **invalid_reason);

  /* Free a format string descriptor, returned by parse().  */
  void (*free) (void *descr);

  /* Return the number of format directives.
     A string that can be output literally has 0 format directives.  */
  int (*get_number_of_directives) (void *descr);

  /* Verify that the argument types/names in msgid_descr and those in
     msgstr_descr are the same (if equality=true), or (if equality=false)
     that those of msgid_descr extend those of msgstr_descr (i.e.
     msgstr_descr may omit some of the arguments of msgid_descr).
     If not, signal an error using
       error_with_progname = false;
       error_at_line (0, 0, pos->file_name, pos->line_number, ...);
       error_with_progname = true;
     (but only if noisy=true) and return true.  Otherwise return false.  */
  bool (*check) (const lex_pos_ty *pos, void *msgid_descr, void *msgstr_descr, bool equality, bool noisy, const char *pretty_msgstr);
};

/* Format string parsers, each defined in its own file.  */
extern DLL_VARIABLE struct formatstring_parser formatstring_c;
extern DLL_VARIABLE struct formatstring_parser formatstring_objc;
extern DLL_VARIABLE struct formatstring_parser formatstring_sh;
extern DLL_VARIABLE struct formatstring_parser formatstring_python;
extern DLL_VARIABLE struct formatstring_parser formatstring_lisp;
extern DLL_VARIABLE struct formatstring_parser formatstring_elisp;
extern DLL_VARIABLE struct formatstring_parser formatstring_librep;
extern DLL_VARIABLE struct formatstring_parser formatstring_smalltalk;
extern DLL_VARIABLE struct formatstring_parser formatstring_java;
extern DLL_VARIABLE struct formatstring_parser formatstring_awk;
extern DLL_VARIABLE struct formatstring_parser formatstring_pascal;
extern DLL_VARIABLE struct formatstring_parser formatstring_ycp;
extern DLL_VARIABLE struct formatstring_parser formatstring_tcl;
extern DLL_VARIABLE struct formatstring_parser formatstring_perl;
extern DLL_VARIABLE struct formatstring_parser formatstring_perl_brace;
extern DLL_VARIABLE struct formatstring_parser formatstring_php;
extern DLL_VARIABLE struct formatstring_parser formatstring_gcc_internal;
extern DLL_VARIABLE struct formatstring_parser formatstring_qt;

/* Table of all format string parsers.  */
extern DLL_VARIABLE struct formatstring_parser *formatstring_parsers[NFORMATS];

/* Returns an array of the ISO C 99 <inttypes.h> format directives
   contained in the argument string.  *intervalsp is assigned to a freshly
   allocated array of intervals (startpos pointing to '<', endpos to '>'),
   and *lengthp is assigned to the number of intervals in this array.  */
struct interval
{
  size_t startpos;
  size_t endpos;
};
extern void get_c99_format_directives (const char *string, struct interval **intervalsp, size_t *lengthp);


#ifdef __cplusplus
}
#endif


#endif /* _FORMAT_H */
