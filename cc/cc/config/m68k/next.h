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

/* The macro NEXT_SEMANTICS is supposed to indicate that the *host*
   is OPENSTEP on Mach or Rhapsody, though currently this macro
   is commonly used to simply flag changes we have made to FSF's code.  */
#define NEXT_SEMANTICS

#include "m68k/m68k.h"
#include "next/nextstep.h"

/* The default -arch flag in the driver */
#define DEFAULT_TARGET_ARCH "m68k"

#define MACHO_PIC 1

#ifndef MACHOPIC_PURE
#define MACHOPIC_PURE          (flag_pic == 2)
#define MACHOPIC_INDIRECT      (flag_pic)
#define MACHOPIC_JUST_INDIRECT (flag_pic == 1)
#define MACHOPIC_M68K 
#endif

#define MACHOPIC_FUNCTION_BRANCHES

/* See m68k.h.  0407 means 68040 (or 68030 or 68020, with 68881/2).  */

#define TARGET_DEFAULT 0407

#define TARGET_ARCHITECTURE \
  { { "m68k", 0 },		/* Accept.  */	       		\
    { "m68030", -01400 },	/* Just -like -m68030.  */	\
    { "m68030", 5 },						\
    { "m68040", 01007 }}	/* Just like -m68040-only.  */

/* Boundary (in *bits*) on which stack pointer should be aligned.  */

#undef	STACK_BOUNDARY
#define STACK_BOUNDARY 32

/* NeXT's calling convention is to use the struct-value register 
   passing a pointer to the struct to the function being called. */
#undef PCC_STATIC_STRUCT_RETURN

/* Names to predefine in the preprocessor for this target machine.  */

#define CPP_PREDEFINES "-Dmc68000 -Dm68k -DNeXT -Dunix -D__MACH__ -D__BIG_ENDIAN__ -D__ARCHITECTURE__=\"m68k\""

/* Every structure or union's size must be a multiple of 2 bytes.
   (Why isn't this in m68k.h?)  */

#define STRUCTURE_SIZE_BOUNDARY 16
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

/* This is how to output an assembler line defining a `float' constant.  */

#undef	ASM_OUTPUT_FLOAT
#ifdef REAL_VALUE_TO_TARGET_SINGLE
#define ASM_OUTPUT_FLOAT(FILE,VALUE)					\
  do {									\
    long hex;								\
    REAL_VALUE_TO_TARGET_SINGLE (VALUE, hex);				\
    if (sizeof (int) == sizeof (long))					\
      fprintf (FILE, "\t.long 0x%x\n", hex);				\
    else								\
      fprintf (FILE, "\t.long 0x%lx\n", hex);				\
  } while (0)
#else
#define ASM_OUTPUT_FLOAT(FILE,VALUE)					\
 do { if (REAL_VALUE_ISINF (VALUE))					\
        {								\
          if (REAL_VALUE_NEGATIVE (VALUE))				\
            fprintf (FILE, "\t.single 0r-99e999\n");			\
          else								\
            fprintf (FILE, "\t.single 0r99e999\n");			\
        }								\
      else								\
        { char dstr[30];						\
          REAL_VALUE_TO_DECIMAL ((VALUE), "%.20e", dstr);		\
          fprintf (FILE, "\t.single 0r%s\n", dstr);			\
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

/* Make sure jump tables have the same alignment as other pointers.  */

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
