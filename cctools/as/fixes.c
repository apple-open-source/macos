/* fixes.h (was taken from write.c in the original GAS)
   Copyright (C) 1986,1987 Free Software Foundation, Inc.

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

#include "sections.h"
#include "obstack.h"
#include "frags.h"
#include "fixes.h"
#include "symbols.h"

/*
 * fix_new() creates a fixS in obstack 'notes'.
 */
void
fix_new(
fragS	*frag,		/* which frag? */
int	where,		/* where in that frag? */
int	size,		/* 1, 2 or 4 bytes */
symbolS *add_symbol,	/* X_add_symbol */
symbolS *sub_symbol,	/* X_subtract_symbol */
long    offset,		/* X_add_number */
int	pcrel,		/* TRUE if PC-relative relocation */
int	pcrel_reloc,	/* TRUE if must have relocation entry */
int	r_type)		/* relocation type */
{
    struct fix *fixP;

	fixP = (struct fix *)obstack_alloc(&notes, sizeof(struct fix));

	fixP->fx_frag	     = frag;
	fixP->fx_where       = where;
	fixP->fx_size	     = size;
	fixP->fx_addsy       = add_symbol;
	fixP->fx_subsy       = sub_symbol;
	fixP->fx_offset      = offset;
	fixP->fx_pcrel       = pcrel;
	fixP->fx_pcrel_reloc = pcrel_reloc;
	fixP->fx_r_type      = r_type;

	fixP->fx_next              = frchain_now->frch_fix_root;
	frchain_now->frch_fix_root = fixP;
}
