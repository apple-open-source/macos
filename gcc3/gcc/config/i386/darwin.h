/* APPLE LOCAL file darwin x86 */
/* Target definitions for x86 running Darwin.
   Copyright (C) 1997, 2000, 2001 Free Software Foundation, Inc.
   Contributed by Apple Computer Inc.

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
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* Enable Mach-O bits in generic x86 code.  */
#undef TARGET_MACHO
#define TARGET_MACHO 1

#define TARGET_VERSION fprintf (stderr, " (x86, BSD syntax)");

#define CPP_PREDEFINES "-D__i386__ -D__MACH__ -D__LITTLE_ENDIAN__ -D__APPLE__"

/* We want -fPIC by default, unless we're using -static to compile for
   the kernel or some such. 
   When -mdynamic-no-pic finally works, remove the "xx" below.  FIXME!!  */

#undef CC1_SPEC
#define CC1_SPEC "%{!static:%{!mxxdynamic-no-pic:-fPIC}} %{!<faltivec}"

#define CPP_ALTIVEC_SPEC "%{!<faltivec}"

#undef DEFAULT_SIGNED_CHAR
#define DEFAULT_SIGNED_CHAR (1)

/* A value other than 0 or 1 might be a better idea.  */
#undef ASSEMBLER_DIALECT
#define ASSEMBLER_DIALECT 0

/* MacOS X 'as' recognises filds/fists/fistps.  */
#undef HAVE_GAS_FILDS_FISTS
#define HAVE_GAS_FILDS_FISTS 1

/* Define macro used to output shift-double opcodes when the shift
   count is in %cl.  Some assemblers require %cl as an argument;
   some don't.  This macro controls what to do: by default, don't
   print %cl.  */

#define SHIFT_DOUBLE_OMITS_COUNT 0

/* Define the syntax of pseudo-ops, labels and comments.  */

/* String containing the assembler's comment-starter.  */

#define ASM_COMMENT_START "#"

/* We don't do APP_ON/APP_OFF.  */

#undef ASM_APP_ON
#define ASM_APP_ON ""
#undef ASM_APP_OFF
#define ASM_APP_OFF ""

/* By default, target has a 80387, uses IEEE compatible arithmetic,
   and returns float values in the 387.  */

#define TARGET_SUBTARGET_DEFAULT (MASK_80387 | MASK_IEEE_FP | MASK_FLOAT_RETURNS)

/* Floating-point return values come in the FP register.  */

#define VALUE_REGNO(MODE) \
  (GET_MODE_CLASS (MODE) == MODE_FLOAT				\
   && TARGET_FLOAT_RETURNS_IN_80387 ? FIRST_FLOAT_REG		\
   : (MODE) == TImode || VECTOR_MODE_P (MODE) ? FIRST_SSE_REG	\
   : 0)

#if 0 /* omit until we're ready to consider it */
/* Output code to add DELTA to the first argument, and then jump to FUNCTION.
   Used for C++ multiple inheritance.  */
#define ASM_OUTPUT_MI_THUNK(FILE, THUNK_FNDECL, DELTA, FUNCTION)	    \
do {									    \
  tree parm;								    \
  rtx xops[3];								    \
									    \
  if (ix86_regparm > 0)							    \
    parm = TYPE_ARG_TYPES (TREE_TYPE (function));			    \
  else									    \
    parm = NULL_TREE;							    \
  for (; parm; parm = TREE_CHAIN (parm))				    \
    if (TREE_VALUE (parm) == void_type_node)				    \
      break;								    \
									    \
  xops[0] = GEN_INT (DELTA);						    \
  if (parm)								    \
    xops[1] = gen_rtx_REG (SImode, 0);					    \
  else if (aggregate_value_p (TREE_TYPE (TREE_TYPE (FUNCTION))))	    \
    xops[1] = gen_rtx_MEM (SImode, plus_constant (stack_pointer_rtx, 8));   \
  else									    \
    xops[1] = gen_rtx_MEM (SImode, plus_constant (stack_pointer_rtx, 4));   \
  output_asm_insn ("add{l} {%0, %1|%1, %0}", xops);			    \
									    \
  if (flag_pic && !TARGET_64BIT)					    \
    {									    \
      xops[0] = pic_offset_table_rtx;					    \
      xops[1] = gen_label_rtx ();					    \
      xops[2] = gen_rtx_SYMBOL_REF (Pmode, "_GLOBAL_OFFSET_TABLE_");        \
									    \
      if (ix86_regparm > 2)						    \
	abort ();							    \
      output_asm_insn ("push{l}\t%0", xops);				    \
      output_asm_insn ("call\t%P1", xops);				    \
      ASM_OUTPUT_INTERNAL_LABEL (FILE, "L", CODE_LABEL_NUMBER (xops[1]));   \
      output_asm_insn ("pop{l}\t%0", xops);				    \
      output_asm_insn ("add{l}\t{%2+[.-%P1], %0|%0, OFFSET FLAT: %2+[.-%P1]}", xops); \
      xops[0] = gen_rtx_MEM (SImode, XEXP (DECL_RTL (FUNCTION), 0));	    \
      output_asm_insn ("mov{l}\t{%0@GOT(%%ebx), %%ecx|%%ecx, %0@GOT[%%ebx]}",\
	               xops);						    \
      asm_fprintf (FILE, "\tpop{l\t%%ebx|\t%%ebx}\n");			    \
      asm_fprintf (FILE, "\tjmp\t{*%%ecx|%%ecx}\n");			    \
    }									    \
  else if (flag_pic && TARGET_64BIT)					    \
    {									    \
      fprintf (FILE, "\tjmp *");					    \
      assemble_name (FILE, XSTR (XEXP (DECL_RTL (FUNCTION), 0), 0));	    \
      fprintf (FILE, "@GOTPCREL(%%rip)\n");				    \
    }									    \
  else									    \
    {									    \
      fprintf (FILE, "\tjmp ");						    \
      assemble_name (FILE, XSTR (XEXP (DECL_RTL (FUNCTION), 0), 0));	    \
      fprintf (FILE, "\n");						    \
    }									    \
} while (0)
#endif

