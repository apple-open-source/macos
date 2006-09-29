/* symbols.c -symbol table-
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

#include <stdlib.h>
#include <string.h>
#include "as.h"
#include "hash.h"
#include "obstack.h"		/* For "symbols.h" */
#include "struc-symbol.h"
#include "symbols.h"
#include "frags.h"
#include "expr.h"
#include "sections.h"
#include "read.h"
#include "xmalloc.h"
#include "messages.h"
#include "fixes.h"
#include "input-scrub.h"

/* symbol-name => struct symbol pointer */
struct hash_control *sy_hash = NULL;

/* FixS & symbols live here */
struct obstack notes = { 0 };

/* all the symbol nodes */
symbolS *symbol_rootP = NULL;
/* last struct symbol we made, or NULL */
symbolS *symbol_lastP = NULL;
/* The last symbol we assigned an index to. */
symbolS *symbol_lastIndexedP = NULL;

symbolS	abs_symbol = { {{0}} };

/*
 * Un*x idea of local labels. They are made by "n:" where n
 * is any decimal digit. Refer to them with
 *  "nb" for previous (backward) n:
 *  or "nf" for next (forward) n:.
 *
 * Like Un*x AS, we have one set of local label counters for entire assembly,
 * not one set per (sub)segment like in most assemblers. This implies that
 * one can refer to a label in another segment, and indeed some crufty
 * compilers have done just that.
 *
 * I document the symbol names here to save duplicating words elsewhere.
 * The mth occurence of label n: is turned into the symbol "Ln^Am" where
 * n is a digit and m is a decimal number. "L" makes it a label discarded
 * unless debugging and "^A"('\1') ensures no ordinary symbol SHOULD get the
 * same name as a local label symbol. The first "4:" is "L4^A1" - the m
 * numbers begin at 1.
 */

typedef short unsigned int local_label_countT;

static local_label_countT local_label_counter[10];

static				/* Returned to caller, then copied. */
  char symbol_name_build[12];	/* used for created names ("4f") */

static long int symbol_count = 0;	/* The number of symbols we've declared. */

static void make_stab_for_symbol(
    symbolS *symbolP);


void
symbol_begin(
void)
{
  symbol_lastP = NULL;
  symbol_rootP = NULL;		/* In case we have 0 symbols (!!) */
  sy_hash = hash_new();
  memset((char *)(&abs_symbol), '\0', sizeof(abs_symbol));
  abs_symbol.sy_type = N_ABS;	/* Can't initialise a union. Sigh. */
  memset((char *)(local_label_counter), '\0', sizeof(local_label_counter) );
}

/*
 *			local_label_name()
 *
 * Caller must copy returned name: we re-use the area for the next name.
 */
char *				/* Return local label name. */
local_label_name(
int n,		/* we just saw "n:", "nf" or "nb" : n a digit */
int augend)	/* 0 for nb, 1 for n:, nf */
{
  register char *	p;
  register char *	q;
  char symbol_name_temporary[10]; /* build up a number, BACKWARDS */

  know( n >= 0 );
  know( augend == 0 || augend == 1 );
  p = symbol_name_build;
  * p ++ = 'L';
  * p ++ = n + '0';		/* Make into ASCII */
  * p ++ = 1;			/* ^A */
  n = local_label_counter [ n ] + augend;
				/* version number of this local label */
  /*
   * Next code just does sprintf( {}, "%d", n);
   * It is more elegant to do the next part recursively, but a procedure
   * call for each digit emitted is considered too costly.
   */
  q = symbol_name_temporary;
  for (*q++=0; n; q++)		/* emits NOTHING if n starts as 0 */
    {
      know(n>0);		/* We expect n > 0 always */
      *q = n % 10 + '0';
      n /= 10;
    }
  while (( * p ++ = * -- q ))
    {
    }
  /* The label, as a '\0' ended string, starts at symbol_name_build. */
  return (symbol_name_build);
}

void
local_colon(
int n)	/* just saw "n:" */
{
  local_label_counter [n] ++;
  colon (local_label_name (n, 0));
}

/*
 *			symbol_new()
 *
 * Return a pointer to a new symbol.
 * Die if we can't make a new symbol.
 * Fill in the symbol's values.
 * Add symbol to end of symbol chain.
 *
 *
 * Please always call this to create a new symbol.
 *
 * Changes since 1985: Symbol names may not contain '\0'. Sigh.
 */
symbolS *
symbol_new(
char	       *name,	/* We copy this: OK to alter your copy. */
unsigned char	type,	/* As in <nlist.h>. */
char		other,	/* As in <nlist.h>. */
short		desc,	/* As in <nlist.h>. */
valueT		value,	/* As in <nlist.h>, often an address. */
			/* Often used as offset from frag address. */
struct frag    *frag)	/* For sy_frag. */
{
  register symbolS *		symbolP;
  register char *		preserved_copy_of_name;
  register unsigned int		name_length;
           char *		p;

