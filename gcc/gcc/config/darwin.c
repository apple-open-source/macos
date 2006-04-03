/* Functions for generic Darwin as target machine for GNU C compiler.
   Copyright (C) 1989, 1990, 1991, 1992, 1993, 2000, 2001, 2002, 2003, 2004,
   2005
   Free Software Foundation, Inc.
   Contributed by Apple Computer Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "rtl.h"
#include "regs.h"
#include "hard-reg-set.h"
#include "real.h"
#include "insn-config.h"
#include "conditions.h"
#include "insn-flags.h"
#include "output.h"
#include "insn-attr.h"
#include "flags.h"
#include "tree.h"
#include "expr.h"
#include "reload.h"
#include "function.h"
#include "ggc.h"
#include "langhooks.h"
#include "target.h"
#include "tm_p.h"
#include "errors.h"
#include "hashtab.h"
/* APPLE LOCAL begin constant cfstrings */
#include "toplev.h"

static tree darwin_build_constant_cfstring (tree);

enum darwin_builtins
{
  DARWIN_BUILTIN_MIN = (int)END_BUILTINS,

  DARWIN_BUILTIN_CFSTRINGMAKECONSTANTSTRING,
  DARWIN_BUILTIN_MAX
};
/* APPLE LOCAL end constant cfstrings */

/* APPLE LOCAL begin mainline 5 nops */
/* Darwin supports a feature called fix-and-continue, which is used
   for rapid turn around debugging.  When code is compiled with the
   -mfix-and-continue flag, two changes are made to the generated code
   that allow the system to do things that it would normally not be
   able to do easily.  These changes allow gdb to load in
   recompilation of a translation unit that has been changed into a
   running program and replace existing functions and methods of that
   translation unit with with versions of those functions and methods
   from the newly compiled translation unit.  The new functions access
   the existing static data from the old translation unit, if the data
   existed in the unit to be replaced, and from the new translation
   unit, for new data.

   The changes are to insert 5 nops at the beginning of all functions
   and to use indirection to get at static duration data.  The 5 nops
   are required by consumers of the generated code.  Currently, gdb
   uses this to patch in a jump to the overriding function, this
   allows all uses of the old name to forward to the replacement,
   including existing function pointers and virtual methods.  See
   rs6000_emit_prologue for the code that handles the nop insertions.
 
   The added indirection allows gdb to redirect accesses to static
   duration data from the newly loaded translation unit to the
   existing data, if any.  @code{static} data is special and is
   handled by setting the second word in the .non_lazy_symbol_pointer
   data structure to the address of the data.  See indirect_data for
   the code that handles the extra indirection, and
   machopic_output_indirection and its use of MACHO_SYMBOL_STATIC for
   the code that handles @code{static} data indirection.  */
/* APPLE LOCAL end mainline 5 nops */

/* Nonzero if the user passes the -mone-byte-bool switch, which forces
   sizeof(bool) to be 1. */
const char *darwin_one_byte_bool = 0;

/* APPLE LOCAL begin pragma reverse_bitfields */
/* Shouldn't there be a comment here?  */
int darwin_reverse_bitfields = 0;
/* APPLE LOCAL end pragma reverse_bitfields */

/* APPLE LOCAL begin AT&T-style stub 4164563 */
/* This is an i386-only option, but the i386 target_flags bitset is full.
   This should resolve itself in 4.1; these decls, and their flags,
   should move to i386/i386.c.  */
int darwin_macho_att_stub = 1;	/* Defaults on.  */
const char *darwin_macho_att_stub_switch;
/* APPLE LOCAL end AT&T-style stub 4164563 */

int
name_needs_quotes (const char *name)
{
  int c;
  while ((c = *name++) != '\0')
    if (! ISIDNUM (c) && c != '.' && c != '$')
      return 1;
  return 0;
}

/* Return true if SYM_REF can be used without an indirection.  */
/* APPLE LOCAL what is this change for? */
int
machopic_symbol_defined_p (rtx sym_ref)
{
  if (SYMBOL_REF_FLAGS (sym_ref) & MACHO_SYMBOL_FLAG_DEFINED)
    return true;

  /* If a symbol references local and is not an extern to this
     file, then the symbol might be able to declared as defined.  */
  if (SYMBOL_REF_LOCAL_P (sym_ref) && ! SYMBOL_REF_EXTERNAL_P (sym_ref))
    {
      /* If the symbol references a variable and the variable is a
	 common symbol, then this symbol is not defined.  */
      if (SYMBOL_REF_FLAGS (sym_ref) & MACHO_SYMBOL_FLAG_VARIABLE)
	{
	  tree decl = SYMBOL_REF_DECL (sym_ref);
	  if (!decl)
	    return true;
	  if (DECL_COMMON (decl))
	    return false;
	}
      return true;
    }
  return false;
}

/* This module assumes that (const (symbol_ref "foo")) is a legal pic
   reference, which will not be changed.  */

enum machopic_addr_class
machopic_classify_symbol (rtx sym_ref)
{
  int flags;
  bool function_p;

  flags = SYMBOL_REF_FLAGS (sym_ref);
  function_p = SYMBOL_REF_FUNCTION_P (sym_ref);
  if (machopic_symbol_defined_p (sym_ref))
    return (function_p 
	    ? MACHOPIC_DEFINED_FUNCTION : MACHOPIC_DEFINED_DATA);
  else
    return (function_p 
	    ? MACHOPIC_UNDEFINED_FUNCTION : MACHOPIC_UNDEFINED_DATA);
}

#ifndef TARGET_FIX_AND_CONTINUE
#define TARGET_FIX_AND_CONTINUE 0
#endif

/* Indicate when fix-and-continue style code generation is being used
   and when a reference to data should be indirected so that it can be
   rebound in a new translation unit to reference the original instance
   of that data.  Symbol names that are for code generation local to
   the translation unit are bound to the new translation unit;
   currently this means symbols that begin with L or _OBJC_;
   otherwise, we indicate that an indirect reference should be made to
   permit the runtime to rebind new instances of the translation unit
   to the original instance of the data.  */

static int
indirect_data (rtx sym_ref)
{
  int lprefix;
  const char *name;

  /* If we aren't generating fix-and-continue code, don't do anything special.  */
  if (TARGET_FIX_AND_CONTINUE == 0)
    return 0;

  /* Otherwise, all symbol except symbols that begin with L or _OBJC_
     are indirected.  Symbols that begin with L and _OBJC_ are always
     bound to the current translation unit as they are used for
     generated local data of the translation unit.  */

  name = XSTR (sym_ref, 0);

  lprefix = (((name[0] == '*' || name[0] == '&')
              && (name[1] == 'L' || (name[1] == '"' && name[2] == 'L')))
	     /* APPLE LOCAL mainline */
             || (strncmp (name, "_OBJC_", 6) == 0));

  return ! lprefix;
}


static int
machopic_data_defined_p (rtx sym_ref)
{
  if (indirect_data (sym_ref))
    return 0;

  switch (machopic_classify_symbol (sym_ref))
    {
    case MACHOPIC_DEFINED_DATA:
    case MACHOPIC_DEFINED_FUNCTION:
      return 1;
    default:
      return 0;
    }
}

void
machopic_define_symbol (rtx mem)
{
  rtx sym_ref;
  if (GET_CODE (mem) != MEM)
    abort ();
  sym_ref = XEXP (mem, 0);
  SYMBOL_REF_FLAGS (sym_ref) |= MACHO_SYMBOL_FLAG_DEFINED;
}

static GTY(()) char * function_base;

const char *
machopic_function_base_name (void)
{
  /* if dynamic-no-pic is on, we should not get here */
  if (MACHO_DYNAMIC_NO_PIC_P)
    abort ();

  if (function_base == NULL)
    function_base =
      (char *) ggc_alloc_string ("<pic base>", sizeof ("<pic base>"));

  current_function_uses_pic_offset_table = 1;

  return function_base;
}

/* Return a SYMBOL_REF for the PIC function base.  */

rtx
machopic_function_base_sym (void)
{
  rtx sym_ref;

  sym_ref = gen_rtx_SYMBOL_REF (Pmode, machopic_function_base_name ());
  SYMBOL_REF_FLAGS (sym_ref) 
    |= (MACHO_SYMBOL_FLAG_VARIABLE | MACHO_SYMBOL_FLAG_DEFINED);
  return sym_ref;
}

static GTY(()) const char * function_base_func_name;
static GTY(()) int current_pic_label_num;

void
machopic_output_function_base_name (FILE *file)
{
  const char *current_name;

  /* If dynamic-no-pic is on, we should not get here.  */
  if (MACHO_DYNAMIC_NO_PIC_P)
    abort ();
  current_name =
    IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (current_function_decl));
  if (function_base_func_name != current_name)
    {
      ++current_pic_label_num;
      function_base_func_name = current_name;
    }
  fprintf (file, "\"L%011d$pb\"", current_pic_label_num);
}

