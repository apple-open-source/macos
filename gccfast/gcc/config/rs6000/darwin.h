/* Target definitions for PowerPC running Darwin (Mac OS X).
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

#undef  TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (Darwin/PowerPC)");

/* The "Darwin ABI" is mostly like AIX, but with some key differences.  */

#define DEFAULT_ABI ABI_DARWIN

/* The object file format is Mach-O.  */

#define TARGET_OBJECT_FORMAT OBJECT_MACHO

/* We're not ever going to do TOCs.  */

#define TARGET_TOC 0
#define TARGET_NO_TOC 1

/* APPLE LOCAL long-branch */
#define RS6000_LONG_BRANCH (1)
#define OUTPUT_COMPILER_STUB (1)

/* Handle #pragma weak and #pragma pack.  */
#define HANDLE_SYSV_PRAGMA 1

/* The Darwin ABI always includes AltiVec, can't be (validly) turned
   off.  */

#define SUBTARGET_OVERRIDE_OPTIONS  \
  /* APPLE LOCAL begin -malign-mac68k sts */ \
  do {						\
    rs6000_altivec_abi = 1;			\
  } while (0)
/* APPLE LOCAL end -malign-mac68k sts */

#define TARGET_OS_CPP_BUILTINS()                \
  do                                            \
    {                                           \
      builtin_define ("__ppc__");               \
      builtin_define ("__POWERPC__");           \
      builtin_define ("__NATURAL_ALIGNMENT__"); \
      /* APPLE LOCAL XJR */                     \
      SUBTARGET_OS_CPP_BUILTINS ();             \
    }                                           \
  while (0)

/* We want -fPIC by default, unless we're using -static to compile for
   the kernel or some such.  */

/* APPLE LOCAL  dynamic-no-pic  */
/* APPLE LOCAL  AltiVec */
#define CC1_SPEC "\
%{static: %{Zdynamic: %e conflicting code gen style switches are used}}\
%{static: %{mdynamic-no-pic: %e conflicting code gen style switches are used}}\
%{!static:%{!fast:%{!fastf:%{!mdynamic-no-pic:-fPIC}}}} %{faltivec:-D__VEC__=10206 -D__ALTIVEC__=1}"

/* APPLE LOCAL asm flags */
#define ASM_SPEC "\
  %{Zforce_cpusubtype_ALL:-force_cpusubtype_ALL} \
  %{!Zforce_cpusubtype_ALL:%{faltivec:-force_cpusubtype_ALL}}\
  %{!fast*:\
    %{mcpu=970|mcpu=G5|mpowerpc64|mcpu=power4: -arch ppc970}\
    %{!mcpu=970:%{!mcpu=G5:%{!mpowerpc64:%{!mcpu=power4: -arch %T}}}}}\
  %{fast*:\
    %{mcpu=G5|mcpu=970|mpowerpc64|mcpu=power4:-arch ppc970}\
    %{!mcpu*:%{!mpowerpc64:-arch ppc970}}\
    %{mcpu*|mpowerpc*:%{!mcpu=G5:%{!mcpu=970:%{!mpowerpc64:%{!mcpu=power4: -arch %T}}}}}}"

/* APPLE LOCAL AltiVec */
#define CPP_ALTIVEC_SPEC "%{faltivec:-D__VEC__=10206 -D__ALTIVEC__=1}"

/* Make both r2 and r3 available for allocation.  */
#define FIXED_R2 0
#define FIXED_R13 0

/* Base register for access to local variables of the function.  */

#undef  FRAME_POINTER_REGNUM
#define FRAME_POINTER_REGNUM 30

#undef  RS6000_PIC_OFFSET_TABLE_REGNUM
#define RS6000_PIC_OFFSET_TABLE_REGNUM 31

/* APPLE LOCAL begin -pg fix */
/* -pg has a problem which is normally concealed by -fPIC;
   either -mdynamic-no-pic or -static exposes the -pg problem, causing the
   crash.  FSF gcc for Darwin also has this bug.  The problem is that -pg
   causes several int registers to be saved and restored although they may
   not actually be used (config/rs6000/rs6000.c:first_reg_to_save()).  In the
   rare case where none of them is actually used, a consistency check fails
   (correctly).  This cannot happen with -fPIC because the PIC register (R31)
   is always "used" in the sense checked by the consistency check.  The
   easy fix, here, is therefore to mark R31 always "used" whenever -pg is on.
   A better, but harder, fix would be to improve -pg's register-use
   logic along the lines suggested by comments in the function listed above. */
