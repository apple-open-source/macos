/* OPENSTEP and Mac OS X mach-o pic support functions.
   Copyright (C) 1992, 1994 Free Software Foundation, Inc.

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* 
 * flag_pic = 1 ... generate only indirections
 * flag_pic = 2 ... generate indirections and pure code
 */


/* 
 *   This module assumes that (const (symbol_ref "foo")) is
 *   a legal pic reference, which will not be changed.
 */
#include "config.h"

#ifdef MACHO_PIC  /* Covers this entire source file */

#include <stdio.h>
#include "tree.h"
#include "obcp/cp-tree.h"
#include "rtl.h"
#include "output.h"
#include "apple/machopic.h"
#include "insn-config.h"
#include "insn-flags.h"
#include "regs.h"
#ifdef TARGET_TOC /* i.e., PowerPC */
#include "flags.h"
#endif

int mcount_called = 0;		/* was mcount called at any point?  */

/* There were quite a few 256-char buffers around the place; compiling
   the Finder with it's gazillion nested namespaces has finally caused
   these to overflow.  We need a more generic mechanism, but for now
   I'm just increasing the buffer size to something ludicrously large.
   Alternatively, we should turn on squangling.  */
 
#define	MAX_NAME_LEN	1024	

/* Answer if the symbol named IDENT is known to be defined in 
   the current module.  It is critical, that it *never* says
   something is defined, when it isn't.  However, it is ok to be 
   sloppy on the other end of the scale, it will only generate 
   worse code than if it guessed correct. */

static tree machopic_defined_list = 0;

extern int flag_dave_indirect;

/* Called by various routines in varasm.c as the 'decl' is being written
   out to the assembly file.  This allows us (I hope) to determine whether
   or not we need to use PIC indirection.  */

void machopic_define_decl (decl, name)
    tree decl;
    const char *name;
{
  if (flag_pic)
    {
      if (TREE_CODE (decl) == FUNCTION_DECL)
      {
        /*fprintf(asm_out_file, "## NAME '%s', decl %p\n", name, decl);*/

        TREE_ASM_WRITTEN (get_identifier (name)) = 1;   /* Hmmm. */
	/*TREE_ASM_WRITTEN (decl) = 1; */
      } 
    }
}
enum machopic_addr_class
machopic_classify_ident (ident)
     tree ident;
{
  const char *name = IDENTIFIER_POINTER (ident);
  int lprefix = ((name[0] == '*' 
		  && (name[1] == 'L' || (name[1] == '"' && name[2] == 'L')))
		 || (   name[0] == '_' 
		     && name[1] == 'O' 
		     && name[2] == 'B' 
		     && name[3] == 'J'
		     && name[4] == 'C'
		     && name[5] == '_'));
    
  tree temp, decl  = lookup_name (ident, 0);

  if (!decl)
    {
      if (lprefix)
	{
	  char *name = IDENTIFIER_POINTER (ident);
	  int len = strlen(name);
	  if ( (len > 5 && !strcmp (name+len-5, "$stub"))
	    || (len > 6 && !strcmp (name+len-6, "$stub\"")))
	    return MACHOPIC_DEFINED_FUNCTION;
	  return MACHOPIC_DEFINED_DATA;
	}

      for (temp = machopic_defined_list;
	   temp != NULL_TREE; 
	   temp = TREE_CHAIN (temp))
	{
	  if (ident == TREE_VALUE (temp))
	    return MACHOPIC_DEFINED_DATA;
	}

      if (TREE_ASM_WRITTEN (ident))
	return MACHOPIC_DEFINED_DATA;

      return MACHOPIC_UNDEFINED;
    }

  /* variable declarations */
  else if (TREE_CODE (decl) == VAR_DECL)
    {
#ifdef HAVE_COALESCED_SYMBOLS
      if (DECL_COALESCED (decl))
	return MACHOPIC_UNDEFINED_DATA;
#endif
      if ((DECL_INITIAL (decl)
           || TREE_STATIC (decl))
          && ! TREE_PUBLIC (decl))
	return MACHOPIC_DEFINED_DATA;
    }

  /* function declarations */
  else if (TREE_CODE (decl) == FUNCTION_DECL
	   && (!DECL_EXTERNAL (decl)))
    {
#ifdef HAVE_COALESCED_SYMBOLS
      if (DECL_COALESCED (decl))
	return MACHOPIC_UNDEFINED_FUNCTION;
#endif
      if (TREE_STATIC (decl)
	  || TREE_ASM_WRITTEN (decl))
	return MACHOPIC_DEFINED_FUNCTION;
    }

#ifdef HAVE_COALESCED_SYMBOLS
  if (DECL_COALESCED (decl))
    return (TREE_CODE (decl) == FUNCTION_DECL) ? MACHOPIC_UNDEFINED_FUNCTION :
						 MACHOPIC_UNDEFINED_DATA;
#endif

  for (temp = machopic_defined_list;
       temp != NULL_TREE; 
       temp = TREE_CHAIN (temp))
    {
      if (ident == TREE_VALUE (temp))
	if (TREE_CODE (decl) == FUNCTION_DECL)
	  return MACHOPIC_DEFINED_FUNCTION;
	else
	  return MACHOPIC_DEFINED_DATA;
    }
  
  if (TREE_CODE (decl) == FUNCTION_DECL)
    {
      if (lprefix)
	return MACHOPIC_DEFINED_FUNCTION;
      else
	return MACHOPIC_UNDEFINED_FUNCTION;
    }
  else
    {
      if (lprefix)
	return MACHOPIC_DEFINED_DATA;
      else
	return MACHOPIC_UNDEFINED_DATA;
    }
}

     
enum machopic_addr_class
machopic_classify_name (name)
     const char *name;
{
  return machopic_classify_ident (get_identifier (name));
}

