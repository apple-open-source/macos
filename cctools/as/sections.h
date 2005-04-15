/* sections.h (was subsegs.h in the original GAS)
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

#import <mach-o/loader.h>
#import "struc-symbol.h"

/*
 * For every section the user mentions in the assembley program, we make one
 * struct frchain.  Each section has exactly one struct frchain and vice versa.
 *
 * Struct frchain's are forward chained (in ascending order of section number).
 * The chain runs through frch_next of each section. 
 *
 * From each struct frchain dangles a chain of struct frags.  The frags
 * represent code fragments, for that section, forward chained.
 */

struct frchain {			/* control building of a frag chain */
    struct frag	    *frch_root;		/* 1st struct frag in chain, or NULL */
    struct frag     *frch_last;		/* last struct frag in chain, or NULL */
    struct frchain  *frch_next;		/* next in chain of struct frchain-s */

    section_t	     frch_section;	/* section info, name, type, etc. */
    unsigned long    frch_nsect;	/* section number (1,2,3,...) */
    struct fix      *frch_fix_root;	/* section fixups */
    isymbolS	    *frch_isym_root;	/* 1st indirect symbol in chain */
    isymbolS	    *frch_isym_last;	/* last indirect symbol in chain */
};

typedef struct frchain frchainS;

/*
 * All sections' chains hang off here.  NULL means no frchains yet.
 */
extern frchainS *frchain_root;

/*
 * The frchain we are assembling into now.  That is, the current section's
 * frag chain, even if it contains no (complete) frags.
 */
extern frchainS *frchain_now;

/*
 * The global routines defined in sections.c
 */
extern void sections_begin(
    void);

extern frchainS *section_new(
    char *segname,
    char *sectname,
    unsigned long type,
    unsigned long attributes,
    unsigned long sizeof_stub);

extern unsigned long is_section_coalesced(
    unsigned long n_sect);

extern unsigned long is_section_non_lazy_symbol_pointers(
    unsigned long n_sect);

extern unsigned long is_end_section_address(
    unsigned long n_sect,
    unsigned long addr);