/* The suffix attached to non-lazy pointer symbols.  */
#define NON_LAZY_POINTER_SUFFIX "$non_lazy_ptr"
/* The suffix attached to stub symbols.  */
#define STUB_SUFFIX "$stub"

typedef struct machopic_indirection GTY (())
{
  /* The SYMBOL_REF for the entity referenced.  */
  rtx symbol;
  /* The name of the stub or non-lazy pointer.  */
  const char * ptr_name;
  /* True iff this entry is for a stub (as opposed to a non-lazy
     pointer).  */
  bool stub_p;
  /* True iff this stub or pointer pointer has been referenced.  */
  bool used;
} machopic_indirection;

/* A table mapping stub names and non-lazy pointer names to
   SYMBOL_REFs for the stubbed-to and pointed-to entities.  */

static GTY ((param_is (struct machopic_indirection))) htab_t 
  machopic_indirections;

/* Return a hash value for a SLOT in the indirections hash table.  */

static hashval_t
machopic_indirection_hash (const void *slot)
{
  const machopic_indirection *p = (const machopic_indirection *) slot;
  return htab_hash_string (p->ptr_name);
}

/* Returns true if the KEY is the same as that associated with
   SLOT.  */

static int
machopic_indirection_eq (const void *slot, const void *key)
{
  return strcmp (((const machopic_indirection *) slot)->ptr_name, key) == 0;
}

/* Return the name of the non-lazy pointer (if STUB_P is false) or
   stub (if STUB_B is true) corresponding to the given name.  */

const char *
machopic_indirection_name (rtx sym_ref, bool stub_p)
{
  char *buffer;
  const char *name = XSTR (sym_ref, 0);
  size_t namelen = strlen (name);
  machopic_indirection *p;
  void ** slot;
  /* APPLE LOCAL begin mainline */
  bool saw_star = false;
  bool needs_quotes;
  const char *suffix;
  const char *prefix = user_label_prefix;
  const char *quote = "";
  
  if (name[0] == '*')
    {
      saw_star = true;
      prefix = "";
      ++name;
      --namelen;
    }

  needs_quotes = name_needs_quotes (name);
  if (needs_quotes)
    {
      quote = "\"";
    }

  if (stub_p)
    suffix = STUB_SUFFIX;
  else
    suffix = NON_LAZY_POINTER_SUFFIX;

  buffer = alloca (strlen ("&L")
		   + strlen (prefix)
		   + namelen
		   + strlen (suffix)
		   + 2 * strlen (quote)
		   + 1 /* '\0' */);

  /* Construct the name of the non-lazy pointer or stub.  */
  sprintf (buffer, "&%sL%s%s%s%s", quote, prefix, name, suffix, quote);
  /* APPLE LOCAL end mainline */

  if (!machopic_indirections)
    machopic_indirections = htab_create_ggc (37, 
					     machopic_indirection_hash,
					     machopic_indirection_eq,
					     /*htab_del=*/NULL);
  
  slot = htab_find_slot_with_hash (machopic_indirections, buffer,
				   htab_hash_string (buffer), INSERT);
  if (*slot)
    {
      p = (machopic_indirection *) *slot;
    }
  else
    {
      p = (machopic_indirection *) ggc_alloc (sizeof (machopic_indirection));
      p->symbol = sym_ref;
      p->ptr_name = xstrdup (buffer);
      p->stub_p = stub_p;
      p->used = false;
      *slot = p;
    }
  
  return p->ptr_name;
}

/* Return the name of the stub for the mcount function.  */

const char*
machopic_mcount_stub_name (void)
{
  rtx symbol = gen_rtx_SYMBOL_REF (Pmode, "*mcount");
  return machopic_indirection_name (symbol, /*stub_p=*/true);
}

/* If NAME is the name of a stub or a non-lazy pointer , mark the stub
   or non-lazy pointer as used -- and mark the object to which the
   pointer/stub refers as used as well, since the pointer/stub will
   emit a reference to it.  */

void
machopic_validate_stub_or_non_lazy_ptr (const char *name)
{
  machopic_indirection *p;
  
  p = ((machopic_indirection *) 
       (htab_find_with_hash (machopic_indirections, name,
			     htab_hash_string (name))));
  if (p && ! p->used)
    {
      const char *real_name;
      tree id;
      
      p->used = true;

      /* Do what output_addr_const will do when we actually call it.  */
      if (SYMBOL_REF_DECL (p->symbol))
	mark_decl_referenced (SYMBOL_REF_DECL (p->symbol));

      real_name = targetm.strip_name_encoding (XSTR (p->symbol, 0));
      
      id = maybe_get_identifier (real_name);
      if (id)
	mark_referenced (id);
    }
}

/* Transform ORIG, which may be any data source, to the corresponding
   source using indirections.  */

rtx
machopic_indirect_data_reference (rtx orig, rtx reg)
{
  rtx ptr_ref = orig;

  if (! MACHOPIC_INDIRECT)
    return orig;

  /* APPLE LOCAL begin dynamic-no-pic  */
  switch (GET_CODE (orig))
    {
    case SYMBOL_REF:
      {
	int defined = machopic_data_defined_p (orig);

      if (defined && MACHO_DYNAMIC_NO_PIC_P)
	{
#if defined (TARGET_TOC)
 	  emit_insn (gen_macho_high (reg, orig));
 	  emit_insn (gen_macho_low (reg, reg, orig));
#else
#if defined (TARGET_386)
	    return orig;
#else /* defined (TARGET_386) */
	    /* some other cpu -- writeme!  */
	    abort ();
#endif /* defined (TARGET_386) */
#endif
	   return reg;
	}
      else if (defined)
	{
#if defined (TARGET_TOC) || defined (HAVE_lo_sum)
	  rtx pic_base = machopic_function_base_sym ();
	  rtx offset = gen_rtx_CONST (Pmode,
				      gen_rtx_MINUS (Pmode, orig, pic_base));
#endif

#if defined (TARGET_TOC) /* i.e., PowerPC */
	  rtx hi_sum_reg = (no_new_pseudos ? reg : gen_reg_rtx (Pmode));

	  if (reg == NULL)
	    abort ();

	  emit_insn (gen_rtx_SET (Pmode, hi_sum_reg,
			      gen_rtx_PLUS (Pmode, pic_offset_table_rtx,
				       gen_rtx_HIGH (Pmode, offset))));
	  emit_insn (gen_rtx_SET (Pmode, reg,
				  gen_rtx_LO_SUM (Pmode, hi_sum_reg, offset)));

	  orig = reg;
#else
#if defined (HAVE_lo_sum)
	  if (reg == 0) abort ();

	  emit_insn (gen_rtx_SET (VOIDmode, reg,
				  gen_rtx_HIGH (Pmode, offset)));
	  emit_insn (gen_rtx_SET (VOIDmode, reg,
				  gen_rtx_LO_SUM (Pmode, reg, offset)));
	  emit_insn (gen_rtx_USE (VOIDmode, pic_offset_table_rtx));

	  orig = gen_rtx_PLUS (Pmode, pic_offset_table_rtx, reg);
#endif
#endif
	  return orig;
	}

      ptr_ref = (gen_rtx_SYMBOL_REF
		 (Pmode, 
		  machopic_indirection_name (orig, /*stub_p=*/false)));

      SYMBOL_REF_DECL (ptr_ref) = SYMBOL_REF_DECL (orig);

      ptr_ref = gen_const_mem (Pmode, ptr_ref);
      machopic_define_symbol (ptr_ref);

#ifdef TARGET_386
	if (reg && TARGET_DYNAMIC_NO_PIC)
	  {
	    emit_insn (gen_rtx_SET (Pmode, reg, ptr_ref));
	    ptr_ref = reg;
	  }
#endif	/* TARGET_386 */

      return ptr_ref;
    }
      break;
  
    case CONST:
      {
	/* If "(const (plus ...", walk the PLUS and return that result.
	   PLUS processing (below) will restore the "(const ..." if
	   appropriate.  */
	if (GET_CODE (XEXP (orig, 0)) == PLUS)
	  return machopic_indirect_data_reference (XEXP (orig, 0), reg);
	else 
	  return orig;
      }
      break;
  
    case MEM:
      {
	XEXP (ptr_ref, 0) = machopic_indirect_data_reference (XEXP (orig, 0), reg);
	return ptr_ref;
      }
      break;
  
    case PLUS:
      {
	rtx base, result;

	/* When the target is i386, this code prevents crashes due to the
	   compiler's ignorance on how to move the PIC base register to
	   other registers.  (The reload phase sometimes introduces such
	   insns.)  */
	if (GET_CODE (XEXP (orig, 0)) == REG
	    && REGNO (XEXP (orig, 0)) == PIC_OFFSET_TABLE_REGNUM
#ifdef TARGET_386
	    /* Prevent the same register from being erroneously used
	       as both the base and index registers.  */
	    && GET_CODE (XEXP (orig, 1)) == CONST
#endif
	    && reg)
	  {
	    emit_move_insn (reg, XEXP (orig, 0));
	    XEXP (ptr_ref, 0) = reg;
	    return ptr_ref;
	  }

	/* Legitimize both operands of the PLUS.  */
	base = machopic_indirect_data_reference (XEXP (orig, 0), reg);
	orig = machopic_indirect_data_reference (XEXP (orig, 1),
						 (base == reg ? 0 : reg));
	if (MACHOPIC_INDIRECT && GET_CODE (orig) == CONST_INT)
	  result = plus_constant (base, INTVAL (orig));
	else
	  result = gen_rtx_PLUS (Pmode, base, orig);

	if (MACHOPIC_JUST_INDIRECT && GET_CODE (base) == MEM)
	  {
	    if (reg)
	      {
		emit_move_insn (reg, result);
		result = reg;
	      }
	    else
	      result = force_reg (GET_MODE (result), result);
	  }
	return result;
      }
      break;

    default:
      break;
    }	/* End switch (GET_CODE (orig)) */
  /* APPLE LOCAL end dynamic-no-pic */
  return ptr_ref;
}

