#ifndef _FIXES_H_
#define _FIXES_H_
/* fixes.h (was write.h in the original GAS)
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

/*
 * For machines with machine dependent relocation types (encoded in the r_type
 * field of a relocation_info struct) they use NO_RELOC in assembling
 * instructions which they want to indicate have no relocation.
 */
#define NO_RELOC        0x10 /* above the range of r_type:4 */

/*
 * FixSs may be built up in any order.
 */
struct fix {
    fragS	*fx_frag;	/* which frag? */
    long int	 fx_where;	/* where is the 1st byte to fix up? */
    symbolS	*fx_addsy;	/* NULL or Symbol whose value we add in */
    symbolS	*fx_subsy;	/* NULL or Symbol whose value we subtract */
    long int	 fx_offset;	/* absolute number we add in */
    struct fix	*fx_next;	/* NULL or -> next fixS */
    char	 fx_size;	/* how many bytes are involved? */
    char	 fx_pcrel;	/* TRUE: pc-relative. */
    char	 fx_pcrel_reloc;/* force a pc-relative relocatation entry */
    char	 fx_r_type;	/* relocation type */
    long	 fx_value;	/* the relocated value placed in the frag */
    char	*file;		/* the file name this came from for errors */
    unsigned int line;		/* the line number this came from for errors */
};
typedef struct fix fixS;

extern void fix_new(
	fragS	*frag,		/* which frag? */
	int	where,		/* where in that frag? */
	int	size,		/* 1, 2 or 4 bytes */
	symbolS *add_symbol,	/* X_add_symbol */
	symbolS *sub_symbol,	/* X_subtract_symbol */
	signed_target_addr_t
	offset,		/* X_add_number */
	int	pcrel,		/* TRUE if PC-relative */
	int	pcrel_reloc,	/* TRUE if must have relocation entry */
	int	r_type);	/* relocation type */
#endif /* _FIXES_H_ */