int
machopic_ident_defined_p (ident)
     tree ident;
{
  switch (machopic_classify_ident (ident))
    {
    case MACHOPIC_UNDEFINED:
    case MACHOPIC_UNDEFINED_DATA:
    case MACHOPIC_UNDEFINED_FUNCTION:
      return 0;
    default:
      return 1;
    }
}

int
machopic_data_defined_p (name)
     const char *name;
{
  switch (machopic_classify_ident (get_identifier (name)))
    {
    case MACHOPIC_DEFINED_DATA:
      return 1;
    default:
      return 0;
    }
}

int
machopic_name_defined_p (name)
     const char *name;
{
  return machopic_ident_defined_p (get_identifier (name));
}

void
machopic_define_ident (ident)
     tree ident;
{
  if (!machopic_ident_defined_p (ident))
    machopic_defined_list = 
      perm_tree_cons (NULL_TREE, ident, machopic_defined_list);
}

void
machopic_define_name (name)
     const char *name;
{
  machopic_define_ident (get_identifier (name));
}

/* This is a static to make inline functions work.  The rtx */
/* representing the PIC base symbol allways points to here. */
static char function_base[MAX_NAME_LEN];
static int current_pic_label_num;

char*
machopic_function_base_name ()
{
  static char *name = 0, *curr_name;
  static int base = 0;

  curr_name = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (current_function_decl));

  if (name != curr_name)
    {
      current_function_uses_pic_offset_table = 1;

      /* Save mucho space and time.  Some of the C++ mangled names are over
	 700 characters long!  Note that we produce a label containing a '-'
	 if the function we're compiling is an Objective-C method, as evinced
	 by the incredibly scientific test below.  This is because code in
	 rs6000.c makes the same ugly test when loading the PIC reg.  */
 
      ++current_pic_label_num;
      if (*curr_name == '+' || *curr_name == '-')
	sprintf (function_base, "*\"L-%d$pb\"", current_pic_label_num);
      else
	sprintf (function_base, "*L%d$pb", current_pic_label_num);

      name = curr_name;
    }

  return function_base;
}

static tree machopic_non_lazy_pointers = 0;

char* 
machopic_non_lazy_ptr_name (name)
     const char *name;
{
  tree temp, ident = get_identifier (name);
  
  for (temp = machopic_non_lazy_pointers;
       temp != NULL_TREE; 
       temp = TREE_CHAIN (temp))
    {
      if (ident == TREE_VALUE (temp))
	return IDENTIFIER_POINTER (TREE_PURPOSE (temp));
    }

  {
    char buffer[MAX_NAME_LEN];
    tree ptr_name;

    strcpy (buffer, "*L");
    if (name[0] == '*')
      strcat (buffer, name+1);
    else
      {
	strcat (buffer, "_");
	strcat (buffer, name);
      }
      
    strcat (buffer, "$non_lazy_ptr");
    ptr_name = get_identifier (buffer);

    machopic_non_lazy_pointers 
      = perm_tree_cons (ptr_name, ident, machopic_non_lazy_pointers);

    TREE_USED (machopic_non_lazy_pointers) = 0;
    return IDENTIFIER_POINTER (ptr_name);
  }
}


static tree machopic_stubs = 0;

static int
name_needs_quotes(name)
     const char *name;
{
  int c;
  while ((c = *name++) != '\0')
    {
      if (!isalnum(c) && c != '_')
        return 1;
    }
  return 0;
}

