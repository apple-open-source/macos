/* Target definitions for GNU compiler for hppa running NeXTSTEP
   Copyright (C) 1989, 1990, 1991, 1992, 1993 Free Software Foundation, Inc.

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

/* The macro NEXT_SEMANTICS is supposed to indicate that the *host*
   is OPENSTEP on Mach or Rhapsody, though currently this macro
   is commonly used to simply flag changes we have made to FSF's code.  */
#define NEXT_SEMANTICS

/* must before the inclusion of pa/pa.h */
#define NeXT_ASM

#define MACHO_PIC 1

#define TARGET_DEFAULT (1+128+512)	/* 1.1, gas, stub calls */

#define TARGET_ARCHITECTURE \
  { { "hppa", 1 },		/* same as snake.  */		\
    { "hppa1.0", -1 },		/* nosnake.  */			\
  }

#include "pa/pa.h"
#include "next/nextstep.h"


#define DEFAULT_TARGET_ARCH "hppa"

#define HP_FP_ARG_DESCRIPTOR_REVERSED

#undef CPP_PREDEFINES
#define CPP_PREDEFINES "-Dhppa -DNATURAL_ALIGNMENT -DNeXT -Dunix -D__MACH__ -D__BIG_ENDIAN__ -D__ARCHITECTURE__=\"hppa\" -D_PA_RISC1_1"

#undef ASM_FILE_START
#define ASM_FILE_START

#undef ASM_FILE_END
#undef ASM_OUTPUT_EXTERNAL_LIBCALL
#undef ASM_OUTPUT_EXTERNAL

#undef FUNCTION_PROLOGUE
#define FUNCTION_PROLOGUE

#undef FUNCTION_EPILOGUE
#define FUNCTION_EPILOGUE

#undef GLOBAL_ASM_OP
#define GLOBAL_ASM_OP ".globl"

#undef SIZE_TYPE
#define SIZE_TYPE "long unsigned int"

#undef ASM_GLOBALIZE_LABEL
#define ASM_GLOBALIZE_LABEL(FILE,NAME)	\
  do { fprintf (FILE, "%s ", GLOBAL_ASM_OP);		\
       assemble_name (FILE, NAME);			\
       fputs ("\n", FILE);} while (0)

/* This is how to output an internal numbered label where
   PREFIX is the class of label and NUM is the number within the class.  */

#undef ASM_OUTPUT_INTERNAL_LABEL
#define ASM_OUTPUT_INTERNAL_LABEL(FILE,PREFIX,NUM)	\
  asm_fprintf (FILE, "%0L%s%d:\n", PREFIX, NUM)

/* This is how to store into the string LABEL
   the symbol_ref name of an internal numbered label where
   PREFIX is the class of label and NUM is the number within the class.
   This is suitable for output with `assemble_name'.  */

#undef STRING_SECTION_NAME
#define STRING_SECTION_NAME "ascii"

#undef ASM_GENERATE_INTERNAL_LABEL
#define ASM_GENERATE_INTERNAL_LABEL(LABEL,PREFIX,NUM)	\
  sprintf (LABEL, "*%s%d", PREFIX, NUM)

/* Enable simulation when cross-compiling */

#if !defined(HOST_WORDS_BIG_ENDIAN)
#define REAL_ARITHMETIC
#elif HOST_WORDS_BIG_ENDIAN == 0
#define REAL_ARITHMETIC
#endif

#undef ASM_OUTPUT_ALIGNED_COMMON
#define ASM_OUTPUT_ALIGNED_COMMON(FILE, NAME, SIZE, ALIGNED)  		\
{ const_section ();							\
  assemble_name ((FILE), (NAME));					\
  fputs ("\t.comm ", (FILE));						\
  fprintf ((FILE), "%d\n", MAX ((SIZE), ((ALIGNED) / BITS_PER_UNIT)));}

/* This is how to output an assembler line defining a `double' constant.  */

#undef ASM_OUTPUT_DOUBLE
#define ASM_OUTPUT_DOUBLE(FILE,VALUE)					\
  do {									\
    long hex[2];							\
    REAL_VALUE_TO_TARGET_DOUBLE (VALUE, hex);				\
    fprintf(FILE, "\t;	.double %.20g\n", (VALUE));			\
    fprintf (FILE, "\t.long 0x%x\n\t.long 0x%x\n", hex[0], hex[1]);	\
  } while (0)

/* This is how to output an assembler line defining a `float' constant.  */

#undef ASM_OUTPUT_FLOAT
#define ASM_OUTPUT_FLOAT(FILE,VALUE)			\
do { long l;						\
     REAL_VALUE_TO_TARGET_SINGLE (VALUE, l);		\
     fprintf(FILE, "\t;	.single %.12e\n", (VALUE));	\
     fprintf (FILE, "\t.long 0x%x\n", l);		\
   } while (0)

#undef ASM_OUTPUT_SKIP
#define ASM_OUTPUT_SKIP(FILE,SIZE)  \
  fprintf (FILE, "\t.space %u\n", (SIZE))

/* Output to assembler file text saying following lines
   may contain character constants, extra white space, comments, etc.  */