/* Define the syntax of pseudo-ops, labels and comments.  */

#define LPREFIX "L"

/* Assembler pseudos to introduce constants of various size.  */

#define ASM_BYTE_OP "\t.byte\t"
#define ASM_SHORT "\t.word\t"
#define ASM_LONG "\t.long\t"
/* darwin as doesn't do ".quad".  */
/* #define ASM_QUAD "\t.quad\t"  */

#undef ASM_OUTPUT_ALIGN
#define ASM_OUTPUT_ALIGN(FILE,LOG)	\
 do { if ((LOG) != 0)			\
      if (in_text_section ()		\
          || darwin_named_section_is ("__TEXT,__textcoal,coalesced") \
          || darwin_named_section_is ("__TEXT,__textcoal_nt,coalesced,no_toc") \
          || darwin_named_section_is (STATIC_INIT_SECTION)) \
	fprintf (FILE, "\t%s %d,0x90\n", ALIGN_ASM_OP, (LOG)); \
      else \
	fprintf (FILE, "\t%s %d\n", ALIGN_ASM_OP, (LOG)); \
    } while (0)

/* This says how to output an assembler line
   to define a global common symbol.  */

#define ASM_OUTPUT_COMMON(FILE, NAME, SIZE, ROUNDED)  \
( fputs (".comm ", (FILE)),			\
  assemble_name ((FILE), (NAME)),		\
  fprintf ((FILE), ",%u\n", (ROUNDED)))

/* This says how to output an assembler line
   to define a local common symbol.  */

#define ASM_OUTPUT_LOCAL(FILE, NAME, SIZE, ROUNDED)  \
( fputs (".lcomm ", (FILE)),			\
  assemble_name ((FILE), (NAME)),		\
  fprintf ((FILE), ",%u\n", (ROUNDED)))

/* begin Macintosh alignment 2001-08-15 ff */
#if 0
/* This now supports the Macintosh power alignment mode.  */
#undef ADJUST_FIELD_ALIGN
#define ADJUST_FIELD_ALIGN(FIELD, COMPUTED)		\
  (((COMPUTED) == RS6000_VECTOR_ALIGNMENT)		\
   ? RS6000_VECTOR_ALIGNMENT				\
   : (MIN ((COMPUTED), 32)))

#undef ROUND_TYPE_ALIGN
/* Macintosh alignment modes require more complicated handling
   of alignment, so we replace the macro with a call to a
   out-of-line function.  */
union tree_node;
extern unsigned round_type_align (union tree_node*, unsigned, unsigned); /* rs6000.c  */
#define ROUND_TYPE_ALIGN(STRUCT, COMPUTED, SPECIFIED)	\
  round_type_align(STRUCT, COMPUTED, SPECIFIED)
#endif
/* end Macintosh alignment 2001-08-15 ff */


/* We've run out of bits in 'target_flags'.  So we re-use some bits that
   will never be used on Darwin.
   NO_RED_ZONE only applies to 64-bit x86 stuff (Hammer).
   We don't do stack probing, and we never use Intel syntax.
   Note that TARGET_DEEP_BRANCH_PREDICTION is incompatible with MACH-O PIC.  */

enum
{
	old_x86_MASK_STACK_PROBE = MASK_STACK_PROBE,
	old_x86_MASK_INTEL_SYNTAX = MASK_INTEL_SYNTAX,
	old_x86_MASK_NO_RED_ZONE = MASK_NO_RED_ZONE
};
#undef MASK_NO_RED_ZONE
#define MASK_NO_RED_ZONE 0
#undef MASK_INTEL_SYNTAX
#define MASK_INTEL_SYNTAX 0
#undef MASK_STACK_PROBE
#define MASK_STACK_PROBE 0
#undef MASK_DEBUG_ARG
#define MASK_DEBUG_ARG 0