char* 
machopic_stub_name (name)
     const char *name;
{
  tree temp, ident = get_identifier (name);
  
  for (temp = machopic_stubs;
       temp != NULL_TREE; 
       temp = TREE_CHAIN (temp))
    {
      if (ident == TREE_VALUE (temp))
	return IDENTIFIER_POINTER (TREE_PURPOSE (temp));
    }

  {
    char buffer[MAX_NAME_LEN];
    tree ptr_name;
    int needs_quotes = name_needs_quotes(name);

    if (needs_quotes)
      strcpy (buffer, "*\"L");
    else
      strcpy (buffer, "*L");
    if (name[0] == '*')
      {
	strcat (buffer, name+1);
      }
    else
      {
	strcat (buffer, "_");
	strcat (buffer, name);
      }

    if (needs_quotes)
      strcat (buffer, "$stub\"");
    else
      strcat (buffer, "$stub");
    ptr_name = get_identifier (buffer);

    machopic_stubs = perm_tree_cons (ptr_name, ident, machopic_stubs);
    TREE_USED (machopic_stubs) = 0;

    return IDENTIFIER_POINTER (ptr_name);
  }
}

void
machopic_validate_stub_or_non_lazy_ptr (name, validate_stub)
  const char *name;
  int validate_stub;
{
    tree temp, ident = get_identifier (name);
    for (temp = validate_stub ? machopic_stubs : machopic_non_lazy_pointers;
         temp != NULL_TREE;
         temp = TREE_CHAIN (temp))
      if (ident == TREE_PURPOSE (temp))
	{
	  /* Mark both the stub or non-lazy pointer
	     as well as the original symbol as being referenced.  */
          TREE_USED (temp) = 1;
	  if (TREE_CODE (TREE_VALUE (temp)) == IDENTIFIER_NODE)
	    TREE_SYMBOL_REFERENCED (TREE_VALUE (temp)) = 1;
	}
}

/*
 *  Transform ORIG, which any data source to the corresponding
 *  source using indirections.  
 */

