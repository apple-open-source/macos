/* Target definitions for x86 running Darwin.
   Copyright (C) 2001, 2002 Free Software Foundation, Inc.
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

/* APPLE LOCAL begin default to ppro */
/* Default to -mcpu=pentiumpro instead of i386 (radar 2730299)  ilr */
#undef TARGET_CPU_DEFAULT
#define TARGET_CPU_DEFAULT 4
/* APPLE LOCAL end default to ppro */

#define TARGET_VERSION fprintf (stderr, " (i386 Darwin)");

#define TARGET_OS_CPP_BUILTINS()                \
  do                                            \
    {                                           \
      builtin_define ("__i386__");              \
      builtin_define ("__i386");                \
      builtin_define ("__LITTLE_ENDIAN__");     \
      /* APPLE LOCAL XJR */                     \
      SUBTARGET_OS_CPP_BUILTINS ();             \    
    }                                           \
  while (0)

/* We want -fPIC by default, unless we're using -static to compile for
   the kernel or some such.  */

#undef CC1_SPEC
/* APPLE LOCAL dynamic-no-pic */
/* APPLE LOCAL ignore -mcpu=G4 -mcpu=G5 */
/* When -mdynamic-no-pic finally works, remove the "xx" below.  FIXME!!  */
#define CC1_SPEC "%{!static:%{!mxxdynamic-no-pic:-fPIC}} %{!<faltivec} %{!<mlong-branch} %{!<mlongcall} %{!<mcpu=G4} %{!<mcpu=G5}"

/* APPLE LOCAL asm flags */
/* APPLE LOCAL ignore PPC cpu names */
#define ASM_SPEC "-arch %T \
  %{Zforce_cpusubtype_ALL:-force_cpusubtype_ALL} \
  %{!Zforce_cpusubtype_ALL:%{mmmx:-force_cpusubtype_ALL}\
			   %{msse:-force_cpusubtype_ALL}\
			   %{msse2:-force_cpusubtype_ALL}}"

/* APPLE LOCAL change from FSF3.4 */
/* These are used by -fbranch-probabilities */
#define HOT_TEXT_SECTION_NAME "__TEXT,__text,regular,pure_instructions"
/* APPLE LOCAL begin - rarely executed bb optimization */
#define UNLIKELY_EXECUTED_TEXT_SECTION_NAME \
                              "__TEXT,__unexecuted,regular,pure_instructions"
#define SECTION_FORMAT_STRING ".section %s\n\t.align 2\n"
/* APPLE LOCAL end - rarely executed bb optimization */

/* APPLE LOCAL AltiVec */
#define CPP_ALTIVEC_SPEC "%{!<faltivec}"

/* The Darwin assembler mostly follows AT&T syntax.  */
#undef ASSEMBLER_DIALECT
#define ASSEMBLER_DIALECT ASM_ATT

/* Define macro used to output shift-double opcodes when the shift
   count is in %cl.  Some assemblers require %cl as an argument;
   some don't.  This macro controls what to do: by default, don't
   print %cl.  */

#define SHIFT_DOUBLE_OMITS_COUNT 0

/* Define the syntax of pseudo-ops, labels and comments.  */

/* String containing the assembler's comment-starter.  */

#define ASM_COMMENT_START "#"

/* By default, target has a 80387, uses IEEE compatible arithmetic,
   and returns float values in the 387.  */

#define TARGET_SUBTARGET_DEFAULT (MASK_80387 | MASK_IEEE_FP | MASK_FLOAT_RETURNS)

/* TARGET_DEEP_BRANCH_PREDICTION is incompatible with Mach-O PIC.  */

#undef TARGET_DEEP_BRANCH_PREDICTION
#define TARGET_DEEP_BRANCH_PREDICTION   0

/* Define the syntax of pseudo-ops, labels and comments.  */

#define LPREFIX "L"

/* Assembler pseudos to introduce constants of various size.  */

#define ASM_BYTE_OP "\t.byte\t"
#define ASM_SHORT "\t.word\t"
#define ASM_LONG "\t.long\t"
/* Darwin as doesn't do ".quad".  */

#undef ASM_OUTPUT_ALIGN
#define ASM_OUTPUT_ALIGN(FILE,LOG)	\
 do { if ((LOG) != 0)			\
        {				\
          /* APPLE LOCAL coalescing */  \
          if (in_text_section ()	\
              || darwin_named_section_is ("__TEXT,__textcoal,coalesced") \
              || darwin_named_section_is ("__TEXT,__textcoal_nt,coalesced,no_toc") \
              || darwin_named_section_is (STATIC_INIT_SECTION)) \
            fprintf (FILE, "\t%s %d,0x90\n", ALIGN_ASM_OP, (LOG)); \
          else				\
            fprintf (FILE, "\t%s %d\n", ALIGN_ASM_OP, (LOG)); \
        }				\
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

#undef SUBTARGET_OVERRIDE_OPTIONS
/* Force Darwin/x86 to default as "-march=i686 -mcpu=pentium4".  */
#define SUBTARGET_OVERRIDE_OPTIONS \
  do { \
  if (!ix86_arch_string) ix86_arch_string = "pentiumpro"; \
  if (!ix86_cpu_string) ix86_cpu_string = "pentium4"; \
  } while (0)

/* APPLE LOCAL SSE stack alignment */
#define BASIC_STACK_BOUNDARY (128)