#undef PIC_OFFSET_TABLE_REGNUM
#define PIC_OFFSET_TABLE_REGNUM ((flag_pic || profile_flag) \
    ? RS6000_PIC_OFFSET_TABLE_REGNUM \
    : INVALID_REGNUM)
/* APPLE LOCAL end -pg fix */

/* Pad the outgoing args area to 16 bytes instead of the usual 8.  */

#undef STARTING_FRAME_OFFSET
#define STARTING_FRAME_OFFSET						\
  /* APPLE LOCAL AltiVec */ \
  (RS6000_ALIGN (current_function_outgoing_args_size			\
   + RS6000_VARARGS_AREA						\
   + RS6000_SAVE_AREA, 16))

#undef STACK_DYNAMIC_OFFSET
#define STACK_DYNAMIC_OFFSET(FUNDECL)					\
  /* APPLE LOCAL AltiVec */ \
  (RS6000_ALIGN (current_function_outgoing_args_size			\
   + (STACK_POINTER_OFFSET), 16))

/* APPLE LOCAL change from FSF3.4 */
/* These are used by -fbranch-probabilities */
#define HOT_TEXT_SECTION_NAME "__TEXT,__text,regular,pure_instructions"
/* APPLE LOCAL begin - rarely executed bb optimization */
#define UNLIKELY_EXECUTED_TEXT_SECTION_NAME \
                              "__TEXT,__unexecuted,regular,pure_instructions"
#define SECTION_FORMAT_STRING ".section %s\n\t.align 2\n"
/* APPLE LOCAL end - rarely executed bb optimization */

/* APPLE LOCAL improve performance */
/* Define cutoff for using external functions to save floating point.
   For Darwin, use the function for more than a few registers.  */

/* APPLE LOCAL long-branch */
#define FP_SAVE_INLINE(FIRST_REG) (((FIRST_REG) > 60 && (FIRST_REG) < 64) || TARGET_LONG_BRANCH)

/* APPLE LOCAL begin AltiVec */
/* Define cutoff for using external functions to save vector registers.  */

/* APPLE LOCAL long-branch */
#define VECTOR_SAVE_INLINE(FIRST_REG) \
  (((FIRST_REG) >= LAST_ALTIVEC_REGNO - 1 && (FIRST_REG) <= LAST_ALTIVEC_REGNO) || TARGET_LONG_BRANCH)

/* vector pixel and vector bool are aliases of other vector types.  */

#define VECTOR_PIXEL_AND_BOOL_NOT_DISTINCT
/* APPLE LOCAL end AltiVec */

/* Always use the "debug" register names, they're what the assembler
   wants to see.  */

#undef REGISTER_NAMES
#define REGISTER_NAMES DEBUG_REGISTER_NAMES

/* This outputs NAME to FILE.  */

#undef  RS6000_OUTPUT_BASENAME
#define RS6000_OUTPUT_BASENAME(FILE, NAME)	\
    assemble_name (FILE, NAME);

/* This causes the text and symbol-stub sections to come out adjacent
   in the assembly code, hence out of the linker.  This is needed to
   prevent out-of-range branches when there is a large data section.
   If -static, don't.  Alas, there is no flag_static, so we look for
   its inverse (-fpic or -mdynamic-no-pic).  */
  
#define ASM_FILE_START(FILE)	\
  if (flag_pic || MACHO_DYNAMIC_NO_PIC_P ()) \
    { \
      fprintf (FILE, "\t.section __TEXT,__text,regular,pure_instructions\n"); \
      if (MACHO_DYNAMIC_NO_PIC_P ()) \
        { \
          fprintf (FILE, "\t.section __TEXT,__symbol_stub1,"); \
          fprintf (FILE, "symbol_stubs,pure_instructions,16\n"); \
        } \
      else \
        { \
          fprintf (FILE, "\t.section __TEXT,__picsymbolstub1,"); \
          fprintf (FILE, "symbol_stubs,pure_instructions,32\n"); \
        } \
    }
  
/* Globalizing directive for a label.  */
#undef GLOBAL_ASM_OP
#define GLOBAL_ASM_OP "\t.globl "
#undef TARGET_ASM_GLOBALIZE_LABEL

/* This is how to output an internal label prefix.  rs6000.c uses this
   when generating traceback tables.  */
/* Not really used for Darwin?  */

#undef ASM_OUTPUT_INTERNAL_LABEL_PREFIX
#define ASM_OUTPUT_INTERNAL_LABEL_PREFIX(FILE,PREFIX)	\
  fprintf (FILE, "%s", PREFIX)

/* This says how to output an assembler line to define a global common
   symbol.  */