rtx
machopic_indirect_data_reference (orig, reg)
     rtx orig, reg;
{
  rtx ptr_ref = orig;
  
  if (! MACHOPIC_INDIRECT)
    return orig;

  if (GET_CODE (orig) == SYMBOL_REF)
    {
      const char *name = XSTR (orig, 0);

      if (machopic_data_defined_p (name))
	{
	  rtx pic_base = gen_rtx (SYMBOL_REF, Pmode, 
				   machopic_function_base_name ());
	  rtx offset = gen_rtx (CONST, Pmode,
				gen_rtx (MINUS, Pmode, orig, pic_base));

#if defined (HAVE_hi_sum) || defined (TARGET_TOC) /* i.e., PowerPC */
	  rtx hi_sum_reg =
#ifdef HI_SUM_TARGET_RTX /* apparently only defined for HP PA-RISC  */
	    reload_in_progress ? HI_SUM_TARGET_RTX : gen_reg_rtx (SImode);
#else
	    reg;
#endif

	  if (reg == 0) abort ();

	  emit_insn (gen_rtx (SET, Pmode, hi_sum_reg,
			      gen_rtx (PLUS, Pmode, pic_offset_table_rtx,
				       gen_rtx (HIGH, Pmode, offset))));
	  emit_insn (gen_rtx (SET, Pmode, reg,
			      gen_rtx (LO_SUM, Pmode, hi_sum_reg, offset)));
	  if (0)
	  {
	    rtx insn = get_last_insn ();
	    rtx note = find_reg_note (insn, REG_EQUAL, NULL_RTX);
	    
	    if (note)
	      XEXP (note, 0) = orig;
	    else
	      REG_NOTES (insn) = gen_rtx (EXPR_LIST, 
					  REG_EQUAL, orig, REG_NOTES (insn));
	  }

	  orig = reg;
#elif defined (HAVE_lo_sum)
	  if (reg == 0) abort ();

	  emit_insn (gen_rtx (SET, VOIDmode, reg,
			      gen_rtx (HIGH, Pmode, offset)));
	  emit_insn (gen_rtx (SET, VOIDmode, reg,
			      gen_rtx (LO_SUM, Pmode, reg, offset)));
	  emit_insn (gen_rtx (USE, VOIDmode,
			      gen_rtx_REG (Pmode, PIC_OFFSET_TABLE_REGNUM)));

	  orig = gen_rtx (PLUS, Pmode, pic_offset_table_rtx, reg);
#elif defined (MACHOPIC_M68K)
	  orig = gen_rtx (PLUS, Pmode, pic_offset_table_rtx, offset);
#endif  /*  MACHOPIC_M68K  */

	  return orig;
	}


      ptr_ref = gen_rtx (SYMBOL_REF, Pmode,
                         machopic_non_lazy_ptr_name (name));


      ptr_ref = gen_rtx_MEM (Pmode, ptr_ref);
      RTX_UNCHANGING_P (ptr_ref) = 1;

      return ptr_ref;
    }
  else if (GET_CODE (orig) == CONST)
    {
      rtx base, offset, result;

      /* legitimize both operands of the PLUS */
      if (GET_CODE (XEXP (orig, 0)) == PLUS)
	{
	  base = machopic_indirect_data_reference (XEXP (XEXP (orig, 0), 0), reg);
	  orig = machopic_indirect_data_reference (XEXP (XEXP (orig, 0), 1),
						   base == reg ? 0 : reg);
	}
      else 
	return orig;


      if (MACHOPIC_PURE && GET_CODE (orig) == CONST_INT)
        {
#ifdef INT_14_BITS
          if (INT_14_BITS (orig))
            {
#endif
              result = plus_constant_for_output (base, INTVAL (orig));
#ifdef INT_14_BITS
            }

          else if (!reload_in_progress)
            {
                orig = force_reg (Pmode, orig);
                result = gen_rtx (PLUS, Pmode, base, orig);
            }
          else
            {
              emit_insn (gen_rtx (SET, SImode, reg,
                                  gen_rtx (PLUS, Pmode,
                                           base, gen_rtx (HIGH, SImode, orig))));
              emit_insn (gen_rtx (SET, SImode, reg,
                                  gen_rtx (LO_SUM, SImode,
                                           reg, orig)));
              result = reg;
            }
#endif
        }
      else
        {
           result = gen_rtx (PLUS, Pmode, base, orig);
        }

      if (RTX_UNCHANGING_P (base) && RTX_UNCHANGING_P (orig))
	RTX_UNCHANGING_P (result) = 1;

      if (MACHOPIC_JUST_INDIRECT && GET_CODE (base) == MEM)
	{
	  if (reg)
	    {
	      emit_move_insn (reg, result);
	      result = reg;
	    }
	  else
	    {
	      result = force_reg (GET_MODE (result), result);
	    }
	}

      return result;

    }
  else if (GET_CODE (orig) == MEM)
    XEXP (ptr_ref, 0) = machopic_indirect_data_reference (XEXP (orig, 0), reg);
#ifndef MACHOPIC_M68K
  /* It's unknown why this code is not appropriate when the target is m68k.
     When the target is i386, this code prevents crashes due to the compiler's
     ignorance on how to move the PIC base register to other registers.
     (The reload phase sometimes introduces such insns.)  */
  else if (GET_CODE (orig) == PLUS
	   && GET_CODE (XEXP (orig, 0)) == REG
	   && REGNO (XEXP (orig, 0)) == PIC_OFFSET_TABLE_REGNUM
#ifdef I386
	   /* Prevent the same register from being erroneously used
	      as both the base and index registers.  */
	   && GET_CODE (XEXP (orig, 1)) == CONST
#endif
	   && reg)
    {
      emit_move_insn (reg, XEXP (orig, 0));
      XEXP (ptr_ref, 0) = reg;
    }
#endif
  return ptr_ref;
}

/* For MACHOPIC_INDIRECT_CALL_TARGET below, we need to beware of:

	extern "C" { int f(); }
	struct X { int f(); int g(); };
	int X::f() { ::f(); }
	int X::g() { ::f(); f();}

  This is hairy.  Both calls to "::f()" need to be indirect (i.e., to
  appropriate symbol stubs), but since MACHOPIC_NAME_DEFINED_P calls
  GET_IDENTIFIER which treats "f" as "X::f", and "X::f" is indeed (being)
  defined somewhere in "X"'s inheritance hierarchy, MACHOPIC_NAME_DEFINED_P
  returns TRUE when called with "f", which means that
  MACHOPIC_INDIRECT_CALL_TARGET uses an "internal" call instead of an
  indirect one as it should.

  Our quick-n-dirty solution to this is to call the following
  FUNC_NAME_MAYBE_SCOPED routine which (only for C++) checks whether
  FNAME -- the name of the function which we're calling -- is NOT a
  mangled C++ name, AND if the current function being compiled is a
  method, and if so, use an "external" or "indirect" call. 

  Note that this function will be called ONLY when MACHOPIC_INDIRECT_TARGET_P
  has already indicated that the target is NOT indirect.

  This conservative solution will sometimes make indirect calls where
  it might have been possible to make direct ones.

  FUNC_NAME_MAYBE_SCOPED returns 1 to indicate a "C" name (not scoped),
  which in turns means we should create a stub for an indirect call.
  */