#undef	ASM_APP_ON
#define ASM_APP_ON "#APP\n"

/* Output to assembler file text saying following lines
   no longer contain unusual constructs.  */

#undef	ASM_APP_OFF
#define ASM_APP_OFF "#NO_APP\n"

/* This is how to output a reference to a user-level label named NAME.
   `assemble_name' uses this.  */

#define ASM_GENERATE_LABELREF(BUFFER,NAME)	\
  do { if (NAME[0] == '+' || NAME[0] == '-') sprintf (BUFFER, "\"%s\"", NAME); 	\
       else if (!strncmp (NAME, "_OBJC_", 6)) sprintf (BUFFER, "L%s", NAME);   	\
       else if (!strncmp (NAME, ".objc_class_name_", 17)			\
		|| !strncmp (NAME, "$$", 2)) strcpy (BUFFER, NAME);		\
       else if (!strncmp (NAME, "mcount", 6)) sprintf (BUFFER, "%s", NAME);   	\
       else sprintf (BUFFER, "_%s", NAME); } while (0)

/* This is how to output the definition of a user-level label named NAME,
   such as the label on a static function or variable NAME.  */

#undef ASM_OUTPUT_LABEL
#define ASM_OUTPUT_LABEL(FILE, NAME)	\
  do { assemble_name (FILE, NAME); 	\
       if (TARGET_GAS)			\
	 fputc (':', FILE);		\
       fputc ('\n', FILE); } while (0)


#undef ASM_OUTPUT_LABELREF
#define ASM_OUTPUT_LABELREF(FILE,NAME)				\
  do { char __buf[256]; ASM_GENERATE_LABELREF (__buf, NAME); 	\
       fprintf (FILE, "%s", __buf); } while (0)

/* Base register for access to local variables of the function.  */
#undef FRAME_POINTER_REGNUM
#define FRAME_POINTER_REGNUM 4

#undef ARG_POINTER_REGNUM
#define ARG_POINTER_REGNUM FRAME_POINTER_REGNUM 

/* See comments in rtl.h */
#define HARD_FRAME_POINTER_REGNUM FRAME_POINTER_REGNUM

/* Output before read-only data.  */

#undef TEXT_SECTION_ASM_OP
#define TEXT_SECTION_ASM_OP ".text"

/* Output before writable data.  */

#undef DATA_SECTION_ASM_OP
#define DATA_SECTION_ASM_OP ".data"

/* override how BSS is handled */

#undef BSS_SECTION_ASM_OP

/* This says how to output an assembler line
   to define a global common symbol.  */

#undef ASM_OUTPUT_COMMON
#define ASM_OUTPUT_COMMON(FILE, NAME, SIZE, ROUNDED)  \
( fputs (".comm ", (FILE)),			\
  assemble_name ((FILE), (NAME)),		\
  fprintf ((FILE), ",%u\n", (ROUNDED)))

/* This says how to output an assembler line
   to define a local common symbol.  */

#undef ASM_OUTPUT_LOCAL
#define ASM_OUTPUT_LOCAL(FILE, NAME, SIZE, ROUNDED)  \
( fputs (".lcomm ", (FILE)),			\
  assemble_name ((FILE), (NAME)),		\
  fprintf ((FILE), ",%u\n", (ROUNDED)))

#undef ASM_FORMAT_PRIVATE_NAME
#define ASM_FORMAT_PRIVATE_NAME(OUTPUT, NAME, LABELNO)	\
( (OUTPUT) = (char *) alloca (strlen ((NAME)) + 10),	\
  sprintf ((OUTPUT), "%s.%d", (NAME), (LABELNO)))

#undef ENCODE_SECTION_INFO

#undef	ASM_DECLARE_FUNCTION_NAME

#undef TRAMPOLINE_TEMPLATE(FILE)
#define TRAMPOLINE_TEMPLATE(FILE) \
{						\
  fprintf (FILE, "\tldw 12(0,%%r22),%%r21\n");	\
  fprintf (FILE, "\tbe 0(4,%%r21)\n");		\
  fprintf (FILE, "\tldw 16(0,%%r22),%%r29\n");	\
  fprintf (FILE, "\t.short 0\n");		\
  fprintf (FILE, "\t.short 0\n");		\
}

#undef ASM_OUTPUT_INT
#define ASM_OUTPUT_INT(FILE,VALUE)  \
{ fprintf (FILE, "\t.long ");			\
  if (function_label_operand (VALUE, VOIDmode)  \
      && in_section != in_text)			\
    fprintf (FILE, "P%%");			\
  output_addr_const (FILE, (VALUE));		\
  fprintf (FILE, "\n");}

#undef ASM_OUTPUT_SHORT
#define ASM_OUTPUT_SHORT(FILE,VALUE)  \
( fprintf (FILE, "\t.short "),			\
  output_addr_const (FILE, (VALUE)),		\
  fprintf (FILE, "\n"))

#undef ASM_OUTPUT_ADDR_VEC_ELT
#define ASM_OUTPUT_ADDR_VEC_ELT(FILE, VALUE)  \
  fprintf (FILE, "\tb L%d\n\tnop\n", VALUE)

