/* Target definitions for x86 running Darwin.
   Copyright (C) 2001, 2002, 2004, 2005 Free Software Foundation, Inc.
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
      builtin_define ("__LITTLE_ENDIAN__");     \
      /* APPLE LOCAL remove __MACH__ and __APPLE__, defined in gcc/config/darwin.h */\
      /* APPLE LOCAL constant cfstrings */	\
      SUBTARGET_OS_CPP_BUILTINS ();		\
    }                                           \
  while (0)

/* We want -fPIC by default, unless we're using -static to compile for
   the kernel or some such.  */

#undef CC1_SPEC
/* APPLE LOCAL dynamic-no-pic */
/* APPLE LOCAL ignore -mcpu=G4 -mcpu=G5 */
/* When -mdynamic-no-pic finally works, remove the "xx" below.  FIXME!!  */
#define CC1_SPEC "\
%{g: %{!fno-eliminate-unused-debug-symbols: -feliminate-unused-debug-symbols }} \
%{!static:%{!mxxdynamic-no-pic:-fPIC}} %<faltivec %<mno-fused-madd %<mlong-branch %<mlongcall %<mcpu=G4 %<mcpu=G5"


/* APPLE LOCAL AltiVec */
#define CPP_ALTIVEC_SPEC "%<faltivec"

#undef ASM_SPEC
#define ASM_SPEC "-arch i386 -force_cpusubtype_ALL"

#undef SUBTARGET_EXTRA_SPECS
#define SUBTARGET_EXTRA_SPECS					\
  { "darwin_arch", "i386" },					\
  { "darwin_subarch", "i386" },

/* Use the following macro for any Darwin/x86-specific command-line option
   translation.  */
#define SUBTARGET_OPTION_TRANSLATE_TABLE \
  { "", "" }

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

/* APPLE LOCAL long double default size --mrs */
#define TARGET_SUBTARGET_DEFAULT (MASK_80387 | MASK_IEEE_FP | MASK_FLOAT_RETURNS | MASK_128BIT_LONG_DOUBLE)

/* TARGET_DEEP_BRANCH_PREDICTION is incompatible with Mach-O PIC.  */

#undef TARGET_DEEP_BRANCH_PREDICTION
#define TARGET_DEEP_BRANCH_PREDICTION   0

/* For now, disable dynamic-no-pic.  We'll need to go through i386.c
   with a fine-tooth comb looking for refs to flag_pic!  */
#define MASK_MACHO_DYNAMIC_NO_PIC 0
#define TARGET_DYNAMIC_NO_PIC	  (target_flags & MASK_MACHO_DYNAMIC_NO_PIC)

/* Define the syntax of pseudo-ops, labels and comments.  */

#define LPREFIX "L"

/* These are used by -fbranch-probabilities */
#define HOT_TEXT_SECTION_NAME "__TEXT,__text,regular,pure_instructions"
#define UNLIKELY_EXECUTED_TEXT_SECTION_NAME \
                              "__TEXT,__unlikely,regular,pure_instructions"

/* Assembler pseudos to introduce constants of various size.  */

#define ASM_BYTE_OP "\t.byte\t"
#define ASM_SHORT "\t.word\t"
#define ASM_LONG "\t.long\t"
/* Darwin as doesn't do ".quad".  */

#undef ASM_OUTPUT_ALIGN
#define ASM_OUTPUT_ALIGN(FILE,LOG)	\
 do { if ((LOG) != 0)			\
        {				\
          if (in_text_section ())	\
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
  fprintf ((FILE), ",%lu\n", (unsigned long)(ROUNDED)))

/* This says how to output an assembler line
   to define a local common symbol.  */

#define ASM_OUTPUT_LOCAL(FILE, NAME, SIZE, ROUNDED)  \
( fputs (".lcomm ", (FILE)),			\
  assemble_name ((FILE), (NAME)),		\
  fprintf ((FILE), ","HOST_WIDE_INT_PRINT_UNSIGNED"\n", (ROUNDED)))


/* APPLE LOCAL begin Macintosh alignment 2002-2-19 --ff */
#define MASK_ALIGN_NATURAL	0x40000000
#define TARGET_ALIGN_NATURAL	(target_flags & MASK_ALIGN_NATURAL)
#define rs6000_alignment_flags target_flags
#define MASK_ALIGN_MAC68K	0x20000000
#define TARGET_ALIGN_MAC68K	(target_flags & MASK_ALIGN_MAC68K)

#define REGISTER_TARGET_PRAGMAS DARWIN_REGISTER_TARGET_PRAGMAS

#define ROUND_TYPE_ALIGN(TYPE, COMPUTED, SPECIFIED) \
  (((TREE_CODE (TYPE) == RECORD_TYPE \
     || TREE_CODE (TYPE) == UNION_TYPE \
     || TREE_CODE (TYPE) == QUAL_UNION_TYPE) \
    && TARGET_ALIGN_MAC68K \
    && MAX (COMPUTED, SPECIFIED) == 8) ? 16 \
    : MAX (COMPUTED, SPECIFIED))

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
/* APPLE LOCAL end Macintosh alignment 2002-2-19 --ff */

/* Darwin profiling -- call mcount.  */
#undef FUNCTION_PROFILER
#define FUNCTION_PROFILER(FILE, LABELNO)				\
    do {								\
      if (MACHOPIC_INDIRECT)						\
	{								\
	  const char *name = machopic_mcount_stub_name ();		\
	  fprintf (FILE, "\tcall %s\n", name+1);  /*  skip '&'  */	\
	  machopic_validate_stub_or_non_lazy_ptr (name);		\
	}								\
      else fprintf (FILE, "\tcall mcount\n");				\
    } while (0)

/* APPLE LOCAL SSE stack alignment */
#define BASIC_STACK_BOUNDARY (128)

#undef SUBTARGET_OVERRIDE_OPTIONS
/* Force Darwin/x86 to default as "-march=i686 -mcpu=pentium4".  */
#define SUBTARGET_OVERRIDE_OPTIONS \
  do { \
    if (!ix86_arch_string) ix86_arch_string = "pentiumpro"; \
    if (!ix86_tune_string) ix86_tune_string = "pentium4"; \
  } while (0)