static
int
func_name_maybe_scoped (const char *const in_fname)
{
  static signed char is_cplusplus = -1;  /* why no global for this?  */
  if (is_cplusplus < 0)
    is_cplusplus = (strcmp (lang_identify (), "cplusplus") == 0);
  if (is_cplusplus)
    {
      /* If we have a method, then check whether the function we're trying to
         call is a "C" function.  If so, we should use an indirect call.

         It turns out to be hard to tell whether "we have a method", since
         static member functions have a TREE_CODE of FUNCTION_TYPE, as do
         namespace-level non-member functions.  So here, we always look for
         an extern-"C"-like name, and make stubs for them no matter the
         calling context.  This is temporary, and leaves nagging suspicion
	 that improvements should be possible here.  (I.e., I suspect that
         it can still sometimes make stubs where it needn't.)  */

      /* if (1 || TREE_CODE (TREE_TYPE (current_function_decl)) == METHOD_TYPE) */
	{
	  const char *fname = in_fname;

	  /* If fname is of the form "f__1X" or "f__Fv", it's C++.  */
	  while (*fname == '_') ++fname;	/* skip leading underscores  */
	  while (*fname != 0)
	    {
	      if (fname[0] == '_' && fname[1] == '_'
		  && (fname[2] == 'F' || (fname[2] >= '0' && fname[2] <= '9')))
		return 0;
	      ++fname;
	    }

	  /* Not a C++ mangled name: must be "C", in which case play safe.  */

	  /* UNLESS it's a synthesised static init/destructor function,
	     which is KNOWN to be locally defined.  Horrible hack, but
	     it's worth it.  */

	  if (! strncmp (in_fname,
			"__static_initialization_and_destruction_", 40))
	    return 0;

	  return 1;
	}
    }
  return 0;
}

/* 
 *  Transform TARGET (a MEM), which is a function call target, to the
 *  corresponding symbol_stub if nessecary.  Return the a new MEM.
 */

rtx
machopic_indirect_call_target (target)
     rtx target;
{
  if (GET_CODE (target) != MEM)
    return target;
  if (MACHOPIC_INDIRECT && GET_CODE (XEXP (target, 0)) == SYMBOL_REF)
    { 
      enum machine_mode mode = GET_MODE (XEXP (target, 0));
      const char *name = XSTR (XEXP (target, 0), 0);
      if (!machopic_name_defined_p (name) || func_name_maybe_scoped (name)) 
	{
	  if (flag_dave_indirect) 
	    {
	      XEXP (target, 0) = force_reg (Pmode, XEXP (target, 0));
	    }
	  else /* kevin_indirect */
	    {
	      const char *stub_name = machopic_stub_name (name);
	      XEXP (target, 0) = gen_rtx (SYMBOL_REF, mode, stub_name);
	      RTX_UNCHANGING_P (target) = 1;
	    }
	} 
    }
  return target;
}


rtx
machopic_legitimize_pic_address (orig, mode, reg)
     rtx orig, reg;
     enum machine_mode mode;
{
  rtx pic_ref = orig;

  if (! MACHOPIC_PURE)
    return orig;

