/* struct_symbol.h - Internal symbol structure
   Copyright (C) 1987 Free Software Foundation, Inc.

This file is part of GAS, the GNU Assembler.

GAS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GAS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GAS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifdef NeXT_MOD
#import <mach-o/nlist.h>
#else /* !defined(NeXT_MOD) */
#ifndef		VMS
#include "a.out.h"		/* Needed to define struct nlist. Sigh. */
#else
#include "a_out.h"
#endif
#endif /* defined(NeXT_MOD) */

struct symbol			/* our version of an nlist node */
{
  struct nlist	sy_nlist;	/* what we write in .o file (if permitted) */
  long unsigned sy_name_offset;	/* 1-origin position of sy_name in symbols */
				/* part of object file. */
				/* 0 for (nameless) .stabd symbols. */
				/* Not used until write_object() time. */
  long int	sy_number;	/* 24 bit symbol number. */
				/* Symbol numbers start at 0 and are */
				/* unsigned. */
  struct symbol *sy_next;	/* forward chain, or NULL */
  struct frag   *sy_frag;	/* NULL or -> frag this symbol attaches to. */
  struct symbol *sy_forward;	/* value is really that of this other symbol */
  void *expression;
};

typedef struct symbol symbolS;

#define sy_name		sy_nlist .n_un. n_name
				/* Name field always points to a string. */
				/* 0 means .stabd-like anonymous symbol. */
#define sy_type 	sy_nlist.	n_type
#ifdef NeXT_MOD
#define sy_other	sy_nlist.	n_sect
#else
#define sy_other	sy_nlist.	n_other
#endif
#define sy_desc		sy_nlist.	n_desc
#define sy_value	sy_nlist.	n_value
				/* Value of symbol is this value + object */
				/* file address of sy_frag. */
typedef unsigned valueT;	/* The type of n_value. Helps casting. */

struct indirect_symbol {
  char			 *isy_name;	/* name of the indirect */
  struct frag   	 *isy_frag;	/* frag this indirect attaches to */
  unsigned long		  isy_offset;	/* offset into frag this attaches to */
  struct symbol		 *isy_symbol;	/* symbol for the indirect */
  struct indirect_symbol *isy_next;	/* forward chain, or NULL */
};
typedef struct indirect_symbol isymbolS;
