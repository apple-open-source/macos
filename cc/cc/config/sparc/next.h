/* Target definitions for GNU compiler for SPARC running OPENSTEP on Mach.
   Copyright 1995-1997 Apple Computer, Inc. (unpublished)  */

/* Any uncommented definitions are re-defines. Lookup corresponding
   comments in sparc.h.  */

/* The macro NEXT_SEMANTICS is supposed to indicate that the *host*
   is OPENSTEP on Mach or Rhapsody, though currently this macro
   is commonly used to simply flag changes we have made to FSF's code.  */
#define NEXT_SEMANTICS

#define MACHO_PIC 1

#include "sparc/sparc.h"
#include "next/nextstep.h"

#define TARGET_SPARC (1)

#undef CPP_PREDEFINES
#define CPP_PREDEFINES "-Dsparc -DNATURAL_ALIGNMENT -DNeXT -Dunix -D__MACH__ -D__BIG_ENDIAN__ -D__ARCHITECTURE__=\"sparc\""

#define TARGET_ARCHITECTURE \
  { {"sparc", 0}}

#define DEFAULT_TARGET_ARCH "sparc"

/* putout data directives in 68k format */

#undef ASM_LONG
#define ASM_LONG ".long"
#undef ASM_SHORT
#define ASM_SHORT ".short"
#undef ASM_BYTE_OP
#define ASM_BYTE_OP ".byte"

/* We don't want .proc generated */

#undef	ASM_DECLARE_RESULT
#define	ASM_DECLARE_RESULT(FILE, RESULT)

#undef ASM_OUTPUT_COMMON
#define ASM_OUTPUT_COMMON(FILE, NAME, SIZE, ROUNDED)  \
( fputs ("\n\t.comm ", (FILE)),		\
  assemble_name ((FILE), (NAME)),		\
  fprintf ((FILE), ",%u\n", (ROUNDED)))

/* don't use .reserve */
#undef ASM_OUTPUT_LOCAL
#define ASM_OUTPUT_LOCAL(FILE, NAME, SIZE, ROUNDED)  \
( fputs (".lcomm ", (FILE)),			\
  assemble_name ((FILE), (NAME)),		\
  fprintf ((FILE), ",%u\n", (ROUNDED)))

/* don't use .skip */
#undef ASM_OUTPUT_SKIP
#define ASM_OUTPUT_SKIP(FILE,SIZE)  \
  fprintf (FILE, "\t.space %u\n", (SIZE))

/* assembler preprocessor directives */
#undef ASM_APP_ON
#define ASM_APP_ON "#APP\n"

#undef ASM_APP_OFF
#define ASM_APP_OFF "#NO_APP\n"

#undef ASM_OUTPUT_DOUBLE
#define ASM_OUTPUT_DOUBLE(FILE,VALUE)					\
  {									\
    long t[2];                                                          \
    REAL_VALUE_TO_TARGET_DOUBLE ((VALUE), t);				\
    fprintf (FILE, "!\t.double %.12e\n\t%s\t0x%lx\n\t%s\t0x%lx\n",	\
	     (VALUE), ASM_LONG, t[0], ASM_LONG, t[1]);			\
  }

#undef ASM_OUTPUT_FLOAT
#define ASM_OUTPUT_FLOAT(FILE,VALUE)					\
  {									\
    long t;								\
    REAL_VALUE_TO_TARGET_SINGLE ((VALUE), t);				\
    fprintf (FILE, "!\t.single %.12e\n\t%s\t0x%lx\n", (VALUE), ASM_LONG, t);\
  }

#undef ASM_OUTPUT_SOURCE_LINE
#define ASM_OUTPUT_SOURCE_LINE(file, line)		\
  { static int sym_lineno = 1;				\
    fprintf (file, "\t.stabn 68,0,%d,LM%d\nLM%d:\n",	\
	     line, sym_lineno, sym_lineno);		\
    sym_lineno += 1; }

#undef ASM_OUTPUT_INT
#define ASM_OUTPUT_INT(FILE,VALUE)              \
( fprintf (FILE, "\t%s ", ASM_LONG),		\
  output_addr_const (FILE, (VALUE)),		\
  fprintf (FILE, "\n"))

#undef ASM_OUTPUT_SHORT
#define ASM_OUTPUT_SHORT(FILE,VALUE)            \
( fprintf (FILE, "\t%s ", ASM_SHORT),		\
  output_addr_const (FILE, (VALUE)),		\
  fprintf (FILE, "\n"))

#undef ASM_OUTPUT_CHAR
#define ASM_OUTPUT_CHAR(FILE,VALUE)             \
( fprintf (FILE, "\t%s ", ASM_BYTE_OP),  	\
  output_addr_const (FILE, (VALUE)),		\
  fprintf (FILE, "\n"))

#undef ASM_OUTPUT_BYTE
#define ASM_OUTPUT_BYTE(FILE,VALUE)  \
  fprintf (FILE, "\t%s 0x%x\n", ASM_BYTE_OP, (VALUE))

#undef ASM_OUTPUT_ADDR_VEC_ELT
#define ASM_OUTPUT_ADDR_VEC_ELT(FILE, VALUE)  \
do {									\
  char label[30];							\
  ASM_GENERATE_INTERNAL_LABEL (label, "L", VALUE);			\
  fprintf (FILE, "\t%s ", ASM_LONG);					\
  assemble_name (FILE, label);						\
  fprintf (FILE, "\n");							\
} while (0)

#undef ASM_OUTPUT_ADDR_DIFF_ELT
#define ASM_OUTPUT_ADDR_DIFF_ELT(FILE, VALUE, REL)			\
do {									\
  char label[30];							\
  ASM_GENERATE_INTERNAL_LABEL (label, "L", VALUE);			\
  fprintf (FILE, "\t%s ", ASM_LONG);					\
  assemble_name (FILE, label);						\
  fprintf (FILE, "-1b\n");						\
} while (0)


#undef SUBTARGET_OVERRIDE_OPTIONS
#if 0 /* This macro is no longer needed!  */
#define SUBTARGET_OVERRIDE_OPTIONS                                       \
{                                                                        \
   if (flag_pic) {                                                       \
      if (1 || getenv("SPARC_DYNAMIC")) {                                \
         warning ("-dynamic trial for sparc target architecture");       \
      }                                                                  \
      else {                                                             \
         flag_pic = 0;                                                   \
         warning ("-dynamic unsupported for sparc target architecture"); \
       }                                                                 \
    }                                                                    \
 }
#else
#define SUBTARGET_OVERRIDE_OPTIONS
#endif

#define RELOAD_PIC_REGISTER reload_sparc_pic_register()