  /* First handle a simple SYMBOL_REF or LABEL_REF */
  if (GET_CODE (orig) == LABEL_REF
      || (GET_CODE (orig) == SYMBOL_REF
	  ))
    {
      /* addr(foo) = &func+(foo-func) */
      rtx equiv = orig;
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

      pic_base = gen_rtx (SYMBOL_REF, Pmode, 
			  machopic_function_base_name ());

      if (GET_CODE (orig) == MEM)
	{
	  if (reg == 0)
	    if (reload_in_progress)
	      abort ();
	    else
	      reg = gen_reg_rtx (Pmode);
	
#ifdef HAVE_lo_sum
	  if (GET_CODE (XEXP (orig, 0)) == SYMBOL_REF 
	      || GET_CODE (XEXP (orig, 0)) == LABEL_REF)
	    {
	      rtx offset = gen_rtx (CONST, Pmode,
				    gen_rtx (MINUS, Pmode,
					     XEXP (orig, 0), pic_base));
#if defined (HAVE_hi_sum) || defined (TARGET_TOC) /* i.e., PowerPC */
              rtx hi_sum_reg = reload_in_progress ?
#ifdef HI_SUM_TARGET_RTX /* apparently only defined for HP PA-RISC  */
		HI_SUM_TARGET_RTX :
#else
		reg :
#endif
		  /* Generating a new reg may expose opportunities for common
		     subexpression elimination.  */
		gen_reg_rtx (SImode);

	      emit_insn (gen_rtx (SET, Pmode, hi_sum_reg,
				  gen_rtx (PLUS, Pmode,
					   pic_offset_table_rtx,
					   gen_rtx (HIGH, Pmode, offset))));
	      emit_insn (gen_rtx (SET, VOIDmode, reg,
				  gen_rtx (MEM, GET_MODE (orig),
					   gen_rtx (LO_SUM, Pmode, 
						    hi_sum_reg, offset))));
	      pic_ref = reg;

#else  /* !HAVE_hi_sum  */
	      emit_insn (gen_rtx (USE, VOIDmode,
			      gen_rtx_REG (Pmode, PIC_OFFSET_TABLE_REGNUM)));

	      emit_insn (gen_rtx (SET, VOIDmode, reg,
				  gen_rtx (HIGH, Pmode, 
					   gen_rtx (CONST, Pmode, offset))));
	      emit_insn (gen_rtx (SET, VOIDmode, reg,
				  gen_rtx (LO_SUM, Pmode, reg, 
					   gen_rtx (CONST, Pmode, offset))));
	      pic_ref = gen_rtx (PLUS, Pmode,
				 pic_offset_table_rtx, reg);
#endif  /* else  (!HAVE_hi_sum) */
	    }
	  else
#endif  /* HAVE_lo_sum */
#ifndef PIC_OFFSET_TABLE_RTX
#define PIC_OFFSET_TABLE_RTX pic_offset_table_rtx
#endif
	    {
                rtx pic = PIC_OFFSET_TABLE_RTX;
                if (GET_CODE (pic) != REG)
                  {
                    emit_move_insn (reg, pic);
                    pic = reg;
                  }
#if 0
	      emit_insn (gen_rtx (USE, VOIDmode,
				  gen_rtx (REG, Pmode, PIC_OFFSET_TABLE_REGNUM)));
#endif


	      pic_ref = gen_rtx (PLUS, Pmode,
				 pic, 
				 gen_rtx (CONST, Pmode, 
					  gen_rtx (MINUS, Pmode,
						   XEXP (orig, 0), 
						   pic_base)));
	    }
	  
#if !defined (HAVE_hi_sum) && !defined (TARGET_TOC)
	  RTX_UNCHANGING_P (pic_ref) = 1;
	  emit_move_insn (reg, pic_ref);
	  pic_ref = gen_rtx (MEM, GET_MODE (orig), reg);
#endif
	}
      else
	{

#ifdef HAVE_lo_sum
	  if (GET_CODE (orig) == SYMBOL_REF 
	      || GET_CODE (orig) == LABEL_REF)
	    {
	      rtx offset = gen_rtx (CONST, Pmode,
				    gen_rtx (MINUS, Pmode, orig, pic_base));
#if defined (HAVE_hi_sum) || defined (TARGET_TOC) /* i.e., PowerPC */
              rtx hi_sum_reg;

	      if (reg == 0)
		if (reload_in_progress)
		  abort ();
		else
		 reg = gen_reg_rtx (SImode);
	
	      hi_sum_reg =
#ifdef HI_SUM_TARGET_RTX /* apparently only defined for HP PA-RISC  */
		reload_in_progress ? HI_SUM_TARGET_RTX : gen_reg_rtx (SImode);
#else
		reg;
#endif

	      emit_insn (gen_rtx (SET, Pmode, hi_sum_reg,
				  gen_rtx (PLUS, Pmode,
					   pic_offset_table_rtx,
					   gen_rtx (HIGH, Pmode, offset))));
	      emit_insn (gen_rtx (SET, VOIDmode, reg,
				  gen_rtx (LO_SUM, Pmode,
					   hi_sum_reg, offset)));
	      pic_ref = reg;
#else  /*  !HAVE_hi_sum  */
	      emit_insn (gen_rtx (SET, VOIDmode, reg,
				  gen_rtx (HIGH, Pmode, offset)));
	      emit_insn (gen_rtx (SET, VOIDmode, reg,
				  gen_rtx (LO_SUM, Pmode, reg, offset)));
	      pic_ref = gen_rtx (PLUS, Pmode,
				 pic_offset_table_rtx, reg);
#endif  /*  else (!HAVE_hi_sum)  */

	    }
	  else
#endif  /*  HAVE_lo_sum  */
	    if (GET_CODE (orig) == REG)
	      {
		return orig;
	      }
	    else
	      {
                  rtx pic = PIC_OFFSET_TABLE_RTX;
                  if (GET_CODE (pic) != REG)
                    {
                      emit_move_insn (reg, pic);
                      pic = reg;
                    }
#if 0
			emit_insn (gen_rtx (USE, VOIDmode,
				    pic_offset_table_rtx));
#endif
		
			pic_ref = gen_rtx (PLUS, Pmode,
				   pic, 
				   gen_rtx (CONST, Pmode, 
					    gen_rtx (MINUS, Pmode,
						     orig, pic_base)));
	      }
	}

      RTX_UNCHANGING_P (pic_ref) = 1;

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
      rtx base, offset;
      int is_complex;

      is_complex = (GET_CODE (XEXP (orig, 0)) == MEM);

      base = machopic_legitimize_pic_address (XEXP (orig, 0), Pmode, reg);
      orig = machopic_legitimize_pic_address (XEXP (orig, 1),
					      Pmode, base == reg ? 0 : reg);
      if (GET_CODE (orig) == CONST_INT)
	{
#ifdef INT_14_BITS
          if (INT_14_BITS (orig))
#endif
            {
              pic_ref = plus_constant_for_output (base, INTVAL (orig));
              is_complex = 1;
            }
#ifdef INT_14_BITS
          else if (!reload_in_progress)
            {
                orig = force_reg (Pmode, orig);
                pic_ref = gen_rtx (PLUS, Pmode, base, orig);
            }
          else
            {
              emit_insn (gen_rtx (SET, SImode, reg,
                                  gen_rtx (PLUS, Pmode,
                                           base, gen_rtx (HIGH, SImode, orig))));
              emit_insn (gen_rtx (SET, SImode, reg,
                                  gen_rtx (LO_SUM, SImode,
                                           reg, orig)));
              pic_ref = reg;
            }
#endif
	}
      else
	{
	  pic_ref = gen_rtx (PLUS, Pmode, base, orig);
	}

      if (RTX_UNCHANGING_P (base) && RTX_UNCHANGING_P (orig))
	RTX_UNCHANGING_P (pic_ref) = 1;

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
      rtx addr = machopic_legitimize_pic_address (XEXP (orig, 0), Pmode, reg);

      addr = gen_rtx (MEM, GET_MODE (orig), addr);
      RTX_UNCHANGING_P (addr) = RTX_UNCHANGING_P (orig);
      emit_move_insn (reg, addr);
      pic_ref = reg;
    }