/* Transform TARGET (a MEM), which is a function call target, to the
   corresponding symbol_stub if necessary.  Return a new MEM.  */

rtx
machopic_indirect_call_target (rtx target)
{
  if (GET_CODE (target) != MEM)
    return target;

  if (MACHOPIC_INDIRECT 
      && GET_CODE (XEXP (target, 0)) == SYMBOL_REF
      && !(SYMBOL_REF_FLAGS (XEXP (target, 0))
	   & MACHO_SYMBOL_FLAG_DEFINED))
    {
      rtx sym_ref = XEXP (target, 0);
      const char *stub_name = machopic_indirection_name (sym_ref, 
							 /*stub_p=*/true);
      enum machine_mode mode = GET_MODE (sym_ref);
      tree decl = SYMBOL_REF_DECL (sym_ref);
      
      XEXP (target, 0) = gen_rtx_SYMBOL_REF (mode, stub_name);
      SYMBOL_REF_DECL (XEXP (target, 0)) = decl;
      MEM_READONLY_P (target) = 1;
      MEM_NOTRAP_P (target) = 1;
    }

  return target;
}

rtx
machopic_legitimize_pic_address (rtx orig, enum machine_mode mode, rtx reg)
{
  rtx pic_ref = orig;

  if (! MACHOPIC_INDIRECT)
    return orig;

  /* First handle a simple SYMBOL_REF or LABEL_REF */
  if (GET_CODE (orig) == LABEL_REF
      || (GET_CODE (orig) == SYMBOL_REF
	  ))
    {
      /* addr(foo) = &func+(foo-func) */
      rtx pic_base;

      orig = machopic_indirect_data_reference (orig, reg);

      if (GET_CODE (orig) == PLUS
	  && GET_CODE (XEXP (orig, 0)) == REG)
	{
	  if (reg == 0)
	    return force_reg (mode, orig);

	  emit_move_insn (reg, orig);
	  return reg;
	}

      /* if dynamic-no-pic then use 0 as the pic base  */
      if (MACHO_DYNAMIC_NO_PIC_P)
	pic_base = CONST0_RTX (Pmode);
      else
	pic_base = machopic_function_base_sym ();

      if (GET_CODE (orig) == MEM)
	{
	  if (reg == 0)
	    {
	      if (reload_in_progress)
		abort ();
	      else
		reg = gen_reg_rtx (Pmode);
	    }

#ifdef HAVE_lo_sum
	  if (MACHO_DYNAMIC_NO_PIC_P
	      && (GET_CODE (XEXP (orig, 0)) == SYMBOL_REF
		  || GET_CODE (XEXP (orig, 0)) == LABEL_REF))
	    {
#if defined (TARGET_TOC)	/* ppc  */
	      rtx temp_reg = (no_new_pseudos) ? reg : gen_reg_rtx (Pmode);
	      rtx asym = XEXP (orig, 0);
	      rtx mem;

	      emit_insn (gen_macho_high (temp_reg, asym));
	      mem = gen_const_mem (GET_MODE (orig),
				   gen_rtx_LO_SUM (Pmode, temp_reg, asym));
	      emit_insn (gen_rtx_SET (VOIDmode, reg, mem));
#else
	      /* Some other CPU -- WriteMe! but right now there are no other platform that can use dynamic-no-pic  */
	      abort ();
#endif
	      pic_ref = reg;
	    }
	  else
	  if (GET_CODE (XEXP (orig, 0)) == SYMBOL_REF
	      || GET_CODE (XEXP (orig, 0)) == LABEL_REF)
	    {
	      rtx offset = gen_rtx_CONST (Pmode,
					  gen_rtx_MINUS (Pmode,
							 XEXP (orig, 0),
							 pic_base));
#if defined (TARGET_TOC) /* i.e., PowerPC */
	      /* Generating a new reg may expose opportunities for
		 common subexpression elimination.  */
              rtx hi_sum_reg = no_new_pseudos ? reg : gen_reg_rtx (Pmode);
	      rtx mem;
	      rtx insn;
	      rtx sum;
	      
	      sum = gen_rtx_HIGH (Pmode, offset);
	      if (! MACHO_DYNAMIC_NO_PIC_P)
		sum = gen_rtx_PLUS (Pmode, pic_offset_table_rtx, sum);

	      emit_insn (gen_rtx_SET (Pmode, hi_sum_reg, sum));

	      mem = gen_const_mem (GET_MODE (orig),
				  gen_rtx_LO_SUM (Pmode, 
						  hi_sum_reg, offset));
	      insn = emit_insn (gen_rtx_SET (VOIDmode, reg, mem));
	      REG_NOTES (insn) = gen_rtx_EXPR_LIST (REG_EQUAL, pic_ref, 
						    REG_NOTES (insn));

	      pic_ref = reg;
#else
	      emit_insn (gen_rtx_USE (VOIDmode,
				      gen_rtx_REG (Pmode, 
						   PIC_OFFSET_TABLE_REGNUM)));

	      emit_insn (gen_rtx_SET (VOIDmode, reg,
				      gen_rtx_HIGH (Pmode,
						    gen_rtx_CONST (Pmode, 
								   offset))));
	      emit_insn (gen_rtx_SET (VOIDmode, reg,
				  gen_rtx_LO_SUM (Pmode, reg,
					   gen_rtx_CONST (Pmode, offset))));
	      pic_ref = gen_rtx_PLUS (Pmode,
				      pic_offset_table_rtx, reg);
#endif
	    }
	  else
#endif  /* HAVE_lo_sum */
	    {
	      rtx pic = pic_offset_table_rtx;
	      if (GET_CODE (pic) != REG)
		{
		  emit_move_insn (reg, pic);
		  pic = reg;
		}
#if 0
	      emit_insn (gen_rtx_USE (VOIDmode,
				      gen_rtx_REG (Pmode, 
						   PIC_OFFSET_TABLE_REGNUM)));
#endif

	      /* APPLE LOCAL begin 4278461 */	
	      if (reload_in_progress)
		regs_ever_live[REGNO (pic)] = 1;
	      /* APPLE LOCAL end 4278461 */	
	      pic_ref = gen_rtx_PLUS (Pmode,
				      pic,
				      gen_rtx_CONST (Pmode,
					  gen_rtx_MINUS (Pmode,
							 XEXP (orig, 0),
							 pic_base)));
	    }

#if !defined (TARGET_TOC)
	  emit_move_insn (reg, pic_ref);
	  pic_ref = gen_const_mem (GET_MODE (orig), reg);
#endif
	}
      else
	{

#ifdef HAVE_lo_sum
	  if (GET_CODE (orig) == SYMBOL_REF
	      || GET_CODE (orig) == LABEL_REF)
	    {
	      rtx offset = gen_rtx_CONST (Pmode,
					  gen_rtx_MINUS (Pmode, 
							 orig, pic_base));
#if defined (TARGET_TOC) /* i.e., PowerPC */
              rtx hi_sum_reg;

	      if (reg == 0)
		{
		  if (reload_in_progress)
		    abort ();
		  else
		    reg = gen_reg_rtx (Pmode);
		}

	      hi_sum_reg = reg;

	      emit_insn (gen_rtx_SET (Pmode, hi_sum_reg,
				      (MACHO_DYNAMIC_NO_PIC_P)
				      ? gen_rtx_HIGH (Pmode, offset)
				      : gen_rtx_PLUS (Pmode,
						      pic_offset_table_rtx,
						      gen_rtx_HIGH (Pmode, 
								    offset))));
	      emit_insn (gen_rtx_SET (VOIDmode, reg,
				      gen_rtx_LO_SUM (Pmode,
						      hi_sum_reg, offset)));
	      pic_ref = reg;
#else
	      emit_insn (gen_rtx_SET (VOIDmode, reg,
				      gen_rtx_HIGH (Pmode, offset)));
	      emit_insn (gen_rtx_SET (VOIDmode, reg,
				      gen_rtx_LO_SUM (Pmode, reg, offset)));
	      pic_ref = gen_rtx_PLUS (Pmode,
				      pic_offset_table_rtx, reg);
#endif
	    }
	  else
#endif  /*  HAVE_lo_sum  */
	    {
	      if (REG_P (orig)
	          || GET_CODE (orig) == SUBREG)
		{
		  return orig;
		}
	      else
		{
		  rtx pic = pic_offset_table_rtx;
		  if (GET_CODE (pic) != REG)
		    {
		      emit_move_insn (reg, pic);
		      pic = reg;
		    }
#if 0
		  emit_insn (gen_rtx_USE (VOIDmode,
					  pic_offset_table_rtx));
#endif
	      /* APPLE LOCAL begin 4278461 */	
	      if (reload_in_progress)
		regs_ever_live[REGNO (pic)] = 1;
	      /* APPLE LOCAL end 4278461 */	
		  pic_ref = gen_rtx_PLUS (Pmode,
					  pic,
					  gen_rtx_CONST (Pmode,
					      gen_rtx_MINUS (Pmode,
							     orig, pic_base)));
		}
	    }
	}

      if (GET_CODE (pic_ref) != REG)
        {
          if (reg != 0)
            {
              emit_move_insn (reg, pic_ref);
              return reg;
            }
          else
            {
              return force_reg (mode, pic_ref);
            }
        }
      else
        {
          return pic_ref;
        }
    }

  else if (GET_CODE (orig) == SYMBOL_REF)
    return orig;

  else if (GET_CODE (orig) == PLUS
	   && (GET_CODE (XEXP (orig, 0)) == MEM
	       || GET_CODE (XEXP (orig, 0)) == SYMBOL_REF
	       || GET_CODE (XEXP (orig, 0)) == LABEL_REF)
	   && XEXP (orig, 0) != pic_offset_table_rtx
	   && GET_CODE (XEXP (orig, 1)) != REG)

    {
      rtx base;
      int is_complex = (GET_CODE (XEXP (orig, 0)) == MEM);

      base = machopic_legitimize_pic_address (XEXP (orig, 0), Pmode, reg);
      orig = machopic_legitimize_pic_address (XEXP (orig, 1),
					      Pmode, (base == reg ? 0 : reg));
      if (GET_CODE (orig) == CONST_INT)
	{
	  pic_ref = plus_constant (base, INTVAL (orig));
	  is_complex = 1;
	}
      else
	pic_ref = gen_rtx_PLUS (Pmode, base, orig);

      /* APPLE LOCAL begin gen ADD */
#ifdef MASK_80387
      {
	rtx mem, other;

	if (GET_CODE (orig) == MEM) {
	    mem = orig; other = base;
	    /* Swap the kids only if there is only one MEM, and it's on the right.  */
	    if (GET_CODE (base) != MEM) {
		XEXP (pic_ref, 0) = orig;
		XEXP (pic_ref, 1) = base;
	      }
	  }
	else if (GET_CODE (base) == MEM) {
	    mem = base; other = orig;
	  } else
	    mem = other = NULL_RTX;
     
	/* Both kids are MEMs.  */
	if (other && GET_CODE (other) == MEM)
	  other = force_reg (GET_MODE (other), other);

	/* The x86 can't post-index a MEM; emit an ADD instruction to handle this.  */
	if (mem && GET_CODE (mem) == MEM) {
	  if ( ! reload_in_progress) {
	    rtx set = gen_rtx_SET (VOIDmode, reg, pic_ref);
	    rtx clobber_cc = gen_rtx_CLOBBER (VOIDmode, gen_rtx_REG (CCmode, FLAGS_REG));
	    pic_ref = gen_rtx_PARALLEL (VOIDmode, gen_rtvec (2, set, clobber_cc));
	    emit_insn (pic_ref);
	    pic_ref = reg;
	    is_complex = 0;
	  }
	}
      }
#endif
      /* APPLE LOCAL end gen ADD */

      if (reg && is_complex)
	{
	  emit_move_insn (reg, pic_ref);
	  pic_ref = reg;
	}
      /* Likewise, should we set special REG_NOTEs here?  */
    }

  else if (GET_CODE (orig) == CONST)
    {
      return machopic_legitimize_pic_address (XEXP (orig, 0), Pmode, reg);
    }

  else if (GET_CODE (orig) == MEM
	   && GET_CODE (XEXP (orig, 0)) == SYMBOL_REF)
    {
      /* APPLE LOCAL begin use new pseudo for temp; reusing reg confuses PRE */
      rtx tempreg = reg;
      rtx addr;
      if ( !no_new_pseudos )
	tempreg = gen_reg_rtx (Pmode);
      addr = machopic_legitimize_pic_address (XEXP (orig, 0), Pmode, tempreg);
      /* APPLE LOCAL end use new pseudo for temp; reusing reg confuses PRE */
      addr = replace_equiv_address (orig, addr);
      emit_move_insn (reg, addr);
      pic_ref = reg;
    }

  return pic_ref;
}

