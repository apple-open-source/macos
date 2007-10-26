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

/* APPLE LOCAL begin mainline */
#undef  TARGET_64BIT
#define TARGET_64BIT (target_flags & MASK_64BIT)

#define TARGET_VERSION fprintf (stderr, " (i686 Darwin)");
/* APPLE LOCAL end mainline */

/* APPLE LOCAL begin mainline 2005-04-11 4010614 */
#undef TARGET_FPMATH_DEFAULT
#define TARGET_FPMATH_DEFAULT (TARGET_SSE ? FPMATH_SSE : FPMATH_387)
/* APPLE LOCAL end mainline 2005-04-11 4010614 */

/* APPLE LOCAL begin mainline */
#undef PTRDIFF_TYPE
#define PTRDIFF_TYPE (TARGET_64BIT ? "long int" : "int")

#undef MAX_BITS_PER_WORD
#define MAX_BITS_PER_WORD 64

#define TARGET_OS_CPP_BUILTINS()                \
  do                                            \
    {                                           \
      if (TARGET_64BIT)				\
	builtin_define ("__x86_64__");		\
      else					\
	builtin_define ("__i386__");		\
      builtin_define ("__LITTLE_ENDIAN__");     \
      darwin_cpp_builtins (pfile);		\
    }                                           \
  while (0)
/* APPLE LOCAL end mainline */

/* We want -fPIC by default, unless we're using -static to compile for
   the kernel or some such.  */

#undef CC1_SPEC
/* APPLE LOCAL mainline */
#define CC1_SPEC "%{!mkernel:%{!static:%{!mdynamic-no-pic:-fPIC}}} \
  "/* APPLE LOCAL ARM ignore -mthumb and -mno-thumb */"\
  %<mthumb %<mno-thumb \
  "/* APPLE LOCAL mainline 2007-02-20 5005743 */"\
  %{!mmacosx-version-min=*:-mmacosx-version-min=%(darwin_minversion)} \
  "/* APPLE LOCAL ignore -mcpu=G4 -mcpu=G5 */"\
  %<faltivec %<mno-fused-madd %<mlong-branch %<mlongcall %<mcpu=G4 %<mcpu=G5 \
  %{g: %{!fno-eliminate-unused-debug-symbols: -feliminate-unused-debug-symbols }}"

/* APPLE LOCAL AltiVec */
#define CPP_ALTIVEC_SPEC "%<faltivec"

/* APPLE LOCAL begin mainline */
#undef ASM_SPEC
#define ASM_SPEC "-arch %(darwin_arch) -force_cpusubtype_ALL"

#define DARWIN_ARCH_SPEC "%{m64:x86_64;:i386}"
#define DARWIN_SUBARCH_SPEC "					\
  %{m64: x86_64}						\
  %{!m64: i386}"

/* APPLE LOCAL begin mainline 2007-03-13 5005743 5040758 */ \
/* Determine a minimum version based on compiler options.  */
#define DARWIN_MINVERSION_SPEC				\
 "%{!m64|fgnu-runtime:10.4;				\
    ,objective-c|,objc-cpp-output:10.5;			\
    ,objective-c++|,objective-c++-cpp-output:10.5;	\
    :10.4}"

/* APPLE LOCAL end mainline 2007-03-13 5005743 5040758 */ \
#undef SUBTARGET_EXTRA_SPECS
#define SUBTARGET_EXTRA_SPECS					\
  DARWIN_EXTRA_SPECS						\
  { "darwin_arch", DARWIN_ARCH_SPEC },				\
  { "darwin_crt2", "" },					\
  { "darwin_subarch", DARWIN_SUBARCH_SPEC },
/* APPLE LOCAL end mainline */

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
/* APPLE LOCAL begin mainline */
#undef GOT_SYMBOL_NAME
#define GOT_SYMBOL_NAME (machopic_function_base_name ())
/* APPLE LOCAL end mainline */
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
/* APPLE LOCAL x86_64 support */
#define ASM_QUAD "\t.quad\t"

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
/* APPLE LOCAL begin mainline */
/* Removed ASM_OUTPUT_COMMON and ASM_OUTPUT_LOCAL */
/* APPLE LOCAL end mainline */

/* APPLE LOCAL begin Macintosh alignment 2002-2-19 --ff */
#define MASK_ALIGN_NATURAL	0x40000000
#define TARGET_ALIGN_NATURAL	(target_flags & MASK_ALIGN_NATURAL)
#define rs6000_alignment_flags target_flags
#define MASK_ALIGN_MAC68K	0x20000000
#define TARGET_ALIGN_MAC68K	(target_flags & MASK_ALIGN_MAC68K)

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
/* APPLE LOCAL begin x86_64 support 2006-03-18 */
#undef FUNCTION_PROFILER
#define FUNCTION_PROFILER(FILE, LABELNO)				\
    do {								\
      if (MACHOPIC_INDIRECT && ! TARGET_64BIT)						\
	{								\
	  const char *name = machopic_mcount_stub_name ();		\
	  fprintf (FILE, "\tcall %s\n", name+1);  /*  skip '&'  */	\
	  machopic_validate_stub_or_non_lazy_ptr (name);		\
	}								\
      else fprintf (FILE, "\tcall mcount\n");				\
    } while (0)
