/* layout.c (was part of write.c in original GAS version)
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

#include <stdlib.h>
#include <string.h>
#include "stuff/round.h"
#include "as.h"
#include "sections.h"
#include "frags.h"
#include "symbols.h"
#include "fixes.h"
#include "messages.h"
#include "expr.h"
#include "md.h"
#include "obstack.h"

#ifdef SPARC
/* internal relocation types not to be emitted */
#define SPARC_RELOC_13 (127)
#define SPARC_RELOC_22 (126)
#endif

static void fixup_section(
    fixS *fixP,
    int nsect);
static void relax_section(
    struct frag *section_frag_root,
    int nsect);
static relax_addressT relax_align(
    relax_addressT address,
    long alignment);
static int is_down_range(
    struct frag *f1,
    struct frag *f2);

/*
 * layout_addresses() is called after all the assembly code has been read and
 * fragments, symbols and fixups have been created.  This routine sets the
 * address of the fragments and symbols.  Then it does the fixups of the frags
 * and prepares the fixes so relocation entries can be created from them.
 */
void
layout_addresses(
void)
{
    struct frchain *frchainP;
    fragS *fragP;
    relax_addressT slide, tmp;
    symbolS *symbolP;

	if(frchain_root == NULL)
	    return;

	/*
	 * If there is any current frag close it off.
	 */
	if(frag_now != NULL && frag_now->fr_fix == 0){
	    frag_now->fr_fix = obstack_next_free(&frags) -
			       frag_now->fr_literal;
	    frag_wane(frag_now);
	}

	/*
	 * For every section, add a last ".fill 0" frag that will later be used
	 * as the ending address of that section.
	 */
	for(frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next){
	    /*
	     * We must do the obstack_finish(), so the next object we put on
	     * obstack frags will not appear to start at the fr_literal of the
	     * current frag.  Also, it ensures that the next object will begin
	     * on a address that is aligned correctly for the engine that runs
	     * the assembler.
	     */
	    obstack_finish(&frags);

	    /*
	     * Make a fresh frag for the last frag.
	     */
	    frag_now = (fragS *)obstack_alloc(&frags, SIZEOF_STRUCT_FRAG);
	    memset(frag_now, '\0', SIZEOF_STRUCT_FRAG);
	    frag_now->fr_next = NULL;
	    obstack_finish(&frags);

	    /*
	     * Append the new frag to current frchain.
	     */
	    frchainP->frch_last->fr_next = frag_now;
	    frchainP->frch_last = frag_now;
	    frag_wane(frag_now);

	}

	/*
	 * Now set the relitive addresses of frags within the section by
	 * relaxing each section.  That is all sections will start at address
	 * zero and addresses of the frags in that section will increase from
	 * there.
	 */
	for(frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next){
	    if((frchainP->frch_section.flags & SECTION_TYPE) == S_ZEROFILL)
		continue;
	    /*
	     * This is done so in case md_estimate_size_before_relax() (called
	     * by relax_section) wants to make fixSs they are for this
	     * section.
	     */
	    frchain_now = frchainP;

	    relax_section(frchainP->frch_root, frchainP->frch_nsect);
	}

	/*
	 * Now set the absolute addresses of all frags by sliding the frags in
	 * each non-zerofill section by the address ranges taken up by the
	 * sections before it.
	 */ 
	slide = 0;
	for(frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next){
	    if((frchainP->frch_section.flags & SECTION_TYPE) == S_ZEROFILL)
		continue;
	    slide = round(slide, 1 << frchainP->frch_section.align);
	    tmp = frchainP->frch_last->fr_address;
	    if(slide != 0){
		for(fragP = frchainP->frch_root; fragP; fragP = fragP->fr_next){
		    fragP->fr_address += slide;
		}
	    }
	    slide += tmp;
	}
	/*
	 * Now with the non-zerofill section addresses set set all of the
	 * addresses of the zerofill sections.  Comming in the fr_address is
	 * the size of the section and going out it is the start address.  This
	 * will make layout_symbols() work out naturally.  The only funky thing
	 * is that section numbers do not end up in address order.
	 */
	for(frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next){
	    if((frchainP->frch_section.flags & SECTION_TYPE) != S_ZEROFILL)
		continue;
	    slide = round(slide, 1 << frchainP->frch_section.align);

	    tmp = frchainP->frch_root->fr_address;
	    frchainP->frch_root->fr_address = slide;
	    frchainP->frch_last->fr_address = tmp + slide;
	    slide += tmp;
	}

	/*
	 * Set the symbol addresses based on there frag's address.
	 * First forward references are handled.
	 */
	for(symbolP = symbol_rootP; symbolP; symbolP = symbolP->sy_next){
	    if(symbolP->sy_forward != NULL){
		if(symbolP->sy_nlist.n_type & N_STAB)
		    symbolP->sy_other = symbolP->sy_forward->sy_other;
		symbolP->sy_value += symbolP->sy_forward->sy_value +
				     symbolP->sy_forward->sy_frag->fr_address;
		symbolP->sy_forward = 0;
	    }
	}
	for(symbolP = symbol_rootP; symbolP; symbolP = symbolP->sy_next){
	    symbolP->sy_value += symbolP->sy_frag->fr_address;
	}

	/*
	 * At this point the addresses of frags now reflect addresses we use in 
	 * the object file and the symbol values are correct.
	 * Scan the frags, converting any ".org"s and ".align"s to ".fill"s.
	 * Also converting any machine-dependent frags using md_convert_frag();
	 */
	for(frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next){
	    /*
	     * This is done so any fixes created by md_convert_frag() are for
	     * this section.
	     */
	    frchain_now = frchainP;

	    for(fragP = frchainP->frch_root; fragP; fragP = fragP->fr_next){
		switch(fragP->fr_type){
		case rs_align:
		case rs_org:
		    fragP->fr_type = rs_fill;
		    know(fragP->fr_var == 1);
		    know(fragP->fr_next != NULL);
		    fragP->fr_offset = fragP->fr_next->fr_address -
				       fragP->fr_address -
				       fragP->fr_fix;
		    break;

		case rs_fill:
		    break;

		case rs_machine_dependent:
		    md_convert_frag(fragP);
		    /*
		     * After md_convert_frag, we make the frag into a ".fill 0"
		     * md_convert_frag() should set up any fixSs and constants
		     * required.
		     */
		    frag_wane(fragP);
		    break;

		default:
		    BAD_CASE(fragP->fr_type);
		    break;
		}
	    }
	}

	/*
	 * For each section do the fixups for the frags.
	 */
	for(frchainP = frchain_root; frchainP; frchainP = frchainP->frch_next){
	    fixup_section(frchainP->frch_fix_root, frchainP->frch_nsect);
	}
}