/* Output the stub or non-lazy pointer in *SLOT, if it has been used.
   DATA is the FILE* for assembly output.  Called from
   htab_traverse.  */

static int
machopic_output_indirection (void **slot, void *data)
{
  machopic_indirection *p = *((machopic_indirection **) slot);
  FILE *asm_out_file = (FILE *) data;
  rtx symbol;
  const char *sym_name;
  const char *ptr_name;
  
  if (!p->used)
    return 1;

  symbol = p->symbol;
  sym_name = XSTR (symbol, 0);
  ptr_name = p->ptr_name;
  
  if (p->stub_p)
    {
      char *sym;
      char *stub;

      sym = alloca (strlen (sym_name) + 2);
      if (sym_name[0] == '*' || sym_name[0] == '&')
	strcpy (sym, sym_name + 1);
      else if (sym_name[0] == '-' || sym_name[0] == '+')
	strcpy (sym, sym_name);
      else
	sprintf (sym, "%s%s", user_label_prefix, sym_name);

      stub = alloca (strlen (ptr_name) + 2);
      if (ptr_name[0] == '*' || ptr_name[0] == '&')
	strcpy (stub, ptr_name + 1);
      else
	sprintf (stub, "%s%s", user_label_prefix, ptr_name);

      machopic_output_stub (asm_out_file, sym, stub);
    }
  else if (! indirect_data (symbol)
	   && (machopic_symbol_defined_p (symbol)
	       || SYMBOL_REF_LOCAL_P (symbol)))
    {
      data_section ();
      assemble_align (GET_MODE_ALIGNMENT (Pmode));
      assemble_label (ptr_name);
      assemble_integer (gen_rtx_SYMBOL_REF (Pmode, sym_name),
			GET_MODE_SIZE (Pmode),
			GET_MODE_ALIGNMENT (Pmode), 1);
    }
  else
    {
      rtx init = const0_rtx;

      machopic_nl_symbol_ptr_section ();
      assemble_name (asm_out_file, ptr_name);
      fprintf (asm_out_file, ":\n");
      
      fprintf (asm_out_file, "\t.indirect_symbol ");
      assemble_name (asm_out_file, sym_name);
      fprintf (asm_out_file, "\n");
      
      /* Variables that are marked with MACHO_SYMBOL_STATIC need to
	 have their symbol name instead of 0 in the second entry of
	 the non-lazy symbol pointer data structure when they are
	 defined.  This allows the runtime to rebind newer instances
	 of the translation unit with the original instance of the
	 data.  */

      if ((SYMBOL_REF_FLAGS (symbol) & MACHO_SYMBOL_STATIC)
	  && machopic_symbol_defined_p (symbol))
	init = gen_rtx_SYMBOL_REF (Pmode, sym_name);

      assemble_integer (init, GET_MODE_SIZE (Pmode),
			GET_MODE_ALIGNMENT (Pmode), 1);
    }
  
  return 1;
}

void
machopic_finish (FILE *asm_out_file)
{
  if (machopic_indirections)
    htab_traverse_noresize (machopic_indirections,
			    machopic_output_indirection,
			    asm_out_file);
}

int
machopic_operand_p (rtx op)
{
  if (MACHOPIC_JUST_INDIRECT)
    {
      while (GET_CODE (op) == CONST)
	op = XEXP (op, 0);

      if (GET_CODE (op) == SYMBOL_REF)
	return machopic_symbol_defined_p (op);
      else
	return 0;
    }

  while (GET_CODE (op) == CONST)
    op = XEXP (op, 0);

  if (GET_CODE (op) == MINUS
      && GET_CODE (XEXP (op, 0)) == SYMBOL_REF
      && GET_CODE (XEXP (op, 1)) == SYMBOL_REF
      && machopic_symbol_defined_p (XEXP (op, 0))
      && machopic_symbol_defined_p (XEXP (op, 1)))
      return 1;

  return 0;
}

/* This function records whether a given name corresponds to a defined
   or undefined function or variable, for machopic_classify_ident to
   use later.  */