#undef  ASM_OUTPUT_ALIGNED_COMMON
#define ASM_OUTPUT_COMMON(FILE, NAME, SIZE, ROUNDED)		\
  do { int localsize = (SIZE);					\
    /* 0 means an external reference to the assembler.  */	\
       if (localsize == 0)					\
	 localsize = 1;						\
       fputs (".comm ", (FILE));				\
       RS6000_OUTPUT_BASENAME ((FILE), (NAME));			\
       fprintf ((FILE), ",%d\n", localsize); } while (0)

/* Override the standard rs6000 definition.  */

#undef ASM_COMMENT_START
#define ASM_COMMENT_START ";"

/* APPLE LOCAL don't define SAVE_FP_PREFIX and friends */

/* This is how to output an assembler line that says to advance
   the location counter to a multiple of 2**LOG bytes using the
   "nop" instruction as padding.  */

#define ASM_OUTPUT_ALIGN_WITH_NOP(FILE,LOG)                   \
  do                                                          \
    {                                                         \
      if ((LOG) < 3)                                          \
        {                                                     \
          ASM_OUTPUT_ALIGN (FILE,LOG);                        \
        }                                                     \
      else /* nop == ori r0,r0,0 */                           \
        fprintf (FILE, "\t.align32 %d,0x60000000\n", (LOG));  \
    } while (0)

#ifdef HAVE_GAS_MAX_SKIP_P2ALIGN
/* This is supported in cctools 465 and later.  The macro test
   above prevents using it in earlier build environments.  */
#define ASM_OUTPUT_MAX_SKIP_ALIGN(FILE,LOG,MAX_SKIP)	      \
  if ((LOG) != 0)                                             \
    {                                                         \
      if ((MAX_SKIP) == 0)                                    \
        fprintf ((FILE), "\t.p2align %d\n", (LOG));	      \
      else                                                    \
        fprintf ((FILE), "\t.p2align %d,,%d\n", (LOG), (MAX_SKIP)); \
    }
#endif

/* Generate insns to call the profiler.  */

#define PROFILE_HOOK(LABEL)   output_profile_hook (LABEL)

/* Function name to call to do profiling.  */

#define RS6000_MCOUNT "*mcount"

/* Default processor: a G4.  */

#undef PROCESSOR_DEFAULT
#define PROCESSOR_DEFAULT  PROCESSOR_PPC7400

/* Default target flag settings.  Despite the fact that STMW/LMW
   serializes, it's still a big codesize win to use them.  Use FSEL by
   default as well.  */

#undef  TARGET_DEFAULT
#define TARGET_DEFAULT (MASK_POWERPC | MASK_MULTIPLE | MASK_NEW_MNEMONICS \
                      | MASK_PPC_GFXOPT)

/* Since Darwin doesn't do TOCs, stub this out.  */

#define ASM_OUTPUT_SPECIAL_POOL_ENTRY_P(X, MODE)  0

/* Unlike most other PowerPC targets, chars are signed, for
   consistency with other Darwin architectures.  */

#undef DEFAULT_SIGNED_CHAR
#define DEFAULT_SIGNED_CHAR (1)

/* Given an rtx X being reloaded into a reg required to be      
   in class CLASS, return the class of reg to actually use.     
   In general this is just CLASS; but on some machines
   in some cases it is preferable to use a more restrictive class.
  
   On the RS/6000, we have to return NO_REGS when we want to reload a
   floating-point CONST_DOUBLE to force it to be copied to memory.

   Don't allow R0 when loading the address of, or otherwise furtling with,
   a SYMBOL_REF.  */

#undef PREFERRED_RELOAD_CLASS
#define PREFERRED_RELOAD_CLASS(X,CLASS)			\
  (((GET_CODE (X) == CONST_DOUBLE			\
    && GET_MODE_CLASS (GET_MODE (X)) == MODE_FLOAT)	\
   ? NO_REGS						\
   : (GET_MODE_CLASS (GET_MODE (X)) == MODE_INT 	\
      && (CLASS) == NON_SPECIAL_REGS)			\
   ? GENERAL_REGS					\
   : (GET_CODE (X) == SYMBOL_REF || GET_CODE (X) == HIGH)	\
   ? BASE_REGS						\
   : (CLASS)))

/* Fix for emit_group_load (): force large constants to be pushed via regs.  */
#define ALWAYS_PUSH_CONSTS_USING_REGS_P		1

/* APPLE LOCAL begin Macintosh alignment 2002-2-26 ff */
/* This now supports the Macintosh power, mac68k, and natural 
   alignment modes.  It now has one more parameter than the standard 
   version of the ADJUST_FIELD_ALIGN macro.  
   
   The macro works as follows: We use the computed alignment of the 
   field if we are in the natural alignment mode or if the field is 
   a vector.  Otherwise, if we are in the mac68k alignment mode, we
   use the minimum of the computed alignment and 16 (pegging at
   2-byte alignment).  If we are in the power mode, we peg at 32
   (word alignment) unless it is the first field of the struct, in 
   which case we use the computed alignment.  */