#define OUTPUT_COMPILER_STUB

/* this may not work for double values --  hard coded to 32 bit entities */

#define OBJC_FORWARDING_REG_OFFSET(ISREG, OFF, REGNO) \
  do { OFF = (FP_REG_CLASS_P( REGNO_REG_CLASS(REGNO)) \
              ? -2 * ((REGNO) - 44) - 4               \
              :  4 * ((REGNO) - 26) - 4);             \
       ISREG = 0; } while (0)

#undef FUNCTION_ARG
#define FUNCTION_ARG(CUM, MODE, TYPE, NAMED)		      		\
  (4 >= ((CUM).words + FUNCTION_ARG_SIZE ((MODE), (TYPE)))		\
   ? gen_rtx (REG, (MODE),						\
	      (FUNCTION_ARG_SIZE ((MODE), (TYPE)) > 1			\
	       ? (((MODE) == DFmode)					\
		  ? ((CUM).words ? 50 : 46) 				\
		  : ((CUM).words ? 23 : 25))				\
	       : (((MODE) == SFmode)					\
		  ? (44 + 2 * (CUM).words)					\
		  : (27 - (CUM).words - FUNCTION_ARG_SIZE ((MODE), (TYPE))))))\
   : 0)

/*
   After the magic in calls.c has moved an argument into some register,
   this macro is expanded so the backend can choose to duplicate
   or spill that load to somewhere else.  
     The NeXTSTEP/PA backend uses this to spill arguments from float
   registers into the general purpose regs.  

   NAME is the name of the function being called.
   NREGS is the number of registers being loaded into.
   REG_RTX is the rtx for the (first) register being loaded.
   VALUE_RTX is the value being loaded.
   MODE is the mode the value is loaded in.

   The USE_DUP_REG_ARG_LOAD macro allows emitting USE insns 
   for possible registers changed.
*/

#define DUP_REG_ARG_LOAD(NAME, NREGS, REG_RTX, VALUE_RTX, MODE)         \
  do { if ((MODE)==DFmode && REGNO(REG_RTX) >= 46) {			\
           move_block_to_reg ((REGNO(REG_RTX)==46 ? 25 : 23), 		\
		      validize_mem(VALUE_RTX), 2, MODE);		\
       } else if ((MODE)==SFmode && REGNO(REG_RTX) >= 44) {		\
           move_block_to_reg (26-((REGNO(REG_RTX)-44)/2),		\
		      validize_mem(VALUE_RTX), 1, MODE); }              \
  } while (0)

/* Emit insn's to actually use the spilled regs so the loads
   will not be handled wrong by the optimizer. */
#define USE_DUP_REG_ARG_LOAD(NAME, NREGS, REG_RTX, MODE) 	\
  do { if (NREGS == 0) { if ((MODE)==DFmode && REGNO(REG_RTX) >= 46) {	\
	   use_regs ((REGNO(REG_RTX)==46 ? 25 : 23), 2);		\
       } else if ((MODE)==SFmode) {					\
           emit_insn (gen_rtx (USE, VOIDmode, gen_rtx (REG, SImode, 26-((REGNO (REG_RTX)-44)/2)))); }              \
  }} while (0)

#define PROFILE_LABEL_PREFIX    

#undef OVERRIDE_OPTIONS

#undef CONSTANT_ADDRESS_P
#define CONSTANT_ADDRESS_P(X) \
  ((GET_CODE (X) == LABEL_REF || GET_CODE (X) == SYMBOL_REF		\
   || GET_CODE (X) == CONST_INT || GET_CODE (X) == CONST		\
   || GET_CODE (X) == HIGH) 						\
   && (reload_in_progress || reload_completed 				\
       || ! symbolic_expression_p (X) || machopic_operand_p (X)))

#define HI_SUM_TARGET_RTX 	(gen_rtx (REG, SImode, 1))
#define HI_SUM_TARGET_REGNO 	1

#undef ASM_OUTPUT_ADDR_DIFF_ELT
#define ASM_OUTPUT_ADDR_DIFF_ELT(FILE, VALUE, REL)  \
  fprintf (FILE, "\tb L%d\n\tnop\n", VALUE)

#undef PIC_OFFSET_TABLE_REGNUM
#define PIC_OFFSET_TABLE_REGNUM 18

#undef OPTIMIZATION_OPTIONS
#define OPTIMIZATION_OPTIONS(OPTIMIZE) \
{  								\
  if (OPTIMIZE >= 1)						\
    {								\
      flag_cse_follow_jumps = 1;				\
      flag_cse_skip_blocks = 1;					\
      flag_expensive_optimizations = 1;				\
      flag_strength_reduce = 1;					\
      flag_rerun_cse_after_loop = 1;				\
      flag_caller_saves = 1;					\
      flag_schedule_insns = 1;					\
      flag_schedule_insns_after_reload = 1;		       	\
    }								\
  if (OPTIMIZE >= 2) 						\
    flag_omit_frame_pointer = 1;				\
}