void
darwin_encode_section_info (tree decl, rtx rtl, int first ATTRIBUTE_UNUSED)
{
  rtx sym_ref;

  /* Do the standard encoding things first.  */
  default_encode_section_info (decl, rtl, first);

  if (TREE_CODE (decl) != FUNCTION_DECL && TREE_CODE (decl) != VAR_DECL)
    return;

  sym_ref = XEXP (rtl, 0);
  if (TREE_CODE (decl) == VAR_DECL)
    SYMBOL_REF_FLAGS (sym_ref) |= MACHO_SYMBOL_FLAG_VARIABLE;

  if (!DECL_EXTERNAL (decl)
      && (!TREE_PUBLIC (decl) || !DECL_WEAK (decl))
      && ((TREE_STATIC (decl)
	   && (!DECL_COMMON (decl) || !TREE_PUBLIC (decl)))
	  || (!DECL_COMMON (decl) && DECL_INITIAL (decl)
	      && DECL_INITIAL (decl) != error_mark_node)))
    SYMBOL_REF_FLAGS (sym_ref) |= MACHO_SYMBOL_FLAG_DEFINED;

  /* APPLE LOCAL begin mainline */
  if (! TREE_PUBLIC (decl))
    SYMBOL_REF_FLAGS (sym_ref) |= MACHO_SYMBOL_STATIC;
  /* APPLE LOCAL end mainline */

  /* APPLE LOCAL begin fix OBJC codegen */
  if (TREE_CODE (decl) == VAR_DECL)
    {
      if (strncmp (XSTR (sym_ref, 0), "_OBJC_", 6) == 0)
	SYMBOL_REF_FLAGS (sym_ref) |= MACHO_SYMBOL_FLAG_DEFINED;
    }
  /* APPLE LOCAL end fix OBJC codegen */
}

void
darwin_mark_decl_preserved (const char *name)
{
  fprintf (asm_out_file, ".no_dead_strip ");
  assemble_name (asm_out_file, name);
  fputc ('\n', asm_out_file);
}

void
machopic_select_section (tree exp, int reloc,
			 unsigned HOST_WIDE_INT align ATTRIBUTE_UNUSED)
{
  void (*base_function)(void);
  bool weak_p = DECL_P (exp) && DECL_WEAK (exp);
  /* APPLE LOCAL begin mainline 2005-04-15 <radar 4078608> */
  static void (* const base_funs[][2])(void) = {
    { text_section, text_coal_section },
    { unlikely_text_section, text_unlikely_coal_section },
    { readonly_data_section, const_coal_section },
    { const_data_section, const_data_coal_section },
    { data_section, data_coal_section }
  };
  /* APPLE LOCAL end mainline 2005-04-15 <radar 4078608> */

  if (TREE_CODE (exp) == FUNCTION_DECL)
    base_function = base_funs[reloc][weak_p];
  else if (decl_readonly_section_1 (exp, reloc, MACHOPIC_INDIRECT))
    base_function = base_funs[2][weak_p];
  else if (TREE_READONLY (exp) || TREE_CONSTANT (exp))
    base_function = base_funs[3][weak_p];
  else
    base_function = base_funs[4][weak_p];

  /* APPLE LOCAL begin fwritable strings  */
  if (TREE_CODE (exp) == STRING_CST
      && ((size_t) TREE_STRING_LENGTH (exp)
	  == strlen (TREE_STRING_POINTER (exp)) + 1)
      && ! flag_writable_strings)
    cstring_section ();
  /* APPLE LOCAL end fwritable strings  */
  else if ((TREE_CODE (exp) == INTEGER_CST || TREE_CODE (exp) == REAL_CST)
	   && flag_merge_constants)
    {
      tree size = TYPE_SIZE_UNIT (TREE_TYPE (exp));

      if (TREE_CODE (size) == INTEGER_CST &&
	  TREE_INT_CST_LOW (size) == 4 &&
	  TREE_INT_CST_HIGH (size) == 0)
	literal4_section ();
      else if (TREE_CODE (size) == INTEGER_CST &&
	       TREE_INT_CST_LOW (size) == 8 &&
	       TREE_INT_CST_HIGH (size) == 0)
	literal8_section ();
      else
	base_function ();
    }
  else if (TREE_CODE (exp) == CONSTRUCTOR
	   && TREE_TYPE (exp)
	   && TREE_CODE (TREE_TYPE (exp)) == RECORD_TYPE
	   && TYPE_NAME (TREE_TYPE (exp)))
    {
      /* APPLE LOCAL constant strings */
      extern int flag_next_runtime;
      tree name = TYPE_NAME (TREE_TYPE (exp));
      if (TREE_CODE (name) == TYPE_DECL)
	name = DECL_NAME (name);
      /* APPLE LOCAL begin 4149909 */
      if (!strcmp (IDENTIFIER_POINTER (name), "__builtin_ObjCString"))
	{
	  if (flag_next_runtime)
	    objc_constant_string_object_section ();
	  else
	    objc_string_object_section ();
	}
      /* APPLE LOCAL end 4149909 */
      /* APPLE LOCAL begin constant strings */
      else if (!strcmp (IDENTIFIER_POINTER (name), "__builtin_CFString"))
	cfstring_constant_object_section ();
      /* APPLE LOCAL end constant strings */
      else
	base_function ();
    }
  else if (TREE_CODE (exp) == VAR_DECL &&
	   DECL_NAME (exp) &&
	   TREE_CODE (DECL_NAME (exp)) == IDENTIFIER_NODE &&
	   IDENTIFIER_POINTER (DECL_NAME (exp)) &&
	   !strncmp (IDENTIFIER_POINTER (DECL_NAME (exp)), "_OBJC_", 6))
    {
      const char *name = IDENTIFIER_POINTER (DECL_NAME (exp));

      if (!strncmp (name, "_OBJC_CLASS_METHODS_", 20))
	objc_cls_meth_section ();
      else if (!strncmp (name, "_OBJC_INSTANCE_METHODS_", 23))
	objc_inst_meth_section ();
      else if (!strncmp (name, "_OBJC_CATEGORY_CLASS_METHODS_", 20))
	objc_cat_cls_meth_section ();
      else if (!strncmp (name, "_OBJC_CATEGORY_INSTANCE_METHODS_", 23))
	objc_cat_inst_meth_section ();
      else if (!strncmp (name, "_OBJC_CLASS_VARIABLES_", 22))
	objc_class_vars_section ();
      else if (!strncmp (name, "_OBJC_INSTANCE_VARIABLES_", 25))
	objc_instance_vars_section ();
      else if (!strncmp (name, "_OBJC_CLASS_PROTOCOLS_", 22))
	objc_cat_cls_meth_section ();
      else if (!strncmp (name, "_OBJC_CLASS_NAME_", 17))
	objc_class_names_section ();
      else if (!strncmp (name, "_OBJC_METH_VAR_NAME_", 20))
	objc_meth_var_names_section ();
      else if (!strncmp (name, "_OBJC_METH_VAR_TYPE_", 20))
	objc_meth_var_types_section ();
      else if (!strncmp (name, "_OBJC_CLASS_REFERENCES", 22))
	objc_cls_refs_section ();
      else if (!strncmp (name, "_OBJC_CLASS_", 12))
	objc_class_section ();
      else if (!strncmp (name, "_OBJC_METACLASS_", 16))
	objc_meta_class_section ();
      else if (!strncmp (name, "_OBJC_CATEGORY_", 15))
	objc_category_section ();
      else if (!strncmp (name, "_OBJC_SELECTOR_REFERENCES", 25))
	objc_selector_refs_section ();
      else if (!strncmp (name, "_OBJC_SELECTOR_FIXUP", 20))
	objc_selector_fixup_section ();
      else if (!strncmp (name, "_OBJC_SYMBOLS", 13))
	objc_symbols_section ();
      else if (!strncmp (name, "_OBJC_MODULES", 13))
	objc_module_info_section ();
      else if (!strncmp (name, "_OBJC_IMAGE_INFO", 16))
	objc_image_info_section ();
      else if (!strncmp (name, "_OBJC_PROTOCOL_INSTANCE_METHODS_", 32))
	objc_cat_inst_meth_section ();
      else if (!strncmp (name, "_OBJC_PROTOCOL_CLASS_METHODS_", 29))
	objc_cat_cls_meth_section ();
      else if (!strncmp (name, "_OBJC_PROTOCOL_REFS_", 20))
	objc_cat_cls_meth_section ();
      else if (!strncmp (name, "_OBJC_PROTOCOL_", 15))
	objc_protocol_section ();
      else
	base_function ();
    }
  /* APPLE LOCAL coalescing */
  /* Removed special handling of '::operator new' and '::operator delete'.  */
  /* APPLE LOCAL begin darwin_set_section_for_var_p  */
  else if (darwin_set_section_for_var_p (exp, reloc, align))
    ;
  /* APPLE LOCAL end darwin_set_section_for_var_p  */
  else
    base_function ();
}