#undef ADJUST_FIELD_ALIGN
#define ADJUST_FIELD_ALIGN(FIELD, COMPUTED, FIRST_FIELD_P)	\
  (TARGET_ALIGN_NATURAL ? (COMPUTED) :				\
   (((COMPUTED) == RS6000_VECTOR_ALIGNMENT)			\
    ? RS6000_VECTOR_ALIGNMENT					\
    : (MIN ((COMPUTED), 					\
    	    (TARGET_ALIGN_MAC68K ? 16 				\
    	    			 : ((FIRST_FIELD_P) ? (COMPUTED) \
    	    			 		    : 32))))))

#undef ROUND_TYPE_ALIGN
/* Macintosh alignment modes require more complicated handling
   of alignment, so we replace the macro with a call to a
   out-of-line function.  */
union tree_node;
extern unsigned round_type_align (union tree_node*, unsigned, unsigned); /* rs6000.c  */
#define ROUND_TYPE_ALIGN(STRUCT, COMPUTED, SPECIFIED)	\
  round_type_align(STRUCT, COMPUTED, SPECIFIED)

/* Macros related to the switches that specify the alignment of fields
   within structs.  */
#define MASK_ALIGN_MAC68K       0x04000000
#define MASK_ALIGN_NATURAL      0x08000000
#define TARGET_ALIGN_MAC68K     (target_flags & MASK_ALIGN_MAC68K)
#define TARGET_ALIGN_NATURAL    (target_flags & MASK_ALIGN_NATURAL)

#undef SUBTARGET_SWITCHES
#define SUBTARGET_SWITCHES						\
  {"align-mac68k",      MASK_ALIGN_MAC68K,				\
	N_("Align structs and unions according to mac68k rules")},	\
  {"align-power",       - (MASK_ALIGN_MAC68K | MASK_ALIGN_NATURAL),	\
	N_("Align structs and unions according to PowerPC rules")},	\
  {"align-natural",     MASK_ALIGN_NATURAL,				\
	N_("Align structs and unions according to natural rules")},
/* APPLE LOCAL end Macintosh alignment 2002-2-26 ff */

/* APPLE LOCAL AltiVec */
#undef DWARF_FRAME_REGISTERS
#define DWARF_FRAME_REGISTERS 110

/* APPLE LOCAL begin alignment */
/* Make sure local alignments come from the type node, not the mode;
   mode-based alignments are wrong for vectors.  */
#undef LOCAL_ALIGNMENT
#define LOCAL_ALIGNMENT(TYPE, ALIGN)	(MAX (ALIGN, TYPE_ALIGN (TYPE)))
/* APPLE LOCAL end alignment */

/* XXX: Darwin supports neither .quad, or .llong, but it also doesn't
   support 64 bit PowerPC either, so this just keeps things happy.  */
#define DOUBLE_INT_ASM_OP "\t.quad\t"

/* APPLE LOCAL begin revert back to cost of 3. D. Experiment shows that it is best on Q37*/
#if 0
/* APPLE LOCAL begin branch cost */
#undef BRANCH_COST
/* Better code is generated by saying conditional branches take 1 tick.  */
#define BRANCH_COST	1
/* APPLE LOCAL end branch cost */
#endif
/* APPLE LOCAL end revert back to cost of 3. D. Experiment shows that it is best on Q37*/

/* Get HOST_WIDE_INT and CONST_INT to be 32 bits, for compile time
   space/speed.  */
#undef MAX_LONG_TYPE_SIZE
#define MAX_LONG_TYPE_SIZE 32

/* APPLE LOCAL indirect calls in R12 */
/* Address of indirect call must be computed here */
#define MAGIC_INDIRECT_CALL_REG 12

/* APPLE LOCAL begin backport 3721776 fix from FSF mainline. */
/* For binary compatibility with 2.95; Darwin C APIs use bool from
   stdbool.h, which was an int-sized enum in 2.95.  Users can explicitly
   choose to have sizeof(bool)==1 with the -mone-byte-bool switch. */
extern const char *darwin_one_byte_bool;
#define BOOL_TYPE_SIZE (darwin_one_byte_bool ? CHAR_TYPE_SIZE : INT_TYPE_SIZE)
/* APPLE LOCAL end backport 3721776 fix from FSF mainline. */

/* APPLE LOCAL OS pragma hook */
/* Register generic Darwin pragmas as "OS" pragmas.  */
