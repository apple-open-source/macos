/* Target definitions for GNU compiler for mc680x0 running NeXTSTEP
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

#ifndef NEXT_SEMANTICS
#define NEXT_SEMANTICS
#endif

#include "rs6000/rs6000.h"
#include "next/nextstep.h"

/* The default -arch flag in the driver */
#define DEFAULT_TARGET_ARCH "ppc"

#define MACHO_PIC 1

#ifndef MACHOPIC_PURE
#define MACHOPIC_PURE          (flag_pic == 2)
#define MACHOPIC_INDIRECT      (flag_pic)
#define MACHOPIC_JUST_INDIRECT (flag_pic == 1)
#define MACHOPIC_M68K 
#endif

#define MACHOPIC_FUNCTION_BRANCHES

#undef  TARGET_DEFAULT
#define TARGET_DEFAULT (MASK_POWERPC | MASK_MULTIPLE | MASK_STRING | MASK_NEW_MNEMONICS | MASK_NO_FP_IN_TOC | MASK_NO_SUM_IN_TOC)

#define TARGET_ARCHITECTURE \
  { { "ppc", 0 },		/* Accept.  */	       		\
    { "m98k", 0 },		/* Accept.  */	       		\
    { "ppc-nomult", -MASK_MULTIPLE },	/*   */	\
    { "ppc-nostr", -MASK_STRING }}

/* Boundary (in *bits*) on which stack pointer should be aligned.  */

#undef	STACK_BOUNDARY
#define STACK_BOUNDARY 32

/* NeXT's calling convention is to use the struct-value register 
   passing a pointer to the struct to the function being called. */
#undef PCC_STATIC_STRUCT_RETURN

/* Names to predefine in the preprocessor for this target machine.  */
#undef CPP_PREDEFINES
#define CPP_PREDEFINES "-Dppc -DPPC -DNATURAL_ALIGNMENT -DNeXT -Dunix -D__MACH__ -D__BIG_ENDIAN__ -D__ARCHITECTURE__=\"ppc\"" 

#undef ASM_DECLARE_FUNCTION_NAME
#undef SELECT_RTX_SECTION

#undef REGISTER_NAMES
#define REGISTER_NAMES \
 {"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", 		\
  "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",		\
  "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",	\
  "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",	\
  "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",		\
  "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15",		\
  "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",	\
  "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31",	\
  "mq", "lr", "ctr", "ap",				\
  "cr0", "cr1", "cr2", "cr3", "cr4", "cr5", "cr6", "cr7" }

/* This is how to output an internal numbered label where
   PREFIX is the class of label and NUM is the number within the class.  */

#undef ASM_OUTPUT_INTERNAL_LABEL(FILE,PREFIX,NUM)
#define ASM_OUTPUT_INTERNAL_LABEL(FILE,PREFIX,NUM)	\
  fprintf (FILE, "%s%d:\n", PREFIX, NUM)

/* This is how to output an internal label prefix.  rs6000.c uses this
   when generating traceback tables.  */

#undef ASM_OUTPUT_INTERNAL_LABEL_PREFIX(FILE,PREFIX)
#define ASM_OUTPUT_INTERNAL_LABEL_PREFIX(FILE,PREFIX)	\
  fprintf (FILE, "%s", PREFIX)

/* This is how to output an assembler line defining a `double' constant.  */

#undef	ASM_OUTPUT_DOUBLE
#ifdef REAL_VALUE_TO_TARGET_DOUBLE
#define ASM_OUTPUT_DOUBLE(FILE,VALUE)					\
  do {									\
    long hex[2];							\
    REAL_VALUE_TO_TARGET_DOUBLE (VALUE, hex);				\
    if (sizeof (int) == sizeof (long))					\
      fprintf (FILE, "\t.long 0x%x\n\t.long 0x%x\n", hex[0], hex[1]);	\
    else								\
      fprintf (FILE, "\t.long 0x%lx\n\t.long 0x%lx\n", hex[0], hex[1]);	\
  } while (0)
#else
#define ASM_OUTPUT_DOUBLE(FILE,VALUE)					\
 do { if (REAL_VALUE_ISINF (VALUE))					\
        {								\
          if (REAL_VALUE_NEGATIVE (VALUE))				\
            fprintf (FILE, "\t.double 0r-99e999\n");			\
          else								\
            fprintf (FILE, "\t.double 0r99e999\n");			\
        }								\
      else								\
        { char dstr[30];						\
          REAL_VALUE_TO_DECIMAL ((VALUE), "%.20e", dstr);		\
          fprintf (FILE, "\t.double 0r%s\n", dstr);			\
        }								\
    } while (0)