/* This can be called with address expressions as "rtx".
   They must go in "const".  */

void
machopic_select_rtx_section (enum machine_mode mode, rtx x,
			     unsigned HOST_WIDE_INT align ATTRIBUTE_UNUSED)
{
  if (GET_MODE_SIZE (mode) == 8
      && (GET_CODE (x) == CONST_INT
	  || GET_CODE (x) == CONST_DOUBLE))
    literal8_section ();
  else if (GET_MODE_SIZE (mode) == 4
	   && (GET_CODE (x) == CONST_INT
	       || GET_CODE (x) == CONST_DOUBLE))
    literal4_section ();
  else if (MACHOPIC_INDIRECT
	   && (GET_CODE (x) == SYMBOL_REF
	       || GET_CODE (x) == CONST
	       || GET_CODE (x) == LABEL_REF))
    const_data_section ();
  else
    const_section ();
}

void
machopic_asm_out_constructor (rtx symbol, int priority ATTRIBUTE_UNUSED)
{
  if (MACHOPIC_INDIRECT)
    mod_init_section ();
  else
    constructor_section ();
  assemble_align (POINTER_SIZE);
  assemble_integer (symbol, POINTER_SIZE / BITS_PER_UNIT, POINTER_SIZE, 1);

  if (! MACHOPIC_INDIRECT)
    fprintf (asm_out_file, ".reference .constructors_used\n");
}

void
machopic_asm_out_destructor (rtx symbol, int priority ATTRIBUTE_UNUSED)
{
  if (MACHOPIC_INDIRECT)
    mod_term_section ();
  else
    destructor_section ();
  assemble_align (POINTER_SIZE);
  assemble_integer (symbol, POINTER_SIZE / BITS_PER_UNIT, POINTER_SIZE, 1);

  if (! MACHOPIC_INDIRECT)
    fprintf (asm_out_file, ".reference .destructors_used\n");
}

void
darwin_globalize_label (FILE *stream, const char *name)
{
  if (!!strncmp (name, "_OBJC_", 6))
    default_globalize_label (stream, name);
}

/* APPLE LOCAL begin assembly "abort" directive  */
/* This can be called instead of EXIT.  It will emit a '.abort' directive
   into any existing assembly file, causing assembly to immediately abort,
   thus preventing the assembler from spewing out numerous, irrelevant
   error messages.  */

void
abort_assembly_and_exit (int status)
{
  /* If we're aborting, get the assembler to abort, too.  */
  if (status == FATAL_EXIT_CODE && asm_out_file != 0)
    fprintf (asm_out_file, "\n.abort\n");

  exit (status);
}
/* APPLE LOCAL end assembly "abort" directive  */

/* APPLE LOCAL begin KEXT double destructor */
#include "c-common.h"

/* Handle __attribute__ ((apple_kext_compatibility)).
   This only applies to darwin kexts for 2.95 compatibility -- it shrinks the
   vtable for classes with this attribute (and their descendants) by not
   outputting the new 3.0 nondeleting destructor.  This means that such
   objects CANNOT be allocated on the stack or as globals UNLESS they have
   a completely empty `operator delete'.
   Luckily, this fits in with the Darwin kext model.
   
   This attribute also disables gcc3's potential overlaying of derived
   class data members on the padding at the end of the base class.  */

tree
darwin_handle_odd_attribute (tree *node, tree name, tree args ATTRIBUTE_UNUSED,
			     int flags ATTRIBUTE_UNUSED, bool *no_add_attrs)
{
  /* APPLE KEXT stuff -- only applies with pure static C++ code.  */
  if (! flag_apple_kext || ! c_dialect_cxx ())
    {
      warning ("`%s' 2.95 vtable-compatability attribute applies "
	       "only when compiling a kext", IDENTIFIER_POINTER (name));

      *no_add_attrs = true;
    }
  else if (TREE_CODE (*node) != RECORD_TYPE)
    {
      warning ("`%s' 2.95 vtable-compatability attribute applies "
	       "only to C++ classes", IDENTIFIER_POINTER (name));

      *no_add_attrs = true;
    }

  return NULL_TREE;
}
/* APPLE LOCAL end KEXT double destructor  */

/* APPLE LOCAL begin ObjC GC */
tree
darwin_handle_objc_gc_attribute (tree *node,
				 tree name,
				 tree args,
				 int flags ATTRIBUTE_UNUSED,
				 bool *no_add_attrs)
{
  tree orig = *node, type;

  /* Propagate GC-ness to the innermost pointee.  */
  while (POINTER_TYPE_P (orig)
	 || TREE_CODE (orig) == FUNCTION_TYPE
	 || TREE_CODE (orig) == METHOD_TYPE
	 || TREE_CODE (orig) == ARRAY_TYPE)
    orig = TREE_TYPE (orig);

  type = build_type_attribute_variant (orig,
				       tree_cons (name, args,
				       TYPE_ATTRIBUTES (orig)));

  /* For some reason, build_type_attribute_variant() creates a distinct
     type instead of a true variant!  We make up for this here.  */
  if (TYPE_MAIN_VARIANT (type) == type)
    {
      TYPE_MAIN_VARIANT (type) = orig;
      TYPE_NEXT_VARIANT (type) = TYPE_NEXT_VARIANT (orig);
      TYPE_NEXT_VARIANT (orig) = type;
    }

  *node = reconstruct_complex_type (*node, type);
  /* No need to hang on to the attribute any longer.  */
  *no_add_attrs = true;

  return NULL_TREE;
}
/* APPLE LOCAL end ObjC GC */

/* APPLE LOCAL begin darwin_set_section_for_var_p  20020226 --turly  */

/* This is specifically for any initialised static class constants
   which may be output by the C++ front end at the end of compilation. 
   SELECT_SECTION () macro won't do because these are VAR_DECLs, not
   STRING_CSTs or INTEGER_CSTs.  And by putting 'em in appropriate
   sections, we save space.  

   FIXME: does this really do anything?  Won't the DECL_WEAK test be
   true 99% (or 100%) of the time?  In the other 1% of the time,
   shouldn't select_section be fixed instead of this hackery?  */

extern void cstring_section (void),
	    literal4_section (void), literal8_section (void);
int
darwin_set_section_for_var_p (tree exp, int reloc, int align)
{
  if (!reloc && TREE_CODE (exp) == VAR_DECL
      && DECL_ALIGN (exp) == align 
      && TREE_READONLY (exp) && DECL_INITIAL (exp)
      && ! DECL_WEAK (exp))
    {
      /* Put constant string vars in ".cstring" section.  */

      if (TREE_CODE (TREE_TYPE (exp)) == ARRAY_TYPE
	  && TREE_CODE (TREE_TYPE (TREE_TYPE (exp))) == INTEGER_TYPE
	  && integer_onep (TYPE_SIZE_UNIT (TREE_TYPE (TREE_TYPE (exp))))
	  && TREE_CODE (DECL_INITIAL (exp)) == STRING_CST)
	{

	  /* Compare string length with actual number of characters
	     the compiler will write out (which is not necessarily
	     TREE_STRING_LENGTH, in the case of a constant array of
	     characters that is not null-terminated).   Select appropriate
	     section accordingly. */

	  if (MIN ( TREE_STRING_LENGTH (DECL_INITIAL(exp)),
		    int_size_in_bytes (TREE_TYPE (exp)))
	      == (long) strlen (TREE_STRING_POINTER (DECL_INITIAL (exp))) + 1)
	    {
	      cstring_section ();
	      return 1;
	    }
	  else
	    {
	      const_section ();
	      return 1;
	    }
	}
     else
      if (TREE_READONLY (exp) 
	  && ((TREE_CODE (TREE_TYPE (exp)) == INTEGER_TYPE
	       && TREE_CODE (DECL_INITIAL (exp)) == INTEGER_CST)
	      || (TREE_CODE (TREE_TYPE (exp)) == REAL_TYPE
	  	  && TREE_CODE (DECL_INITIAL (exp)) == REAL_CST))
	  && TREE_CODE (TYPE_SIZE_UNIT (TREE_TYPE (DECL_INITIAL (exp))))
		== INTEGER_CST)
	{
	  tree size = TYPE_SIZE_UNIT (TREE_TYPE (DECL_INITIAL (exp)));
	  if (TREE_INT_CST_HIGH (size) != 0)
	    return 0;

	  /* Put integer and float consts in the literal4|8 sections.  */

	  if (TREE_INT_CST_LOW (size) == 4)
	    {
	      literal4_section ();
	      return 1;
	    }
	  else if (TREE_INT_CST_LOW (size) == 8)
	    {
	      literal8_section ();                                
	      return 1;
	    }
	}
    }
  return 0;
}
/* APPLE LOCAL end darwin_set_section_for_var_p  20020226 --turly  */