  name_length = strlen(name) + 1;
  obstack_grow(&notes,name,name_length);
  p=obstack_finish(&notes);
  /* obstack_1done( &notes, name, name_length, &p ); */
  preserved_copy_of_name = p;
  p=obstack_alloc(&notes,sizeof(struct symbol));
  /* obstack_1blank( &notes, sizeof(struct symbol), &p ); */
  symbolP			= (symbolS *) p;
  symbolP -> sy_name		= preserved_copy_of_name;
  symbolP -> sy_type		= type;
  symbolP -> sy_other		= other;
  symbolP -> sy_desc		= desc;
  symbolP -> sy_value		= value;
  symbolP -> sy_frag		= frag;
  symbolP -> sy_prev_by_index	= NULL; /* Don't know what this is yet. */
  symbolP -> sy_next		= NULL;	/* End of chain. */
  symbolP -> sy_forward		= NULL; /* JF */
  symbolP -> expression		= NULL;
#ifdef SUSPECT
  symbolP -> sy_name_offset	= ~ 0; /* Impossible offset catches errors. */
  symbolP -> sy_number		= ~ 0; /* Ditto. */
#endif
  /*
   * Link to end of symbol chain.
   */
  if (symbol_lastP)
    {
      symbol_lastP -> sy_next = symbolP;
    }
  else
    {
      symbol_rootP = symbolP;
    }
  symbol_lastP = symbolP;

  return (symbolP);
}

/*
 *			symbol_assign_index()
 *
 * Assigns the next index to the given symbol.
 *
 * Asserts that the symbol has not been assigned an index yet.
 *
 */
void
symbol_assign_index(
struct symbol *symbolP)
{
  if (symbolP->sy_prev_by_index != NULL)
    {
      as_fatal("symbol %s already has an index", symbolP->sy_name);
    }
  symbolP->sy_prev_by_index = symbol_lastIndexedP;
  symbol_lastIndexedP = symbolP;
}

/*
 *			colon()
 *
 * We have just seen "<name>:".
 * Creates a struct symbol unless it already exists.
 *
 * Gripes if we are redefining a symbol incompatibly (and ignores it).
 *
 */
/* This is used to work around compiler optimizer bug #50416 */
static volatile unsigned int temp;

void
colon(		/* just seen "x:" - rattle symbols & frags */
char *sym_name) /* symbol name, as a cannonical string */
		/* We copy this string: OK to alter later. */
{
  register struct symbol * symbolP; /* symbol we are working with */

  if (frchain_now == NULL)
    {
      know(flagseen['n']);
      as_fatal("with -n a section directive must be seen before assembly "
	       "can begin");
    }
  if ((symbolP = symbol_table_lookup( sym_name )))
    {
      /*
       *	Now check for undefined symbols
       */
      if ((symbolP -> sy_type & N_TYPE) == N_UNDF)
	{
	  temp = symbolP->sy_desc;
	  if(   symbolP -> sy_other == 0
	     /* bug #50416 -O causes this not to work for:
	     && ((symbolP->sy_desc) & (~REFERENCE_TYPE)) == 0
	     */
	     && (temp & (~(REFERENCE_TYPE | N_WEAK_REF | N_WEAK_DEF |
			   N_NO_DEAD_STRIP | REFERENCED_DYNAMICALLY))) == 0
	     && symbolP -> sy_value == 0)
	    {
	      symbolP -> sy_frag  = frag_now;
	      symbolP -> sy_value = obstack_next_free(& frags) - frag_now -> fr_literal;
	      know( N_UNDF == 0 );
	      symbolP -> sy_type |= N_SECT; /* keep N_EXT bit */
	      symbolP -> sy_other = frchain_now->frch_nsect;
	      symbolP -> sy_desc &= ~REFERENCE_TYPE;
	      symbolP -> sy_desc &= ~N_WEAK_REF;
	      symbol_assign_index(symbolP);
	      if((symbolP->sy_desc & N_WEAK_DEF) == N_WEAK_DEF &&
		 (frchain_now->frch_section.flags & S_COALESCED) != S_COALESCED)
		  as_fatal("symbol: %s can't be a weak_definition (currently "
			   "only supported in section of type coalesced)",
			   sym_name);
#ifdef NeXT_MOD	/* generate stabs for debugging assembly code */
	      if(flagseen['g'])
		  make_stab_for_symbol(symbolP);
#endif
	    }
	  else
	    {
	      as_fatal( "Symbol \"%s\" is already defined as \"%s\"/%d.%d."
			TA_DFMT ".",
		      sym_name,
		      seg_name [(int) N_TYPE_seg [symbolP -> sy_type & N_TYPE]],
		      symbolP -> sy_other, symbolP -> sy_desc,
		      symbolP -> sy_value);
	    }
	}
      else
	{
	  as_fatal("Symbol %s already defined.",sym_name);
	}
    }
  else
    {
      symbolP = symbol_new (sym_name,
			    N_SECT,
	      		    frchain_now->frch_nsect,
			    0,
			    (valueT)(obstack_next_free(&frags)-frag_now->fr_literal),
			    frag_now);
      symbol_table_insert (symbolP);
      symbol_assign_index (symbolP);
#ifdef NeXT_MOD	/* generate stabs for debugging assembly code */
      if(flagseen['g'])
	  make_stab_for_symbol(symbolP);
#endif
    }
}