#endif

#undef	ASM_OUTPUT_FLOAT_OPERAND
#ifdef REAL_VALUE_TO_TARGET_SINGLE
#define ASM_OUTPUT_FLOAT_OPERAND(CODE,FILE,VALUE)			\
  do {									\
    long hex;								\
    REAL_VALUE_TO_TARGET_SINGLE (VALUE, hex);				\
    fprintf (FILE, "#0%c%x", (CODE) == 'f' ? 'b' : 'x', hex);		\
  } while (0)
#else
#define ASM_OUTPUT_FLOAT_OPERAND(CODE,FILE,VALUE)		\
  do{ 								\
      if (CODE != 'f')						\
        {							\
          long l;						\
          REAL_VALUE_TO_TARGET_SINGLE (VALUE, l);		\
          if (sizeof (int) == sizeof (long))			\
            asm_fprintf ((FILE), "%I0x%x", l);			\
          else							\
            asm_fprintf ((FILE), "%I0x%lx", l);			\
        }							\
      else if (REAL_VALUE_ISINF (VALUE))			\
        {							\
          if (REAL_VALUE_NEGATIVE (VALUE))			\
            fprintf (FILE, "#0r-99e999");			\
          else							\
            fprintf (FILE, "#0r99e999");			\
        }							\
      else							\
        { char dstr[30];					\
          REAL_VALUE_TO_DECIMAL ((VALUE), "%.9g", dstr);	\
          fprintf (FILE, "#0r%s", dstr);			\
        }							\
    } while (0)
#endif

#undef	ASM_OUTPUT_DOUBLE_OPERAND
#ifdef REAL_VALUE_TO_TARGET_DOUBLE
#define ASM_OUTPUT_DOUBLE_OPERAND(FILE,VALUE)				\
  do {									\
    long hex[2];							\
    REAL_VALUE_TO_TARGET_DOUBLE (VALUE, hex);				\
    fprintf (FILE, "#0b%x%08x", hex[0], hex[1]);			\
  } while (0)
#else
#define ASM_OUTPUT_DOUBLE_OPERAND(FILE,VALUE)				\
 do { if (REAL_VALUE_ISINF (VALUE))					\
        {								\
          if (REAL_VALUE_NEGATIVE (VALUE))				\
            fprintf (FILE, "#0r-99e999");				\
          else								\
            fprintf (FILE, "#0r99e999");				\
        }								\
      else								\
        { char dstr[30];						\
          REAL_VALUE_TO_DECIMAL ((VALUE), "%.20g", dstr);		\
          fprintf (FILE, "#0r%s", dstr);				\
        }								\
    } while (0)
#endif

/* We do not define JUMP_TABLES_IN_TEXT_SECTION, since we wish to keep
   the text section pure.  There is no point in addressing the jump
   tables using pc relative addressing, since they are not in the text
   section, so we undefine CASE_VECTOR_PC_RELATIVE.  This also
   causes the compiler to use absolute addresses in the jump table,
   so we redefine CASE_VECTOR_MODE to be SImode. */

#undef	CASE_VECTOR_MODE
#define CASE_VECTOR_MODE SImode
#undef	CASE_VECTOR_PC_RELATIVE

/* When generating PIC code, jump tables must have 32 bits elements,
   to support scattered loading in the future.  */
#undef ASM_OUTPUT_ADDR_DIFF_ELT
#define ASM_OUTPUT_ADDR_DIFF_ELT(FILE, VALUE, REL)         \
do { fprintf (FILE, "\t.long\tL%u-", (VALUE));             \
     assemble_name (FILE, (char*)machopic_function_base_name ()); \
     fprintf (FILE, "\n"); } while (0)

#undef PIC_OFFSET_TABLE_REGNUM
#define PIC_OFFSET_TABLE_REGNUM 2

/* Make sure jump tables have the same alignment as other pointers.  */

#undef  ASM_OUTPUT_CASE_LABEL
#define ASM_OUTPUT_CASE_LABEL(FILE,PREFIX,NUM,TABLEINSN)	\
{ ASM_OUTPUT_ALIGN (FILE, 1); ASM_OUTPUT_INTERNAL_LABEL (FILE, PREFIX, NUM); }

/* Don't treat addresses involving labels differently from symbol names.
   Previously, references to labels generated pc-relative addressing modes
   while references to symbol names generated absolute addressing modes.  */

#undef	GO_IF_INDEXABLE_BASE(X, ADDR)
#define GO_IF_INDEXABLE_BASE(X, ADDR)	\
{  if (LEGITIMATE_BASE_REG_P (X)) goto ADDR; }