void
darwin_asm_named_section (const char *name, 
			  unsigned int flags ATTRIBUTE_UNUSED,
			  tree decl ATTRIBUTE_UNUSED)
{
  fprintf (asm_out_file, "\t.section %s\n", name);
}

void 
darwin_unique_section (tree decl ATTRIBUTE_UNUSED, int reloc ATTRIBUTE_UNUSED)
{
  /* Darwin does not use unique sections.  */
}

/* Handle a "weak_import" attribute; arguments as in
   struct attribute_spec.handler.  */

tree
darwin_handle_weak_import_attribute (tree *node, tree name,
				     tree ARG_UNUSED (args),
				     int ARG_UNUSED (flags),
				     bool * no_add_attrs)
{
  if (TREE_CODE (*node) != FUNCTION_DECL && TREE_CODE (*node) != VAR_DECL)
    {
      warning ("%qs attribute ignored", IDENTIFIER_POINTER (name));
      *no_add_attrs = true;
    }
  else
    declare_weak (*node);

  return NULL_TREE;
}

static void
no_dead_strip (FILE *file, const char *lab)
{
  fprintf (file, ".no_dead_strip %s\n", lab);
}

/* Emit a label for an FDE, making it global and/or weak if appropriate. 
   The third parameter is nonzero if this is for exception handling.
   The fourth parameter is nonzero if this is just a placeholder for an
   FDE that we are omitting. */

void 
darwin_emit_unwind_label (FILE *file, tree decl, int for_eh, int empty)
{
  tree id = DECL_ASSEMBLER_NAME (decl)
    ? DECL_ASSEMBLER_NAME (decl)
    : DECL_NAME (decl);

  /* APPLE LOCAL begin mainline */
  const char *prefix = user_label_prefix;
  /* APPLE LOCAL end mainline */

  const char *base = IDENTIFIER_POINTER (id);
  unsigned int base_len = IDENTIFIER_LENGTH (id);

  const char *suffix = ".eh";

  int need_quotes = name_needs_quotes (base);
  int quotes_len = need_quotes ? 2 : 0;
  char *lab;

  if (! for_eh)
    suffix = ".eh1";

  /* APPLE LOCAL begin mainline */
  lab = xmalloc (strlen (prefix)
		 + base_len + strlen (suffix) + quotes_len + 1);
  /* APPLE LOCAL end mainline */
  lab[0] = '\0';

  if (need_quotes)
    strcat(lab, "\"");
  strcat(lab, prefix);
  strcat(lab, base);
  strcat(lab, suffix);
  if (need_quotes)
    strcat(lab, "\"");

  if (TREE_PUBLIC (decl))
    fprintf (file, "\t%s %s\n",
	     (DECL_VISIBILITY (decl) != VISIBILITY_HIDDEN
	      ? ".globl"
	      : ".private_extern"),
	     lab);

  if (DECL_WEAK (decl))
    fprintf (file, "\t.weak_definition %s\n", lab);

  if (empty)
    {
      fprintf (file, "%s = 0\n", lab);

      /* Mark the absolute .eh and .eh1 style labels as needed to
	 ensure that we don't dead code strip them and keep such
	 labels from another instantiation point until we can fix this
	 properly with group comdat support.  */
      no_dead_strip (file, lab);
    }
  else
    fprintf (file, "%s:\n", lab);

  free (lab);
}

/* Generate a PC-relative reference to a Mach-O non-lazy-symbol.  */ 

void
darwin_non_lazy_pcrel (FILE *file, rtx addr)
{
  const char *nlp_name;

  if (GET_CODE (addr) != SYMBOL_REF)
    abort ();

  nlp_name = machopic_indirection_name (addr, /*stub_p=*/false);
  fputs ("\t.long\t", file);
  ASM_OUTPUT_LABELREF (file, nlp_name);
  fputs ("-.", file);
}

/* Emit an assembler directive to set visibility for a symbol.  The
   only supported visibilities are VISIBILITY_DEFAULT and
   VISIBILITY_HIDDEN; the latter corresponds to Darwin's "private
   extern".  There is no MACH-O equivalent of ELF's
   VISIBILITY_INTERNAL or VISIBILITY_PROTECTED. */

void 
darwin_assemble_visibility (tree decl, int vis)
{
  if (vis == VISIBILITY_DEFAULT)
    ;
  else if (vis == VISIBILITY_HIDDEN)
    {
      fputs ("\t.private_extern ", asm_out_file);
      assemble_name (asm_out_file,
		     (IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl))));
      fputs ("\n", asm_out_file);
    }
  else
    warning ("internal and protected visibility attributes not supported "
	     "in this configuration; ignored");
}

/* Output a difference of two labels that will be an assembly time
   constant if the two labels are local.  (.long lab1-lab2 will be
   very different if lab1 is at the boundary between two sections; it
   will be relocated according to the second section, not the first,
   so one ends up with a difference between labels in different
   sections, which is bad in the dwarf2 eh context for instance.)  */

static int darwin_dwarf_label_counter;

void
darwin_asm_output_dwarf_delta (FILE *file, int size,
			       const char *lab1, const char *lab2)
{
  int islocaldiff = (lab1[0] == '*' && lab1[1] == 'L'
		     && lab2[0] == '*' && lab2[1] == 'L');
  const char *directive = (size == 8 ? ".quad" : ".long");

  if (islocaldiff)
    fprintf (file, "\t.set L$set$%d,", darwin_dwarf_label_counter);
  else
    fprintf (file, "\t%s\t", directive);
  assemble_name_raw (file, lab1);
  fprintf (file, "-");
  assemble_name_raw (file, lab2);
  if (islocaldiff)
    fprintf (file, "\n\t%s L$set$%d", directive, darwin_dwarf_label_counter++);
}

void
darwin_file_end (void)
{
  machopic_finish (asm_out_file);
  /* APPLE LOCAL constant cfstrings */
  if (darwin_running_cxx)
    {
      constructor_section ();
      destructor_section ();
      ASM_OUTPUT_ALIGN (asm_out_file, 1);
    }
  fprintf (asm_out_file, "\t.subsections_via_symbols\n");
}

/* True, iff we're generating fast turn around debugging code.  When
   true, we arrange for function prologues to start with 4 nops so
   that gdb may insert code to redirect them, and for data to accessed
   indirectly.  The runtime uses this indirection to forward
   references for data to the original instance of that data.  */

int darwin_fix_and_continue;
const char *darwin_fix_and_continue_switch;
/* APPLE LOCAL mainline 2005-09-01 3449986 */
const char *darwin_macosx_version_min;

/* APPLE LOCAL begin KEXT */
/* Ture, iff we're generating code for loadable kernel extentions.  */

bool
flag_apple_kext_p (void) {
  return flag_apple_kext;
}
/* APPLE LOCAL end KEXT */

/* APPLE LOCAL begin constant cfstrings */
int darwin_constant_cfstrings = 1;
const char *darwin_constant_cfstrings_switch;
int darwin_warn_nonportable_cfstrings = 1;  /* on by default. */
const char *darwin_warn_nonportable_cfstrings_switch;
int darwin_pascal_strings = 0;
const char *darwin_pascal_strings_switch;
int darwin_running_cxx;

static GTY(()) tree cfstring_class_reference = NULL_TREE;
static GTY(()) tree cfstring_type_node = NULL_TREE;
static GTY(()) tree ccfstring_type_node = NULL_TREE;
static GTY(()) tree pccfstring_type_node = NULL_TREE;
static GTY(()) tree pcint_type_node = NULL_TREE;
static GTY(()) tree pcchar_type_node = NULL_TREE;

/* Store all constructed constant CFStrings in a hash table so that
   they get uniqued properly.  */

struct cfstring_descriptor GTY(())
{
  /* The literal argument .  */
  tree literal;

  /* The resulting constant CFString.  */
  tree constructor;
};

static GTY((param_is (struct cfstring_descriptor))) htab_t cfstring_htab;

static hashval_t cfstring_hash (const void *);
static int cfstring_eq (const void *, const void *);