/*
 *			symbol_table_insert()
 *
 * Die if we can't insert the symbol.
 *
 */
void
symbol_table_insert(
struct symbol *symbolP)
{
  const char *	error_string;

  know( symbolP );
  know( symbolP -> sy_name );
  error_string = hash_jam (sy_hash, symbolP -> sy_name, (char *)symbolP);
  if (error_string != NULL && error_string[0] != '\0')
    {
      as_fatal( "Inserting \"%s\" into symbol table failed: %s",
	      symbolP -> sy_name, error_string);
    }
}

/*
 *			symbol_find_or_make()
 *
 * If a symbol name does not exist, create it as undefined, and insert
 * it into the symbol table. Return a pointer to it.
 */
symbolS *
symbol_find_or_make(
char *name)
{
  register symbolS *	symbolP;

  symbolP = symbol_table_lookup (name);
  if (symbolP == NULL)
    {
      symbolP = symbol_new (name, N_UNDF, 0, 0, 0, & zero_address_frag);
      symbol_table_insert (symbolP);
    }
  return (symbolP);
}

/*
 *			symbol_find()
 * 
 * Implement symbol table lookup.
 * In:	A symbol's name as a string: '\0' can't be part of a symbol name.
 * Out:	NULL if the name was not in the symbol table, else the address
 *	of a struct symbol associated with that name.
 */
symbolS *
symbol_find(
char *name)
{
  return ( (symbolS *) hash_find( sy_hash, name ));
}

/*
 *			symbol_table_lookup()
 * 
 * Same as symbol_find() except assumes the symbol is being looked up and is
 * a non-lazy symbol reference.
 */
symbolS *
symbol_table_lookup(
char *name)
{
  register symbolS *	symbolP;

  symbolP = (symbolS *) hash_find( sy_hash, name );
  if(symbolP != NULL)
    symbolP->sy_desc &= ~REFERENCE_FLAG_UNDEFINED_LAZY;
  return(symbolP);
}

#ifdef NeXT_MOD	/* generate stabs for debugging assembly code */
/*
 * make_stab_for_symbol() is called when -g is present for a label that is
 * being defined.  If the label is a text label and in the (__TEXT,__text)
 * section and not a local label create a stab for it.
 * 
 * See the detailed comments about stabs in read_a_source_file() for a
 * description of what is going on here.
 */
static
void
make_stab_for_symbol(
symbolS *symbolP)
{
    symbolS *stab;
    int stabnamelen;
    char *stabname;

	if(symbolP->sy_name[0] == 'L')
	    return;
	if((symbolP->sy_type & N_TYPE) != N_SECT)
	    return;
	if(symbolP->sy_other != text_nsect)
	    return;

	stabnamelen = strlen(symbolP->sy_name) + sizeof(":f3");
	stabname = xmalloc(stabnamelen);
	strcpy(stabname, symbolP->sy_name);
	if(symbolP->sy_type & N_EXT)
	    strcat(stabname, ":F3");
	else
	    strcat(stabname, ":f3");
	
	stab = symbol_new(
		stabname,
		36, /* N_FUN */
		text_nsect, /* n_sect */
		logical_input_line, /* n_desc, line number */
		symbolP->sy_value,
		symbolP->sy_frag);
	free(stabname);
}
#endif /* NeXT generate stabs for debugging assembly code */

/*
 * indirect_symbol_new()
 *
 * Return a pointer to a new indirect_symbol.
 * Die if we can't make a new indirect_symbol.
 * Fill in the indirect_symbol's values.
 * Add symbol to end of section's indirect symbol chain.
 */
