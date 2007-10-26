/* sections.c (was subsegs.c in original GAS version)
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
 * Mach-O sections are chains of fragments.
 */
#include <stdlib.h>
#include <strings.h>
#include "sections.h"
#include "obstack.h"
#include "xmalloc.h"
#include "frags.h"
#include "messages.h"

/*
 * All sections' chains hang off here.  NULL means no frchains yet.
 */
frchainS *frchain_root = NULL;

/*
 * The frchain we are assembling into now.  That is, the current section's
 * frag chain, even if it contains no (complete) frags.
 */
frchainS *frchain_now = NULL;

/*
 * sections_begin() sets up to allow sections to be created.
 */
void
sections_begin(
void)
{
	/* This may be needed if a section_new() for .text is not assumed */
	/* obstack_begin(&frags, 5000); */
	/* frag_now = (fragS *)obstack_alloc(&frags, SIZEOF_STRUCT_FRAG); */
}

/*
 * section_new() (for non-zerofill sections) switches to a new section, creating
 * it if needed, and creates a fresh fragment.  If it is the current section
 * nothing happens except checks to make sure the type, attributes and
 * sizeof_stub are the same.  The segment and section names will be trimed to
 * fit in the section structure and is the responsiblity of the caller to
 * report errors if they don't.  For zerofill sections only the struct frchain
 * for the section is returned after possibly being created (these section are
 * never made the current section and no frags are ever touched).
 *
 * Globals on input:
 *    frchain_now points to the (possibly none) struct frchain for the current
 *    section.
 *    frag_now points at an incomplete frag for current section.
 *    If frag_now == NULL, then there is no old, incomplete frag, so the old
 *    frag is not closed off.
 *
 * Globals on output:
 *    frchain_now points to the (possibly new) struct frchain for this section.
 *    frchain_root updated if needed (for the first section created).
 *    frag_now is set to the last (possibly new) frag in the section.
 */
frchainS *
section_new(
char *segname,
char *sectname,
unsigned long type,
unsigned long attributes,
unsigned long sizeof_stub)
{
    frchainS *frcP;
    frchainS **lastPP;
    unsigned long last_nsect;

	if(frags.chunk_size == 0)
	    /*
	     * This line is use instead of:
	     * 	 obstack_begin(&frags, 5000);
	     * which the only difference is that frags are allocated on 4 byte
	     * boundaries instead of the default.  The problem with the default
	     * is that on some RISC machines the obstack uses 8 (the alignment
	     * of a double after a char in a struct) and then the common use of:
	     * 	frag_now->fr_fix = obstack_next_free(&frags) -
	     *			   frag_now->fr_literal;
	     * can get an extra 4 bytes that are not in the frag because of the
	     * 8 byte alignment where only 4 byte alignment for frags are
	     * needed.
	     */
	    _obstack_begin(&frags, 5000, 4,
			   obstack_chunk_alloc, obstack_chunk_free);

	/*
	 * Determine if this section has been seen.
	 */
	last_nsect = 0;
	for(frcP = *(lastPP = &frchain_root);
	    frcP != NULL;
	    frcP = *(lastPP = &frcP->frch_next)){
	    if(strncmp(frcP->frch_section.segname, segname,
		       sizeof(frcP->frch_section.segname)) == 0 &&
	       strncmp(frcP->frch_section.sectname, sectname,
		       sizeof(frcP->frch_section.sectname)) == 0)
		break;
	    last_nsect = frcP->frch_nsect;
	}

	/*
	 * If this section has been seen make sure it's type and attributes
	 * for this call are the same as when the section was created.
	 */
	if(frcP != NULL){
	    if((frcP->frch_section.flags & SECTION_TYPE) != type){
		as_warn("section type does not match previous section type");
	    }
	    if(type == S_SYMBOL_STUBS &&
	       frcP->frch_section.reserved2 != sizeof_stub){
		as_warn("section stub size does not match previous section "
			"stub size");
	    }
	    frcP->frch_section.flags |= attributes;
	}

	/*
	 * If the current section is the same as for this call there is nothing
	 * more to do.
	 */
	if(frcP != NULL && (frchain_now == frcP || type == S_ZEROFILL)){
	    return(frcP);
	}

	/*
	 * For non-zerofill sections it will be made the current section so deal
	 * with the current frag.
	 */
	if(type != S_ZEROFILL){
	    /*
	     * If there is any current frag in the old section close it off.
	     */
	    if(frag_now != NULL){
		frag_now->fr_fix = obstack_next_free(&frags) -
				   frag_now->fr_literal;
		frag_wane(frag_now);
	    }

	    /*
	     * We must do the obstack_finish(), so the next object we put on
	     * obstack frags will not appear to start at the fr_literal of the
	     * current frag.  Also, it ensures that the next object will begin
	     * on a address that is aligned correctly for the engine that runs
	     * the assembler.
	     */
	    obstack_finish(&frags);
	}

	/*
	 * If this section exists since it is not the current section switch to
	 * it by making it the current chain and create a new frag in it.
	 */
	if(frcP != NULL){
	    /*
	     * For a zerofill section no frags are created here and since it
	     * exist just return a pointer to the section.
	     */
	    if((frcP->frch_section.flags & SECTION_TYPE) == S_ZEROFILL){
		return(frcP);
	    }
	    else{
		/*
		 * Make this section the current section.
		 */
		frchain_now = frcP;

		/*
		 * Make a fresh frag for the section.
		 */
		frag_now = (fragS *)obstack_alloc(&frags, SIZEOF_STRUCT_FRAG);
		memset(frag_now, '\0', SIZEOF_STRUCT_FRAG);
		frag_now->fr_next = NULL;

		/*
		 * Append the new frag to the existing frchain.
		 */
		frchain_now->frch_last->fr_next = frag_now;
		frchain_now->frch_last = frag_now;
	    }
	}
	else{
	    /*
	     * This section does not exist so create a new frchainS struct fill
	     * it in, link it to the chain
	     */
	    frcP = (frchainS *)xmalloc(sizeof(frchainS));
	    memset(frcP, '\0', sizeof(frchainS));
	    strncpy(frcP->frch_section.segname, segname,
		    sizeof(frcP->frch_section.segname));
	    strncpy(frcP->frch_section.sectname, sectname,
		    sizeof(frcP->frch_section.sectname));
	    frcP->frch_section.flags = attributes | type;
	    frcP->frch_section.reserved2 = sizeof_stub;

	    frcP->frch_nsect = last_nsect + 1;

	    *lastPP = frcP;

	    /*
	     * For zerofill sections no frag is created here so just return.
	     * For non-zerofill section create the sections new frag and
	     * make the section the current chain.
	     */
	    if(type == S_ZEROFILL){
		return(frcP);
	    }
	    else{
		/*
		 * Make a fresh frag for the new section.
		 */
		frag_now = (fragS *)obstack_alloc(&frags, SIZEOF_STRUCT_FRAG);
		memset(frag_now, '\0', SIZEOF_STRUCT_FRAG);
		frag_now->fr_next = NULL;

		/*
		 * Append the new frag to new frchain.
		 */
		frcP->frch_root = frag_now;
		frcP->frch_last = frag_now;

		/*
		 * Make this section the current section.
		 */
		frchain_now = frcP;
	    }
	}
	return(frchain_now);
}