/* This accounts for the return pc and saved fp on the m68k. */

#define OBJC_FORWARDING_STACK_OFFSET 8
#define OBJC_FORWARDING_MIN_OFFSET 8

/* INITIALIZE_TRAMPOLINE is changed so that it also enables executable
   stack.  The __enable_execute_stack also clears the insn cache. */

/* NOTE: part of this is copied from m68k.h */
#undef INITIALIZE_TRAMPOLINE
#define INITIALIZE_TRAMPOLINE(TRAMP, FNADDR, CXT)                          \
{                                                                          \
  rtx _addr, _func;                                                        \
  emit_move_insn (gen_rtx (MEM, SImode, plus_constant (TRAMP, 2)), TRAMP);   \
  emit_move_insn (gen_rtx (MEM, SImode, plus_constant (TRAMP, 18)), CXT);    \
  emit_move_insn (gen_rtx (MEM, SImode, plus_constant (TRAMP, 22)), FNADDR); \
  _addr = memory_address (SImode, (TRAMP));                                  \
  _func =  gen_rtx (SYMBOL_REF, Pmode, "__enable_execute_stack");          \
  emit_library_call (_func, 0, VOIDmode, 1, _addr, Pmode);                   \
}

/* A C expression used to clear the instruction cache from 
   address BEG to address END.   On NeXTSTEP this i a system trap. */

#define CLEAR_INSN_CACHE(BEG, END)   \
   asm volatile ("trap #2")

/* Turn on floating point precision control as default */

/* #define DEFAULT_FPPC 1 */


#undef SUBTARGET_OVERRIDE_OPTIONS
#define SUBTARGET_OVERRIDE_OPTIONS \
 { extern int flag_dave_indirect; \
   if (flag_dave_indirect) flag_pic = 2; \
   if (flag_pic == 2) flag_no_function_cse = 1; }


#undef LEGITIMATE_PIC_OPERAND_P
#define LEGITIMATE_PIC_OPERAND_P(X) \
  (! symbolic_operand (X, VOIDmode) \
   || machopic_operand_p (X)	    \
   || ((GET_CODE(X) == SYMBOL_REF) && SYMBOL_REF_FLAG(X)))


/* Output before instructions.  */

#undef TEXT_SECTION_ASM_OP
#define TEXT_SECTION_ASM_OP ".text"

/* Output before writable data.  */

#undef DATA_SECTION_ASM_OP
#define DATA_SECTION_ASM_OP ".data"

/* Turn off FP constants in TOC */
#undef ASM_OUTPUT_SPECIAL_POOL_ENTRY

/* Assembler defaults symbols to external except those starting with L */
#undef ASM_OUTPUT_EXTERNAL
#undef ASM_OUTPUT_EXTERNAL_LIBCALL

/* Turn off TOC reload slot following calls */
#undef RS6000_CALL_GLUE
#define RS6000_CALL_GLUE ""

/* Define a label "func_TOC" for this func to establish addressability */
#define ASM_OUTPUT_POOL_PROLOGUE(FILE, NAME, DECL, SIZE)  \
  {                                                       \
     extern char toc_buffer[];                            \
     readonly_data_section();                             \
     fprintf(FILE, "\t.align 2\n");                       \
     if ( NAME[0] == '+' || NAME[0] == '-')               \
        sprintf (toc_buffer, "\"%s_TOC\"", NAME);         \
     else                                                 \
        sprintf(toc_buffer, "%s_TOC", NAME);              \
     fprintf(FILE, "%s:\n", toc_buffer);                  \
  }


#define UPDATE_CURRENT_LABEL                           \
   do {                                                \
      extern int currentLabelNumber, const_labelno;    \
      currentLabelNumber = const_labelno;              \
   } while(0)

#undef DEFAULT_SIGNED_CHAR
#define DEFAULT_SIGNED_CHAR (1)

#undef ASM_GENERATE_INTERNAL_LABEL
#ifndef MXW
#define ASM_GENERATE_INTERNAL_LABEL(LABEL,PREFIX,NUM)                   \
  sprintf (LABEL, "*%s%d", PREFIX, NUM)
#else
#define ASM_GENERATE_INTERNAL_LABEL(LABEL,PREFIX,NUM) \
  do {                                                \
     extern int currentLabelNumber;                   \
     if ( !strcmp(PREFIX,"LC") )                      \
        sprintf (LABEL, "*%s%d-%s%d", PREFIX, NUM, PREFIX, currentLabelNumber); \
     else                                                                       \
        sprintf (LABEL, "*%s%d", PREFIX, NUM);                                  \
	} while (0)
#endif