isymbolS *
indirect_symbol_new(
char	       *name,	  /* We copy this: OK to alter your copy. */
struct frag    *frag,	  /* For sy_frag. */
unsigned long	offset)	  /* Offset from frag address. */
{
    isymbolS *isymbolP;
    char *preserved_copy_of_name;
    unsigned long name_length;
    char *p;
    struct frag *fr_next;
    symbolS *symbolP;
#ifdef CHECK_INDIRECTS
    unsigned long stride, fr_fix;
#endif

	/*
	 * First see if the last frag recorded for an indirect symbol turned
	 * out to be zero sized then changed that recorded frag to the next
	 * non-zero frag in the list.  I think this happens because we record
	 * the frag before we fill it and if we run out of space that frag gets
	 * a zero size and a new one is created.
	 */
	if(frchain_now->frch_isym_last != NULL &&
	   frchain_now->frch_isym_last->isy_frag->fr_fix == 0){
	    if(frchain_now->frch_isym_last->isy_frag->fr_next != NULL){
		fr_next = frchain_now->frch_isym_last->isy_frag->fr_next;
		while(fr_next->fr_fix == 0 &&
		      fr_next->fr_type == rs_fill &&
		      fr_next->fr_next != NULL)
			fr_next = fr_next->fr_next;
		frchain_now->frch_isym_last->isy_frag = fr_next;
	    }
	}

	name_length = strlen(name) + 1;
	obstack_grow(&notes, name, name_length);
	p = obstack_finish(&notes);
	preserved_copy_of_name = p;
	p = obstack_alloc(&notes, sizeof(struct indirect_symbol));
	isymbolP = (isymbolS *)p;
	isymbolP->isy_name    = preserved_copy_of_name;
	isymbolP->isy_offset  = offset;
	isymbolP->isy_frag    = frag;
	isymbolP->isy_next    = NULL;	/* End of chain. */
	isymbolP->isy_symbol  = NULL;

	/*
	 * Link to end of indirect symbol chain and check for missing indirect
	 * symbols.
	 */
	if(frchain_now->frch_isym_root == NULL){
#ifdef CHECK_INDIRECTS
	    if(offset != 0)
		as_bad("missing or bad indirect symbol for section (%s,%s)",
			frchain_now->frch_section.segname,
			frchain_now->frch_section.sectname);
#endif
	    frchain_now->frch_isym_root = isymbolP;
	    frchain_now->frch_isym_last = isymbolP;
	}
	else{
#ifdef CHECK_INDIRECTS
	    if((frchain_now->frch_section.flags & SECTION_TYPE) ==
	       S_SYMBOL_STUBS)
		stride = frchain_now->frch_section.reserved2;
	    else
		stride = sizeof(unsigned long);
	    if(frag == frchain_now->frch_isym_last->isy_frag){
		if(offset - frchain_now->frch_isym_last->isy_offset != stride)
		    as_bad("missing or bad indirect symbol for section "
			    "(%s,%s)", frchain_now->frch_section.segname,
			    frchain_now->frch_section.sectname);
	    }
	    else{
		if(frchain_now->frch_isym_last->isy_frag->fr_fix < stride){
		    fr_fix = 0;
		    fr_next = frchain_now->frch_isym_last->isy_frag;
		    while(fr_fix + fr_next->fr_fix < stride &&
			  fr_next->fr_type == rs_fill &&
			  fr_next->fr_next != NULL){
			fr_fix += fr_next->fr_fix;
			fr_next = fr_next->fr_next;
		    }
		    if(frag != fr_next->fr_next ||
		       fr_fix + fr_next->fr_fix != stride ||
		       offset != 0)
			as_bad("missing or bad indirect symbol for section "
				"(%s,%s)", frchain_now->frch_section.segname,
				frchain_now->frch_section.sectname);
		}
		else{
		    fr_next = frchain_now->frch_isym_last->isy_frag->fr_next;
		    /*
		     * Because of section changes there maybe some zero length
		     * frags after the last one that passed through here.  So
		     * skip them and get to the last real one.
		     */
		    while(fr_next->fr_fix == 0 &&
			  fr_next->fr_type == rs_fill &&
			  fr_next->fr_next != NULL)
			fr_next = fr_next->fr_next;
		    if(frag != fr_next || offset != 0)
			as_bad("missing or bad indirect symbol for section "
				"(%s,%s)", frchain_now->frch_section.segname,
				frchain_now->frch_section.sectname);
		}
	    }
#endif
	    frchain_now->frch_isym_last->isy_next = isymbolP;
	    frchain_now->frch_isym_last = isymbolP;
	}
	if((frchain_now->frch_section.flags & SECTION_TYPE) ==
	   S_NON_LAZY_SYMBOL_POINTERS){
	    symbolP = (symbolS *)hash_find(sy_hash, name);
	    if(symbolP != NULL)
		symbolP->sy_desc &= ~REFERENCE_FLAG_UNDEFINED_LAZY;
	}
	return(isymbolP);
}

/* end: symbols.c */