/* APPLE LOCAL end x86_64 support 2006-03-18 */

/* APPLE LOCAL CW asm blocks */
extern int flag_iasm_blocks;
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
    if (flag_iasm_blocks)					\
      flag_ms_asms = 1;						\
    /* APPLE LOCAL end CW asm blocks */				\
    if (TARGET_64BIT)						\
      {								\
	if (MACHO_DYNAMIC_NO_PIC_P)				\
	  target_flags &= ~MASK_MACHO_DYNAMIC_NO_PIC;		\
      }								\
  } while (0)

/* APPLE LOCAL begin kexts */
#define C_COMMON_OVERRIDE_OPTIONS do {		\
  SUBTARGET_C_COMMON_OVERRIDE_OPTIONS;		\
  } while (0)
/* APPLE LOCAL end kexts */

/* True, iff we're generating fast turn around debugging code.  When
   true, we arrange for function prologues to start with 6 nops so
   that gdb may insert code to redirect them, and for data to be
   accessed indirectly.  The runtime uses this indirection to forward
   references for data to the original instance of that data.  */

#define TARGET_FIX_AND_CONTINUE (darwin_fix_and_continue)
/* APPLE LOCAL end fix-and-continue x86 */

/* APPLE LOCAL begin mainline 2006-02-21 4439051 */
/* Darwin uses the standard DWARF register numbers but the default
   register numbers for STABS.  Fortunately for 64-bit code the
   default and the standard are the same.  */
#undef DBX_REGISTER_NUMBER
#define DBX_REGISTER_NUMBER(n) 					\
  (TARGET_64BIT ? dbx64_register_map[n]				\
   : write_symbols == DWARF2_DEBUG ? svr4_dbx_register_map[n]	\
   : dbx_register_map[n])

/* Unfortunately, the 32-bit EH information also doesn't use the standard
   DWARF register numbers.  */
#define DWARF2_FRAME_REG_OUT(n, for_eh)					\
  (! (for_eh) || write_symbols != DWARF2_DEBUG || TARGET_64BIT ? (n)	\
   : (n) == 5 ? 4							\
   : (n) == 4 ? 5							\
   : (n) >= 11 && (n) <= 18 ? (n) + 1					\
   : (n))
/* APPLE LOCAL end mainline 2006-02-21 4439051 */

/* APPLE LOCAL begin 4457939 stack alignment mishandled */
/* <rdar://problem/4471596> stack alignment is not handled properly

   Please remove this entire a**le local when addressing this
   Radar.  */
extern void ix86_darwin_init_expanders (void);
#define INIT_EXPANDERS (ix86_darwin_init_expanders ())
/* APPLE LOCAL end 4457939 stack alignment mishandled */

/* APPLE LOCAL begin x86_64 */
/* For 64-bit, we need to add 4 because @GOTPCREL is relative to the
   end of the instruction, but without the 4 we'd only have the right
   address for the start of the instruction. */
#define ASM_MAYBE_OUTPUT_ENCODED_ADDR_RTX(FILE, ENCODING, SIZE, ADDR, DONE)	\
  if (TARGET_64BIT)													\
    {																\
      if ((SIZE) == 4 && ((ENCODING) & 0x70) == DW_EH_PE_pcrel)		\
        {															\
          fputs (ASM_LONG, FILE);									\
          assemble_name (FILE, XSTR (ADDR, 0));						\
          fputs ("+4@GOTPCREL", FILE);								\
          goto DONE;												\
        }															\
    }																\
  else																\
    {																\
      if (ENCODING == ASM_PREFERRED_EH_DATA_FORMAT (2, 1))			\
        {															\
          darwin_non_lazy_pcrel (FILE, ADDR);						\
          goto DONE;												\
        }															\
    }
/* APPLE LOCAL end x86_64 */

/* APPLE LOCAL begin mainline */
#undef REGISTER_TARGET_PRAGMAS
#define REGISTER_TARGET_PRAGMAS() DARWIN_REGISTER_TARGET_PRAGMAS()

#undef TARGET_SET_DEFAULT_TYPE_ATTRIBUTES
#define TARGET_SET_DEFAULT_TYPE_ATTRIBUTES darwin_set_default_type_attributes
/* APPLE LOCAL end mainline */
/* APPLE LOCAL begin CW asm blocks */
#define IASM_VALID_PIC(DECL, E)						\
  do {									\
    if (E->as_immediate && ! TARGET_DYNAMIC_NO_PIC && flag_pic)		\
      warning ("non-pic addressing form not suitible for pic code");	\
  } while (0)
/* APPLE LOCAL end cw asm blocks */

/* APPLE LOCAL begin 4106131 */
#undef TARGET_DEEP_BRANCH_PREDICTION
#define TARGET_DEEP_BRANCH_PREDICTION ((m_PPRO | m_K6 | m_ATHLON_K8 | m_PENT4) & TUNEMASK)
/* APPLE LOCAL end 4106131 */
