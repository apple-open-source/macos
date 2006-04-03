/* Target definitions for x86 running Darwin.
   APPLE LOCAL mainline 2005-04-11
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

#define TARGET_VERSION fprintf (stderr, " (i686 Darwin)");

/* APPLE LOCAL begin mainline 2005-04-11 4010614 */
#undef TARGET_FPMATH_DEFAULT
#define TARGET_FPMATH_DEFAULT (TARGET_SSE ? FPMATH_SSE : FPMATH_387)
/* APPLE LOCAL end mainline 2005-04-11 4010614 */

#define TARGET_OS_CPP_BUILTINS()                \
  do                                            \
    {                                           \
      builtin_define ("__i386__");              \
      builtin_define ("__LITTLE_ENDIAN__");     \
/* APPLE LOCAL mainline 2005-09-01 3449986 */	\
      darwin_cpp_builtins (pfile);		\
    }                                           \
  while (0)

/* We want -fPIC by default, unless we're using -static to compile for
   the kernel or some such.  */

#undef CC1_SPEC
/* APPLE LOCAL begin dynamic-no-pic */
#define CC1_SPEC "\
%{g: %{!fno-eliminate-unused-debug-symbols: -feliminate-unused-debug-symbols }} \
"/* APPLE LOCAL ignore -mcpu=G4 -mcpu=G5 */"\
%{!static:%{!mdynamic-no-pic:-fPIC}} %<faltivec %<mno-fused-madd %<mlong-branch %<mlongcall %<mcpu=G4 %<mcpu=G5"
/* APPLE LOCAL end dynamic-no-pic */

/* APPLE LOCAL AltiVec */
#define CPP_ALTIVEC_SPEC "%<faltivec"

#undef ASM_SPEC
/* APPLE LOCAL mainline 2005-04-11 */
#define ASM_SPEC "-arch i386 -force_cpusubtype_ALL"

#undef SUBTARGET_EXTRA_SPECS
#define SUBTARGET_EXTRA_SPECS					\
  /* APPLE LOCAL begin mainline 2005-04-11 */			\
  { "darwin_arch", "i386" },					\
  { "darwin_subarch", "i386" },
  /* APPLE LOCAL end mainline 2005-04-11 */

/* APPLE LOCAL begin 4078600 */
/* Support for configure-time defaults of some command line options.  */
#undef OPTION_DEFAULT_SPECS
#define OPTION_DEFAULT_SPECS \
  {"arch", "%{!march=*:-march=%(VALUE)}"}, \
  {"tune", "%{!mtune=*:-mtune=%(VALUE)}"}
/* APPLE LOCAL end 4078600 */

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

/* APPLE LOCAL begin deep branch prediction pic-base */
extern void darwin_x86_file_end (void);
#undef TARGET_ASM_FILE_END
#define TARGET_ASM_FILE_END darwin_x86_file_end
/* APPLE LOCAL end deep branch prediction pic-base */

/* Define the syntax of pseudo-ops, labels and comments.  */

/* String containing the assembler's comment-starter.  */

#define ASM_COMMENT_START "#"

/* By default, target has a 80387, uses IEEE compatible arithmetic,
   and returns float values in the 387.  */

/* APPLE LOCAL temporary not ALIGN_DOUBLE */
#define TARGET_SUBTARGET_DEFAULT (MASK_80387 | MASK_IEEE_FP | MASK_FLOAT_RETURNS | MASK_128BIT_LONG_DOUBLE | (0 & MASK_ALIGN_DOUBLE))

/* APPLE LOCAL begin dynamic-no-pic */
/* Darwin switches.  */
/* Use dynamic-no-pic codegen (no picbase reg; not suitable for shlibs.)  */
#define MASK_MACHO_DYNAMIC_NO_PIC (0x00800000)

#define TARGET_DYNAMIC_NO_PIC	(target_flags & MASK_MACHO_DYNAMIC_NO_PIC)
/* APPLE LOCAL end dynamic-no-pic */

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

/* APPLE LOCAL begin SSE stack alignment */
#define BASIC_STACK_BOUNDARY (128)
/* APPLE LOCAL end SSE stack alignment */

/* APPLE LOCAL CW asm blocks */
extern int flag_cw_asm_blocks;
/* APPLE LOCAL begin fix-and-continue x86 */
#undef SUBTARGET_OVERRIDE_OPTIONS
#define SUBTARGET_OVERRIDE_OPTIONS				\
  do {								\
    /* Handle -mfix-and-continue.  */				\
    if (darwin_fix_and_continue_switch)				\
      {								\
	const char *base = darwin_fix_and_continue_switch;	\
	while (base[-1] != 'm') base--;				\
								\
	if (*darwin_fix_and_continue_switch != '\0')		\
	  error ("invalid option %qs", base);			\
	darwin_fix_and_continue = (base[0] != 'n');		\
      }								\
    /* APPLE LOCAL begin AT&T-style stub 4164563 */		\
    /* Handle -matt-stubs.  */					\
    if (darwin_macho_att_stub_switch)				\
      {								\
	const char *base = darwin_macho_att_stub_switch;	\
	while (base[-1] != 'm') base--;				\
								\
	if (*darwin_macho_att_stub_switch != '\0')		\
	  error ("invalid option %qs", base);			\
	darwin_macho_att_stub = (base[0] != 'n');		\
      }								\
    /* APPLE LOCAL end AT&T-style stub 4164563 */		\
    /* APPLE LOCAL begin CW asm blocks */			\
    if (flag_cw_asm_blocks)					\
      flag_ms_asms = 1;						\
    /* APPLE LOCAL end CW asm blocks */				\
  } while (0)

/* True, iff we're generating fast turn around debugging code.  When
   true, we arrange for function prologues to start with 6 nops so
   that gdb may insert code to redirect them, and for data to be
   accessed indirectly.  The runtime uses this indirection to forward
   references for data to the original instance of that data.  */

#define TARGET_FIX_AND_CONTINUE (darwin_fix_and_continue)
/* APPLE LOCAL end fix-and-continue x86 */

/* APPLE LOCAL begin CW asm blocks */
#define CW_ASM_REGISTER_NAME(STR, BUF) i386_cw_asm_register_name (STR, BUF)
/* APPLE LOCAL end CW asm blocks */