  return pic_ref;
}


void
machopic_finish (asm_out_file)
     FILE *asm_out_file;
{
  tree temp;

  for (temp = machopic_stubs;
       temp != NULL_TREE; 
       temp = TREE_CHAIN (temp))
    {
      char symb[MAX_NAME_LEN];
      char stub[MAX_NAME_LEN];
      char *symb_name = IDENTIFIER_POINTER (TREE_VALUE (temp));
      char *stub_name = IDENTIFIER_POINTER (TREE_PURPOSE (temp));

      tree decl  = lookup_name (TREE_VALUE (temp), 0);

      if (! TREE_USED (temp))
          continue;

      /* don't emit stubs for static inline functions which has not been compiled. */
      if (decl
          && TREE_CODE (decl) == FUNCTION_DECL
          && DECL_INLINE (decl)
          && ! TREE_PUBLIC (decl)
          && ! TREE_ASM_WRITTEN (decl))
          continue;
  
      if (symb_name[0] == '*')
	strcpy (symb, symb_name+1);
      else if (symb_name[0] == '-' || symb_name[0] == '+')
	strcpy (symb, symb_name);	  
      else
	symb[0] = '_', strcpy (symb+1, symb_name);

      if (stub_name[0] == '*')
	strcpy (stub, stub_name+1);
      else
	stub[0] = '_', strcpy (stub+1, stub_name);

      /* must be in aux-out.c */
      machopic_output_stub (asm_out_file, symb, stub);
	  
    }

#if defined (I386) || defined (TARGET_TOC) /* i.e., PowerPC */
  {
    if (flag_pic && mcount_called)
      machopic_output_stub (asm_out_file, "mcount", "Lmcount$stub");
  }
#endif

  for (temp = machopic_non_lazy_pointers;
       temp != NULL_TREE; 
       temp = TREE_CHAIN (temp))
    {
      char *symb_name = IDENTIFIER_POINTER (TREE_VALUE (temp));
      char *lazy_name = IDENTIFIER_POINTER (TREE_PURPOSE (temp));

      tree decl  = lookup_name (TREE_VALUE (temp), 0);

      if (! TREE_USED (temp))
          continue;

      if (machopic_ident_defined_p (TREE_VALUE (temp))
          || (decl && DECL_PRIVATE_EXTERN (decl)))
	{
	  char symb[MAX_NAME_LEN];
	  
	  if (symb_name[0] == '*')
	    strcpy (symb, symb_name+1);
	  else
	    strcpy (symb, symb_name);

	  data_section ();
	  assemble_align (UNITS_PER_WORD * BITS_PER_UNIT);
	  assemble_label (lazy_name);
	  assemble_integer (gen_rtx (SYMBOL_REF, Pmode, symb_name),
			    GET_MODE_SIZE (Pmode), 1);
	}
      else
	{
	  machopic_nl_symbol_ptr_section ();
	  assemble_name (asm_out_file, lazy_name); 
	  fprintf (asm_out_file, ":\n");

	  fprintf (asm_out_file, "\t.indirect_symbol ");
	  assemble_name (asm_out_file, symb_name); 
	  fprintf (asm_out_file, "\n");

	  assemble_integer (const0_rtx, GET_MODE_SIZE (Pmode), 1);
	}
    }
}