/*
 * fixup_section() does the fixups of the frags and prepares the fixes so
 * relocation entries can be created from them.  The fixups cause the contents
 * of the frag to have the value for the fixup expression.  A fix structure that
 * ends up with a non NULL fx_addsy will have a relocation entry created for it.
 */
static
void
fixup_section(
fixS *fixP,
int nsect)
{
    symbolS *add_symbolP;
    symbolS *sub_symbolP;
    long value;
    int size;
    char *place;
    long where;
    char pcrel;
    fragS *fragP;
    int	add_symbol_N_TYPE;
    int	add_symbol_nsect;

	/*
	 * The general fix expression is "fx_addsy - fx_subsy + fx_offset".
	 * The goal is to put the result of this expression into the frag at
	 * "place" for size "size".  The value of the expression is calculated
	 * in the variable "value" and starts with just the fx_offset.
	 */
	for( ; fixP != NULL; fixP = fixP->fx_next){
	    fragP       = fixP->fx_frag;
	    know(fragP);
	    where	= fixP->fx_where;
	    place       = fragP->fr_literal + where;
	    size	= fixP->fx_size;
	    add_symbolP = fixP->fx_addsy;
	    sub_symbolP = fixP->fx_subsy;
	    value  	= fixP->fx_offset;
	    pcrel       = fixP->fx_pcrel;

	    add_symbol_N_TYPE = 0;
	    add_symbol_nsect = 0;

	    if(add_symbolP != NULL){
		add_symbol_N_TYPE = add_symbolP->sy_type & N_TYPE;
		if(add_symbol_N_TYPE == N_SECT)
		    add_symbol_nsect = add_symbolP->sy_other;
	    }

	    /*
	     * Is there a subtract symbol?
	     */
	    if(sub_symbolP){
		/* is it just -sym ? */
		if(add_symbolP == NULL){
		    if(sub_symbolP->sy_type != N_ABS)
			as_warn("Negative of non-absolute symbol %s",
				sub_symbolP->sy_name);
		    value -= sub_symbolP->sy_value;
		    fixP->fx_subsy = NULL;
		}
		/*
		 * There are both an add symbol and a subtract symbol at this
		 * point.
		 *
		 * If both symbols are absolute then just calculate the
		 * value of the fix expression and no relocation entry will be
		 * needed.
		 */
		else if((sub_symbolP->sy_type & N_TYPE) == N_ABS &&
		        (add_symbolP->sy_type & N_TYPE) == N_ABS){
		    value += add_symbolP->sy_value - sub_symbolP->sy_value;
		    add_symbolP = NULL;
		    fixP->fx_addsy = NULL; /* no relocation entry */
		    fixP->fx_subsy = NULL;
		}
		/*
		 * If both symbols are defined in a section then calculate the
		 * value of the fix expression and let a section difference
		 * relocation entry be created.
		 */
		else if((sub_symbolP->sy_type & N_TYPE) == N_SECT &&
		        (add_symbolP->sy_type & N_TYPE) == N_SECT){
		    /*
		     * We are use the new features that are incompatible with
		     * 3.2 then just calculate the value and let this create a
		     * SECTDIFF relocation type.
		     */
#ifdef SPARC
		    /*
		     * Special case dealing with assembler internal relocation
		     * entries SPARC_RELOC_13 and RELOC_22. The can not be
		     * output and must be resolved.
		     */
		    if((fixP->fx_r_type == SPARC_RELOC_13) ||
		       (fixP->fx_r_type == SPARC_RELOC_22)){
			if(sub_symbolP->sy_other == add_symbolP->sy_other){
			    value += add_symbolP->sy_value -
			    sub_symbolP->sy_value;
			    add_symbolP = NULL;
			    fixP->fx_addsy = NULL; /* no relocation entry */
			    fixP->fx_subsy = NULL;
			}
			else{
			    as_warn("Can't emit reloc type %u {-symbol \"%s\"} "
			            "@ file address %ld (mode?).",
				    fixP->fx_r_type, sub_symbolP->sy_name,
				    fragP->fr_address + where);
			}
		    }
		    else
			value += add_symbolP->sy_value - sub_symbolP->sy_value;
#else
		    value += add_symbolP->sy_value - sub_symbolP->sy_value;
#endif
		    goto down;
		}
		/*
		 * If the subtract symbol is absolute subtract it's value from
		 * the fix expression and let a relocation entry get created
		 * that is not a section difference type.
		 */
		else if(sub_symbolP->sy_type == N_ABS){
		    value -= sub_symbolP->sy_value;
		    fixP->fx_subsy = NULL; /* no SECTDIFF relocation entry */
		}
		/*
		 * At this point we have something we can't generate a
		 * relocation entry for (two undefined symbols, etc.).
		 */
	        else{
		     as_warn("Can't emit reloc {- symbol \"%s\"} @ file "
			     "address %ld.", sub_symbolP->sy_name,
			     fragP->fr_address + where);
		}
	    }

	    /*
	     * If a there is an add symbol in the fixup expression then add
	     * the symbol value into the fixup expression's value.
	     */
	    if(add_symbolP){
		/*
		 * If this symbol is in this section and is pc-relative and we
		 * do not want to force a pc-relative relocation entry (to
		 * support scattered loading) then just calculate the value.
		 */
		if(add_symbol_nsect == nsect &&
		   pcrel && !(fixP->fx_pcrel_reloc)){
		    /*
		     * This fixup was made when the symbol's section was
		     * unknown, but it is now in this section. So we know how
		     * to do the address without relocation.
		     */
		    value += add_symbolP->sy_value;
		    value -= size + where + fragP->fr_address;
		    pcrel = 0;	/* Lie. Don't want further pcrel processing. */
		    fixP->fx_addsy = NULL; /* No relocations please. */
		    /*
		     * It would be nice to check that the address does not
		     * overflow.
		     * I didn't do this check because:
		     * +  It is machine dependent in the general case (eg 32032)
		     * +  Compiler output will never need this checking, so why
		     *    slow down the usual case?
		     */
		}
		else{
		    switch(add_symbol_N_TYPE){
		    case N_ABS:
			/*
			 * If the value of the symbol was an expression then
			 * now evaluate the expression now.  This can happen
			 * when symbols like:
			 *	.set x,a-b
			 * are used and the value of x is not known till all
			 * of the symbols are seen and had their values set.
			 */
			if(add_symbolP->expression != NULL){
			    expressionS *exp;

			    exp = (expressionS *)add_symbolP->expression;
			    value +=
				exp->X_add_symbol->sy_value -
				exp->X_subtract_symbol->sy_value;
			}
			else
			    value += add_symbolP->sy_value;
			fixP->fx_addsy = NULL; /* no relocation entry */
			add_symbolP = NULL;
			break;
			
		    case N_SECT:
			if((add_symbolP->sy_type & N_EXT) != N_EXT ||
			   add_symbol_N_TYPE != N_SECT ||
			   !is_section_coalesced(add_symbol_nsect))
			    value += add_symbolP->sy_value;
			break;
			
		    case N_UNDF:
			break;
			
		    default:
			BAD_CASE(add_symbol_N_TYPE);
			break;
		    }
		}
	    }
down:
	    /*
	     * If the fixup expression is pc-relative then the value of the pc
	     * will be added to the expression when the machine executes the
	     * the instruction so we adjust the fixup expression's value by
	     * subtracting off the pc value (where) and adjust for insn size.
	     */
	    if(pcrel){
		value -= size + where + fragP->fr_address;
		if(add_symbolP == NULL){
		    fixP->fx_addsy = &abs_symbol; /* force relocation entry */
		}
	    }

	    if((size == 1 && (value & 0xffffff00) &&
			    ((value & 0xffffff80) != 0xffffff80)) ||
	       (size == 2 && (value & 0xffff8000) &&
			    ((value & 0xffff8000) != 0xffff8000)))
		as_warn("Fixup of %ld too large for field width of %d",
			value, size);

	    /*
	     * Now place the fix expression's value in the place for the size.
	     * And save the fix expression's value to be used when creating
	     * a relocation entry if required.
	     */
	    md_number_to_imm((unsigned char *)place, value, size, fixP, nsect);
	    fixP->fx_value = value;
	}
}