#undef TARGET_DEEP_BRANCH_PREDICTION
#define TARGET_DEEP_BRANCH_PREDICTION	0

/* Get HOST_WIDE_INT and CONST_INT to be 32 bits, for compile time
   space/speed.  */
#undef MAX_LONG_TYPE_SIZE
#define MAX_LONG_TYPE_SIZE 32

/* Macros related to the switches that specify default alignment of fields
   within structs.  */

#define MASK_ALIGN_MAC68K       old_x86_MASK_STACK_PROBE
#define TARGET_ALIGN_MAC68K     (target_flags & MASK_ALIGN_MAC68K)

/* For now, disable dynamic-no-pic.  We'll need to go through i386.c
   with a fine-tooth comb looking for refs to flag_pic!  */
#define MASK_MACHO_DYNAMIC_NO_PIC 0
#define TARGET_DYNAMIC_NO_PIC	  (target_flags & MASK_MACHO_DYNAMIC_NO_PIC)

/* APPLE LOCAL begin Macintosh alignment 2002-2-19 ff */
#define MASK_ALIGN_NATURAL	old_x86_MASK_INTEL_SYNTAX
#define TARGET_ALIGN_NATURAL	(target_flags & MASK_ALIGN_NATURAL)

#undef SUBTARGET_SWITCHES
#define SUBTARGET_SWITCHES						\
  {"align-mac68k",      MASK_ALIGN_MAC68K,				\
	N_("Align structs and unions according to mac68k rules")},	\
  {"align-power",       - (MASK_ALIGN_MAC68K | MASK_ALIGN_NATURAL),	\
	N_("Align structs and unions according to PowerPC rules")},	\
  {"align-natural",     MASK_ALIGN_NATURAL,				\
	N_("Align structs and unions according to natural rules")},	\
  {"dynamic-no-pic",    MASK_MACHO_DYNAMIC_NO_PIC,			\
	N_("Generate code suitable for executables (NOT shared libs)")},\
  {"no-dynamic-no-pic", -MASK_MACHO_DYNAMIC_NO_PIC,  ""},
/* APPLE LOCAL end Macintosh alignment 2002-2-19 ff */

/* Make sure local alignments come from the type node, not the mode;
   mode-based alignments are wrong for vectors.  */
#undef LOCAL_ALIGNMENT
#define LOCAL_ALIGNMENT(TYPE, ALIGN)	(MAX (ALIGN, TYPE_ALIGN (TYPE)))

#if 0 /* probably bogus 2001-11-11 */
/* Darwin increases natural record alignment to doubleword if the first
   field is an FP double while the FP fields remain word aligned.  */
#define ROUND_TYPE_ALIGN(STRUCT, COMPUTED, SPECIFIED)	\
  ((TREE_CODE (STRUCT) == RECORD_TYPE			\
    || TREE_CODE (STRUCT) == UNION_TYPE			\
    || TREE_CODE (STRUCT) == QUAL_UNION_TYPE)		\
   && TYPE_FIELDS (STRUCT) != 0				\
   && DECL_MODE (TYPE_FIELDS (STRUCT)) == DFmode	\
   ? MAX (MAX ((COMPUTED), (SPECIFIED)), 64)		\
   : MAX ((COMPUTED), (SPECIFIED)))
#endif



/* XXX: Darwin supports neither .quad, or .llong, but it also doesn't
   support 64 bit powerpc either, so this just keeps things happy.  */
#define DOUBLE_INT_ASM_OP "\t.quad\t"

/* Darwin profiling -- call mcount.  */
#undef FUNCTION_PROFILER
#define FUNCTION_PROFILER(FILE, LABELNO)				\
    do {								\
      if (MACHOPIC_INDIRECT)						\
	{								\
	  const char *name = machopic_stub_name ("*mcount");		\
	  fprintf (FILE, "\tcall %s\n", name+1);  /*  skip '&'  */	\
	  machopic_validate_stub_or_non_lazy_ptr (name, /*stub:*/1);	\
	}								\
      else fprintf (FILE, "\tcall mcount\n");				\
    } while (0)

/* APPLE LOCAL PFE  */
#ifdef PFE
/* Macro to call darwin_pfe_freeze_thaw_target_additions().  If this macro is
   not defined then the target additions function is never called.  */
#define PFE_TARGET_ADDITIONS(hdr) darwin_pfe_freeze_thaw_target_additions (hdr)

/* Macro to call darwin_pfe_maybe_savestring to determine whether strings
   should be allocated by pfe_savestring() or not.  */
#define PFE_TARGET_MAYBE_SAVESTRING(s) darwin_pfe_maybe_savestring ((char *)(s))
#endif

#define USER_LABEL_PREFIX ""