void
darwin_init_cfstring_builtins (void)
{
  tree field, fields, pccfstring_ftype_pcchar;

  /* struct __builtin_CFString {
       const int *isa;		(will point at
       int flags;		 __CFConstantStringClassReference)
       const char *str;
       int length;
     };  */

  pcint_type_node
    = build_pointer_type (build_qualified_type (integer_type_node,
			  TYPE_QUAL_CONST));
  pcchar_type_node
    = build_pointer_type (build_qualified_type (char_type_node,
			  TYPE_QUAL_CONST));
  cfstring_type_node = (*lang_hooks.types.make_type) (RECORD_TYPE);
  fields = build_decl (FIELD_DECL, NULL_TREE, pcint_type_node);
  field = build_decl (FIELD_DECL, NULL_TREE, integer_type_node);
  TREE_CHAIN (field) = fields; fields = field;
  field = build_decl (FIELD_DECL, NULL_TREE, pcchar_type_node);
  TREE_CHAIN (field) = fields; fields = field;
  field = build_decl (FIELD_DECL, NULL_TREE, integer_type_node);
  TREE_CHAIN (field) = fields; fields = field;
  /* NB: The finish_builtin_struct() routine expects FIELD_DECLs in
     reverse order!  */
  finish_builtin_struct (cfstring_type_node, "__builtin_CFString",
			 fields, NULL_TREE);

  /* const struct __builtin_CFstring *
     __builtin___CFStringMakeConstantString (const char *); */

  ccfstring_type_node
    = build_qualified_type (cfstring_type_node, TYPE_QUAL_CONST);
  pccfstring_type_node
    = build_pointer_type (ccfstring_type_node);
  pccfstring_ftype_pcchar
    = build_function_type_list (pccfstring_type_node,
				pcchar_type_node, NULL_TREE);
  lang_hooks.builtin_function ("__builtin___CFStringMakeConstantString",
			       pccfstring_ftype_pcchar,
			       DARWIN_BUILTIN_CFSTRINGMAKECONSTANTSTRING,
			       BUILT_IN_NORMAL, NULL, NULL_TREE);

  /* extern int __CFConstantStringClassReference[];  */
  cfstring_class_reference
   = build_decl (VAR_DECL,
		 get_identifier ("__CFConstantStringClassReference"),
		 build_array_type (integer_type_node, NULL_TREE));
  TREE_PUBLIC (cfstring_class_reference) = 1;
  TREE_USED (cfstring_class_reference) = 1;
  DECL_ARTIFICIAL (cfstring_class_reference) = 1;
  (*lang_hooks.decls.pushdecl) (cfstring_class_reference);
  DECL_EXTERNAL (cfstring_class_reference) = 1;
  rest_of_decl_compilation (cfstring_class_reference, 0, 0);
  
  /* Initialize the hash table used to hold the constant CFString objects.  */
  cfstring_htab = htab_create_ggc (31, cfstring_hash,
				   cfstring_eq, NULL);
}

tree
darwin_expand_tree_builtin (tree function, tree params,
			    tree coerced_params ATTRIBUTE_UNUSED)
{
  unsigned int fcode = DECL_FUNCTION_CODE (function);

  switch (fcode)
    {
    case DARWIN_BUILTIN_CFSTRINGMAKECONSTANTSTRING:
      if (!darwin_constant_cfstrings)
	{
	  error ("built-in function `%s' requires `-fconstant-cfstrings' flag",
		 IDENTIFIER_POINTER (DECL_NAME (function)));
	  return error_mark_node;
	}

      return darwin_build_constant_cfstring (TREE_VALUE (params));
    default:
      break;
    }

  return NULL_TREE;
}

static hashval_t
cfstring_hash (const void *ptr)
{
  tree str = ((struct cfstring_descriptor *)ptr)->literal;
  const unsigned char *p = (const unsigned char *) TREE_STRING_POINTER (str);
  int i, len = TREE_STRING_LENGTH (str);
  hashval_t h = len;

  for (i = 0; i < len; i++)
    h = ((h * 613) + p[i]);

  return h;
}

static int
cfstring_eq (const void *ptr1, const void *ptr2)
{
  tree str1 = ((struct cfstring_descriptor *)ptr1)->literal;
  tree str2 = ((struct cfstring_descriptor *)ptr2)->literal;
  int len1 = TREE_STRING_LENGTH (str1);

  return (len1 == TREE_STRING_LENGTH (str2)
	  && !memcmp (TREE_STRING_POINTER (str1), TREE_STRING_POINTER (str2),
		      len1));
}

tree
darwin_construct_objc_string (tree str)
{
  if (!darwin_constant_cfstrings)
  /* APPLE LOCAL begin 4080358 */
    {
      /* Even though we are not using CFStrings, place our literal
	 into the cfstring_htab hash table, so that the
	 darwin_constant_cfstring_p() function below will see it.  */
      struct cfstring_descriptor key;
      void **loc;

      key.literal = str;
      loc = htab_find_slot (cfstring_htab, &key, INSERT);

      if (!*loc)
	{
	  *loc = ggc_alloc (sizeof (struct cfstring_descriptor));
	  ((struct cfstring_descriptor *)*loc)->literal = str;
	}

      return NULL_TREE;  /* Fall back to NSConstantString.  */
    }

  /* APPLE LOCAL end 4080358 */
  return darwin_build_constant_cfstring (str);
}

bool
darwin_constant_cfstring_p (tree str)
{
  struct cfstring_descriptor key;
  void **loc;

  if (!str)
    return false;

  STRIP_NOPS (str);

  if (TREE_CODE (str) == ADDR_EXPR)
    str = TREE_OPERAND (str, 0);

  if (TREE_CODE (str) != STRING_CST)
    return false;

  key.literal = str;
  loc = htab_find_slot (cfstring_htab, &key, NO_INSERT);
  
  if (loc)
    return true;

  return false;
}

static tree
darwin_build_constant_cfstring (tree str)
{
  struct cfstring_descriptor *desc, key;
  void **loc;
  tree addr;

  if (!str)
    goto invalid_string;

  STRIP_NOPS (str);

  if (TREE_CODE (str) == ADDR_EXPR)
    str = TREE_OPERAND (str, 0);

  if (TREE_CODE (str) != STRING_CST)
    {
     invalid_string:
      error ("CFString literal expression is not constant");
      return error_mark_node;
    }

  /* Perhaps we already constructed a constant CFString just like this one? */
  key.literal = str;
  loc = htab_find_slot (cfstring_htab, &key, INSERT);
  desc = *loc;

  if (!desc)
    {
      tree initlist, constructor, field = TYPE_FIELDS (ccfstring_type_node);
      tree var;
      int length = TREE_STRING_LENGTH (str) - 1;
      /* FIXME: The CFString functionality should probably reside
	 in darwin-c.c.  */
      extern tree pushdecl_top_level (tree);

      if (darwin_warn_nonportable_cfstrings)
	{
	  extern int isascii (int);
	  const char *s = TREE_STRING_POINTER (str);
	  int l = 0;

	  for (l = 0; l < length; l++)
	    if (!s[l] || !isascii (s[l]))
	      {
		warning ("%s in CFString literal",
			 s[l] ? "non-ASCII character" : "embedded NUL");
		break;
	      }
	}

      *loc = desc = ggc_alloc (sizeof (*desc));
      desc->literal = str;

      initlist = build_tree_list
		 (field, build1 (ADDR_EXPR, pcint_type_node, 
				 cfstring_class_reference));
      field = TREE_CHAIN (field);
      initlist = tree_cons (field, build_int_cst (NULL_TREE, 0x000007c8),
			    initlist);
      field = TREE_CHAIN (field);
      initlist = tree_cons (field,
			    build1 (ADDR_EXPR, pcchar_type_node,
				    str), initlist);
      field = TREE_CHAIN (field);
      initlist = tree_cons (field, build_int_cst (NULL_TREE, length),
			    initlist);

      constructor = build_constructor (ccfstring_type_node,
				       nreverse (initlist));
      TREE_READONLY (constructor) = 1;
      TREE_CONSTANT (constructor) = 1;
      TREE_STATIC (constructor) = 1;

      /* Fromage: The C++ flavor of 'build_unary_op' expects constructor nodes
	 to have the TREE_HAS_CONSTRUCTOR (...) bit set.  However, this file is
	 being built without any knowledge of C++ tree accessors; hence, we shall
	 use the generic accessor that TREE_HAS_CONSTRUCTOR actually maps to!  */
      if (darwin_running_cxx)
	TREE_LANG_FLAG_4 (constructor) = 1;   /* TREE_HAS_CONSTRUCTOR  */

      /* Create an anonymous global variable for this CFString.  */
      var = build_decl (CONST_DECL, NULL, TREE_TYPE (constructor));
      DECL_INITIAL (var) = constructor;
      TREE_STATIC (var) = 1;
      pushdecl_top_level (var);
      desc->constructor = var;
    }

  addr = build1 (ADDR_EXPR, pccfstring_type_node, desc->constructor);
  TREE_CONSTANT (addr) = 1;

  return addr;
}

/* APPLE LOCAL end constant cfstrings */

/* APPLE LOCAL begin CW asm blocks */
/* Assume labels like L_foo$stub etc in CW-style inline code are
   intended to be taken as literal labels, and return the identifier,
   otherwise return NULL signifying that we have no special
   knowledge.  */
tree
darwin_cw_asm_special_label (tree id)
{
  const char *name = IDENTIFIER_POINTER (id);

  if (name[0] == 'L')
    {
      int len = strlen (name);

      if ((len > 5 && strcmp (name + len - 5, "$stub") == 0)
	  || (len > 9 && strcmp (name + len - 9, "$lazy_ptr") == 0)
	  || (len > 13 && strcmp (name + len - 13, "$non_lazy_ptr") == 0))
	return id;
    }

  return NULL_TREE;
}
/* APPLE LOCAL end CW asm blocks */

#include "gt-darwin.h"