unsigned long
is_section_coalesced(
unsigned long n_sect)
{
    struct frchain *frchainP;

	for(frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next){
	    if(frchainP->frch_nsect == n_sect &&
	       (frchainP->frch_section.flags & SECTION_TYPE) == S_COALESCED)
		return(1); /* TRUE */
	}
	return(0); /* FALSE */
}

unsigned long
is_section_non_lazy_symbol_pointers(
unsigned long n_sect)
{
    struct frchain *frchainP;

	for(frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next){
	    if(frchainP->frch_nsect == n_sect &&
	       (frchainP->frch_section.flags & SECTION_TYPE) ==
	        S_NON_LAZY_SYMBOL_POINTERS)
		return(1); /* TRUE */
	}
	return(0); /* FALSE */
}

unsigned long
is_section_debug(
unsigned long n_sect)
{
    struct frchain *frchainP;

	for(frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next){
	    if(frchainP->frch_nsect == n_sect &&
	       (frchainP->frch_section.flags & SECTION_ATTRIBUTES) ==
	        S_ATTR_DEBUG)
		return(1); /* TRUE */
	}
	return(0); /* FALSE */
}

unsigned long
is_section_cstring_literals(
unsigned long n_sect)
{
    struct frchain *frchainP;

	for(frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next){
	    if(frchainP->frch_nsect == n_sect &&
	       (frchainP->frch_section.flags & SECTION_TYPE) ==
		S_CSTRING_LITERALS)
		return(1); /* TRUE */
	}
	return(0); /* FALSE */
}

unsigned long
is_end_section_address(
unsigned long n_sect,
unsigned long addr)
{
    struct frchain *frchainP;

	for(frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next){
	    if(frchainP->frch_nsect == n_sect){
		if((frchainP->frch_section.flags & SECTION_TYPE) == S_ZEROFILL)
		    return(0); /* FALSE */
		if(frchainP->frch_last->fr_address == addr)
		    return(1); /* TRUE */
		else
		    return(0); /* FALSE */
	    }
	}
	return(0); /* FALSE */
}

unsigned long
section_has_fixed_size_data(
unsigned long n_sect)
{
    struct frchain *frchainP;

	for(frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next){
	    if(frchainP->frch_nsect == n_sect){
			switch (frchainP->frch_section.flags & SECTION_TYPE){
				case S_CSTRING_LITERALS:
				case S_4BYTE_LITERALS:
				case S_8BYTE_LITERALS:
				case S_16BYTE_LITERALS:
				case S_LITERAL_POINTERS:
				case S_NON_LAZY_SYMBOL_POINTERS:
				case S_LAZY_SYMBOL_POINTERS:
				case S_MOD_INIT_FUNC_POINTERS:
				case S_MOD_TERM_FUNC_POINTERS:
				case S_INTERPOSING:
					return(1); /* TRUE */
			}
		}
	}
	return(0); /* FALSE */
}