int 
machopic_operand_p (op)
     rtx op;
{
  if (MACHOPIC_JUST_INDIRECT)
    {
      while (GET_CODE (op) == CONST)
	op = XEXP (op, 0);

      if (GET_CODE (op) == SYMBOL_REF)
	return machopic_name_defined_p (XSTR (op, 0));
      else
	return 0;
    }

  while (GET_CODE (op) == CONST)
    op = XEXP (op, 0);

  if (GET_CODE (op) == MINUS
	   && GET_CODE (XEXP (op, 0)) == SYMBOL_REF
	   && GET_CODE (XEXP (op, 1)) == SYMBOL_REF
	   && machopic_name_defined_p (XSTR (XEXP (op, 0), 0))
	   && machopic_name_defined_p (XSTR (XEXP (op, 1), 0)))
    {
      return 1;
    }

#ifdef TARGET_TOC /* i.e., PowerPC */
  /* Without this statement, the compiler crashes while compiling enquire.c
     when targetting PowerPC.  It is not known why this code is not needed
     when targetting other processors.  */
  else if (GET_CODE (op) == SYMBOL_REF
	   && (machopic_classify_name (XSTR (op, 0))
	       == MACHOPIC_DEFINED_FUNCTION))
    {
      return 1;
    }
#endif

  return 0;
}


/* Is DECL an RTTI variable?
   Please excuse the horrible method for determining whether DECL is
   an RTTI symbol.  Mea maxima culpa!  */

int rtti_var_p (tree decl)
{
  if (TREE_CODE (decl) == VAR_DECL && DECL_ARTIFICIAL (decl) && DECL_RTL (decl))
    {
      const char *name = XSTR (XEXP (DECL_RTL (decl), 0), 0);

      if (!strncmp (name, "__ti", 4))
	return 1;
    }
  return 0;
}

/* Is DECL an RTTI "systemlib" variable (ie, in libcc_dynamic.a?)  This
   list includes exception, bad_exception, bad_alloc, bad_typeid, bad_cast,
   type_info, ...
   And probably others.  */

int is_systemlib_rtti_p (tree decl)
{
  if (TREE_CODE (decl) == VAR_DECL || TREE_CODE (decl) == FUNCTION_DECL)
    {
      const char *name = IDENTIFIER_POINTER (DECL_NAME (decl));
      static const char libnames[][22] = {
	"13bad_exception",	"9exception",		"9bad_alloc", 
	"10bad_typeid",		"14__si_type_info",	"16__user_type_info",
	"17__class_type_info",	"8bad_cast",		"9type_info",
	"16__attr_type_info",	"16__func_type_info",	"16__ptmd_type_info",
	"16__ptmf_type_info",	"17__array_type_info",
	"19__builtin_type_info", "19__pointer_type_info"
      };
      static const char s_libnames[][3] = {	/* "small" libnames */
	"Sc", "Uc", "Ui", "Ul", "Us", "Ux",
	"b", "c", "d", "f", "i", "l",
	"r", "s", "v", "w", "x"
      };
      int ix;

      /* These names must begin with '__tf' or '__ti'.  */
      if (name[0] != '_' || name[1] != '_' || name[2] != 't')
	return 0;
      if (name[3] != 'f' && name[3] != 'i')
	return 0;
      name += 4;

      for (ix = 0; ix < sizeof (libnames) / sizeof (libnames[0]); ++ix)
	if (*name == libnames[ix][0] && !strcmp (name+1, libnames[ix]+1))
	  return 1;

      for (ix = 0; ix < sizeof (s_libnames) / sizeof (s_libnames[0]); ++ix)
	if (*name == s_libnames[ix][0] && !strcmp (name+1, s_libnames[ix]+1))
	  return 1;
    }

  return 0;
}

#endif   /*  MACHO_PIC (Covers this entire source file)  */