/*
 * relax_section() here we set the fr_address values in the frags.
 * After this, all frags in this segment have addresses that are correct
 * relative to the section (that is the section starts at address zero).
 * After all of the sections have been processed by this call and their sizes
 * are know then they can be slid to their final address.
 */
static
void
relax_section(
struct frag *frag_root,
int nsect)
{
    struct frag *fragP;
    relax_addressT address;

    long stretch; /* May be any size, 0 or negative. */
		  /* Cumulative number of addresses we have */
		  /* relaxed this pass. */
		  /* We may have relaxed more than one address. */
    long stretched;  /* Have we stretched on this pass? */
		    /* This is 'cuz stretch may be zero, when,
		       in fact some piece of code grew, and
		       another shrank.  If a branch instruction
		       doesn't fit anymore, we need another pass */

    const relax_typeS *this_type;
    const relax_typeS *start_type;
    relax_substateT next_state;
    relax_substateT this_state;

    long growth;
    long was_address;
    long offset;
    symbolS *symbolP;
    long target;
    long after;
    long aim;

	growth = 0;

	/*
	 * For each frag in segment count and store (a 1st guess of) fr_address.
	 */
	address = 0;
	for(fragP = frag_root; fragP != NULL; fragP = fragP->fr_next){
	    fragP->fr_address = address;
	    address += fragP->fr_fix;
	    switch(fragP->fr_type){
	    case rs_fill:
		address += fragP->fr_offset * fragP->fr_var;
		break;

	    case rs_align:
		address += relax_align(address, fragP->fr_offset);
		break;

	    case rs_org:
		/*
		 * Assume .org is nugatory. It will grow with 1st relax.
		 */
		break;

	    case rs_machine_dependent:
		address += md_estimate_size_before_relax(fragP, nsect);
		break;

	    default:
		BAD_CASE(fragP->fr_type);
		break;
	    }
	}

	/*
	 * Do relax().
	 * Make repeated passes over the chain of frags allowing each frag to
	 * grow if needed.  On each pass each frag's address is incremented by
	 * the accumulated growth, kept in stretched.  Passes are continued 
	 * until there is no stretch on the previous pass.
	 */
	do{
	    stretch = 0;
	    stretched = 0;
	    for(fragP = frag_root; fragP != NULL; fragP = fragP->fr_next){
		was_address = fragP->fr_address;
		fragP->fr_address += stretch;
		address = fragP->fr_address;
		symbolP = fragP->fr_symbol;
		offset = fragP->fr_offset;
		switch(fragP->fr_type){
		case rs_fill:	/* .fill never relaxes. */
		    growth = 0;
		    break;

		case rs_align:
		    growth = relax_align((relax_addressT)
					 (address + fragP->fr_fix), offset) -
			     relax_align((relax_addressT)
					 (was_address + fragP->fr_fix), offset);
		    break;

		case rs_org:
		    target = offset;
		    if(symbolP != NULL){
			know(((symbolP->sy_type & N_TYPE) == N_ABS) ||
			     ((symbolP->sy_type & N_TYPE) == N_SECT));
			know(symbolP->sy_frag);
			know((symbolP->sy_type & N_TYPE) != N_ABS ||
			     symbolP->sy_frag == &zero_address_frag );
			target += symbolP->sy_value +
				  symbolP->sy_frag->fr_address;
		    }
		    know(fragP->fr_next);
		    after = fragP->fr_next->fr_address;
		    /*
		     * Growth may be negative, but variable part of frag cannot
		     * have < 0 chars. That is, we can't .org backwards.
		     */
		    growth = ((target - after ) > 0) ? (target - after) : 0;

		    growth -= stretch;	/* This is an absolute growth factor */
		    break;

		case rs_machine_dependent:
		    this_state = fragP->fr_subtype;
		    this_type = md_relax_table + this_state;
		    start_type = this_type;

		    target = offset;
		    if(symbolP){
			know(((symbolP->sy_type & N_TYPE) == N_ABS) ||
			     ((symbolP->sy_type & N_TYPE) == N_SECT));
			know(symbolP->sy_frag);
			know((symbolP->sy_type & N_TYPE) != N_ABS ||
			     symbolP->sy_frag == &zero_address_frag);

			target += symbolP->sy_value +
				  symbolP->sy_frag->fr_address;
			/*
			 * If frag has yet to be reached on this pass,
			 * assume it will move by STRETCH just as we did.
			 * If this is not so, it will be because some frag
			 * between grows, and that will force another pass.
			 */
			if(symbolP->sy_frag->fr_address >= was_address &&
			   is_down_range(fragP, symbolP->sy_frag))
			    target += stretch;
		    }
		    aim = target - address - fragP->fr_fix;
		    if(aim < 0){
			/* Look backwards. */
			for(next_state = this_type->rlx_more; next_state; ){
			    if(aim >= this_type->rlx_backward)
				next_state = 0;
			    else{	/* Grow to next state. */
				this_state = next_state;
				this_type = md_relax_table + this_state;
				next_state = this_type->rlx_more;
			    }
			}
		    }
		    else{
			/* Look forwards. */
			for(next_state = this_type->rlx_more; next_state; ){
			    if(aim <= this_type->rlx_forward)
				next_state = 0;
			    else{	/* Grow to next state. */
				this_state = next_state;
				this_type = md_relax_table + this_state;
				next_state = this_type->rlx_more;
			    }
			}
		    }
		    if((growth = this_type->rlx_length -start_type->rlx_length))
			  fragP->fr_subtype = this_state;
		    break;

		  default:
		      BAD_CASE(fragP->fr_type);
		      break;
		}
		if(growth) {
		    stretch += growth;
		    stretched++;
		}
	    }			/* For each frag in the segment. */
	}while(stretched);	/* Until nothing further to relax. */

	/*
	 * We now have valid fr_address'es for each frag.  All fr_address's
	 * are correct, relative to their own section.  We have made all the
	 * fixS for this section that will be made.
	 */
}

/*
 * Relax_align. Advance location counter to next address that has 'alignment'
 * lowest order bits all 0s.
 */
static
relax_addressT		/* How many addresses does the .align take? */
relax_align(
relax_addressT address, /* Address now. */
long alignment)		/* Alignment (binary). */
{
    relax_addressT mask;
    relax_addressT new_address;

	mask = ~ ( (~0) << alignment );
	new_address = (address + mask) & (~ mask);
	return(new_address - address);
}

/*
 * is_down_range() is used in relax_section() to determine it one fragment is
 * after another to know if it will also be moved if the first is moved.
 */
static
int
is_down_range(
struct frag *f1,
struct frag *f2)
{
	while(f1){
	    if(f1->fr_next == f2)
		return(1);
	    f1 = f1->fr_next;
	}
	return(0);
}
