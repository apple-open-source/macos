/* Target definitions for GNU compiler for PowerPC running MacOS X.
   Copyright 1998 Apple Computer, Inc. (unpublished)  */

#define	APPLE_SEMANTICS
#define	TARGET_MACOS	1

#include "rs6000/powerpc.h"

#define HAS_INIT_SECTION

/* PPCAsm takes different parameters than as. */

#undef	ASM_SPEC
#define ASM_SPEC "-case on -typecheck strict %(asm_cpu)"

#undef	ASM_DEFAULT_SPEC
#define	ASM_DEFAULT_SPEC	""

/* Common ASM definitions used by ASM_SPEC among the various targets
   for handling -mcpu=xxx switches.  */
#undef	ASM_CPU_SPEC
#define ASM_CPU_SPEC \
"%{!mcpu*: \
  %{mpower: %{!mpower2: -dialect Power}} \
  %{mpower2: -dialect Power} \
  %{mpowerpc*: -dialect PowerPC} \
  %{mno-power: %{!mpowerpc*: -dialect PowerPC}} \
  %{!mno-power: %{!mpower2: %(asm_default)}}} \
%{mcpu=common: -dialect PowerPC} \
%{mcpu=power: -dialect Power} \
%{mcpu=power2: -dialect Power} \
%{mcpu=powerpc: -dialect PowerPC} \
%{mcpu=rios: -dialect Power} \
%{mcpu=rios1: -dialect Power} \
%{mcpu=rios2: -dialect Power} \
%{mcpu=rsc: -dialect Power} \
%{mcpu=rsc1: -dialect Power} \
%{mcpu=403: -dialect PowerPC} \
%{mcpu=505: -dialect PowerPC} \
%{mcpu=601: -dialect PowerPC} \
%{mcpu=602: -dialect PowerPC} \
%{mcpu=603: -dialect PowerPC} \
%{mcpu=603e: -dialect PowerPC} \
%{mcpu=604: -dialect PowerPC} \
%{mcpu=620: -dialect PowerPC} \
%{mcpu=821: -dialect PowerPC} \
%{mcpu=860: -dialect PowerPC}"


/* This macro generates the assembly code for function exit,
   on machines that need it.  If FUNCTION_EPILOGUE is not defined
   then individual return instructions are generated for each
   return statement.  Args are same as for FUNCTION_PROLOGUE.

   The function epilogue should not depend on the current stack pointer!
   It should use the frame pointer only.  This is mandatory because
   of alloca; we also take advantage of it to omit stack adjustments
   before returning.  */

#undef	FUNCTION_EPILOGUE
#define FUNCTION_EPILOGUE(FILE, SIZE) ppcasm_output_epilog (FILE, SIZE)

/* This macro generates the assembly code for function entry.
   FILE is a stdio stream to output the code to.
   SIZE is an int: how many units of temporary storage to allocate.
   Refer to the array `regs_ever_live' to determine which registers
   to save; `regs_ever_live[I]' is nonzero if register number I
   is ever used in the function.  This macro is responsible for
   knowing which registers should not be saved even if used.  */

#undef	FUNCTION_PROLOGUE
#define FUNCTION_PROLOGUE(FILE, SIZE) ppcasm_output_prolog (FILE, SIZE)

/* Names to predefine in the preprocessor for this target machine.  */ 
#undef  CPP_PREDEFINES
#define CPP_PREDEFINES "-D__ppc__ -D__NATURAL_ALIGNMENT__ -D_POWER -D__BIG_ENDIAN__ -D__APPLE__ -Asystem(MacOS) -Acpu(powerpc) -Amachine(powerpc)"

/* Text to write out after a CALL that may be replaced by glue code by
   the loader. */

#undef	RS6000_CALL_GLUE
#define RS6000_CALL_GLUE "nop"

/* This is how to output a reference to a user-level label named NAME.
   `assemble_name' uses this.  PPCAsm version needs to strip '[RW]' etc
   at end of name.  Quel pain!! */

#undef	ASM_OUTPUT_LABELREF
#define ASM_OUTPUT_LABELREF(FILE,NAME)  \
  ppcasm_output_labelref(FILE, NAME)	/*fputs (NAME, FILE)*/

/* This is how to output the definition of a user-level label named NAME,
   such as the label on a static function or variable NAME.  */

#undef ASM_OUTPUT_LABEL
#define ASM_OUTPUT_LABEL(FILE,NAME)	\
  do { assemble_name (FILE, NAME); fputs (":\n", FILE); } while (0)

/* The default -arch flag in the driver */

#undef	DEFAULT_TARGET_ARCH
#define DEFAULT_TARGET_ARCH "ppc"

#undef  TARGET_DEFAULT
#define TARGET_DEFAULT (MASK_POWERPC | MASK_MULTIPLE | MASK_STRING | \
 MASK_NEW_MNEMONICS | MASK_NO_FP_IN_TOC | MASK_NO_SUM_IN_TOC)

#undef	ASM_COMMENT_START
#define	ASM_COMMENT_START	"\t;;"

#undef	ASM_OUTPUT_SOURCE_FILENAME
#define	ASM_OUTPUT_SOURCE_FILENAME(FILE, NAME)	\
	fprintf(FILE, "%s '%s'\n\n", ASM_COMMENT_START, NAME)

#undef	ASM_OUTPUT_OPTIONS
#define	ASM_OUTPUT_OPTIONS(FILE)	/*nothing*/

/* Make strings be ASIS and dialect PowerPC */
#define	PPCASM_FILE_PROLOG(FILE) \
	fprintf(FILE, "\tDIALECT\tPowerPC\n\tSTRING\tASIS\n\n")

#undef	ASM_FILE_START
#define ASM_FILE_START(FILE)                                    \
{                                                               \
  PPCASM_FILE_PROLOG(FILE);					\
  rs6000_gen_section_name (&xcoff_bss_section_name,             \
                           main_input_filename, "_bss_");       \
  rs6000_gen_section_name (&xcoff_private_data_section_name,    \
                           main_input_filename, "_rw_");        \
  rs6000_gen_section_name (&xcoff_read_only_section_name,       \
                           main_input_filename, "_ro_");        \
                                                                \
  output_file_directive (FILE, main_input_filename);            \
  toc_section ();                                               \
  if (write_symbols != NO_DEBUG)                                \
    private_data_section ();                                    \
  text_section ();                                              \
  if (profile_flag)                                             \
    fprintf (FILE, "\tIMPORT %s\n", RS6000_MCOUNT);            \
  rs6000_file_start (FILE, TARGET_CPU_DEFAULT);                 \
}

/* Output at end of assembler file.

   On the RS/6000, referencing data should automatically pull in text.  */

/* Changed for PPCAsm to just emit an END directive. */

#undef	ASM_FILE_END
#define ASM_FILE_END(FILE) fputs("\n\tEND\n", FILE)
		
#undef	EXTRA_SECTION_FUNCTIONS
#define EXTRA_SECTION_FUNCTIONS				\
							\
void							\
read_only_data_section ()				\
{							\
  if (in_section != read_only_data)			\
    {							\
      fprintf (asm_out_file, "\tCSECT\t%s[RO]\n",		\
	       xcoff_read_only_section_name);		\
      in_section = read_only_data;			\
    }							\
}							\
							\
void							\
private_data_section ()					\
{							\
  if (in_section != private_data)			\
    {							\
      fprintf (asm_out_file, "\tCSECT\t%s[RW]\n",		\
	       xcoff_private_data_section_name);	\
							\
      in_section = private_data;			\
    }							\
}							\
							\
void							\
read_only_private_data_section ()			\
{							\
  if (in_section != read_only_private_data)		\
    {							\
      fprintf (asm_out_file, "\tCSECT\t%s[RO]\n",		\
	       xcoff_private_data_section_name);	\
      in_section = read_only_private_data;		\
    }							\
}							\
							\
void							\
toc_section ()						\
{							\
  if (TARGET_MINIMAL_TOC)				\
    {							\
      static int toc_initialized = 0;			\
							\
      /* toc_section is always called at least once from ASM_FILE_START, \
	 so this is guaranteed to always be defined once and only once   \
	 in each file.  */						 \
      if (! toc_initialized)				\
	{						\
	  fprintf(asm_out_file,"\tTOC\nLCTOC%s0:\n",INTERNAL_LABEL_SUBSTRING);\
	  fputs ("\tTC\ttoc_table[TC],toc_table[RW]\n", asm_out_file); \
	  toc_initialized = 1;				\
	}						\
							\
      if (in_section != toc)				\
	fputs ("\tCSECT\ttoc_table[RW]\n", asm_out_file); \
    }							\
  else							\
    {							\
      if (in_section != toc)				\
        fputs ("\tTOC\n", asm_out_file);		\
    }							\
  in_section = toc;					\
}


/* This macro produces the initial definition of a function name.
   On the RS/6000, we need to place an extra '.' in the function name and
   output the function descriptor.

   The csect for the function will have already been created by the
   `text_section' call previously done.  We do have to go back to that
   csect, however.  */

/* Changed to use MacOS-style TOC entries (no third zero and each function
   with its own CSECT, allowing the linker to dead code strip. */

#undef	ASM_DECLARE_FUNCTION_NAME
#define ASM_DECLARE_FUNCTION_NAME(FILE,NAME,DECL)               \
{ if (TREE_PUBLIC (DECL))                                       \
    {                                                           \
      fputs ("\tEXPORT\t.", FILE);                               \
      RS6000_OUTPUT_BASENAME (FILE, NAME);                      \
      if (!strcmp (NAME, "main"))				\
	fprintf (FILE, "[PR]");					\
    }                                                           \
  else                                                          \
    {                                                           \
	/* no need to make something "locally global" */	\
      /*fputs ("\t.lglobl .", FILE);*/                              \
      /*RS6000_OUTPUT_BASENAME (FILE, NAME);*/                      \
    }                                                           \
  putc ('\n', FILE);                                            \
  fputs ("\tCSECT\t", FILE);                                      \
  RS6000_OUTPUT_BASENAME (FILE, NAME);                          \
  fputs ("[DS]\n", FILE);                                       \
  RS6000_OUTPUT_BASENAME (FILE, NAME);                          \
  fputs (":\n", FILE);                                          \
  fputs ((TARGET_32BIT) ? "\tDC.L\t." : "\tDC.LL\t.", FILE);    \
  RS6000_OUTPUT_BASENAME (FILE, NAME);                          \
  fputs (", TOC[tc0] ;; , 0 <- not used on Mac \n", FILE);      \
								\
  /*-->		fputs ("\tCSECT\t.text[PR]\n.", FILE); */	\
  /*-->		RS6000_OUTPUT_BASENAME (FILE, NAME); */		\
  /*-->		fputs (":\n", FILE); */				\
								\
  /*<-- */	fputs ("\tCSECT\t.", FILE);			\
  /*<-- */	RS6000_OUTPUT_BASENAME (FILE, NAME);		\
  /*<-- */	fputs ("[PR]\n.", FILE);			\
  /*<-- */	RS6000_OUTPUT_BASENAME (FILE, NAME);            \
  /*<-- */	putc ('\n', FILE);				\
								\
  if (write_symbols == XCOFF_DEBUG)                             \
    xcoffout_declare_function (FILE, DECL, NAME);               \
}

/* Output something to declare an external symbol to the assembler.  Most
   assemblers don't need this.

   If we haven't already, add "[RW]" (or "[DS]" for a function) to the
   name.  Normally we write this out along with the name.  In the few cases
   where we can't, it gets stripped off.  */

#undef	ASM_OUTPUT_EXTERNAL
#define ASM_OUTPUT_EXTERNAL(FILE, DECL, NAME)   \
{ rtx _symref = XEXP (DECL_RTL (DECL), 0);      \
  if ((TREE_CODE (DECL) == VAR_DECL             \
       || TREE_CODE (DECL) == FUNCTION_DECL)    \
      && (NAME)[strlen (NAME) - 1] != ']')      \
    {                                           \
      char *_name = (char *) permalloc (strlen (XSTR (_symref, 0)) + 5); \
      strcpy (_name, XSTR (_symref, 0));        \
      strcat (_name, TREE_CODE (DECL) == FUNCTION_DECL ? "[DS]" : "[RW]"); \
      XSTR (_symref, 0) = _name;                \
    }                                           \
  fputs ("\tIMPORT\t", FILE);                   \
  assemble_name (FILE, XSTR (_symref, 0));      \
  if (TREE_CODE (DECL) == FUNCTION_DECL)        \
    {                                           \
      fputs ("\n\tIMPORT\t.", FILE);            \
      RS6000_OUTPUT_BASENAME (FILE, XSTR (_symref, 0)); \
    }                                           \
  putc ('\n', FILE);                            \
}

/* Similar, but for libcall.  We only have to worry about the function name,
   not that of the descriptor. */

#undef	ASM_OUTPUT_EXTERNAL_LIBCALL
#define ASM_OUTPUT_EXTERNAL_LIBCALL(FILE, FUN)  \
{ fputs ("\tIMPORT\t.", FILE);                  \
  assemble_name (FILE, XSTR (FUN, 0));          \
  putc ('\n', FILE);                            \
}

/* Output before instructions.  */

#undef	TEXT_SECTION_ASM_OP
#define TEXT_SECTION_ASM_OP "\tCSECT\t.text[PR]"

/* Output before writable data.  */

#undef	DATA_SECTION_ASM_OP
#define DATA_SECTION_ASM_OP "\tCSECT\t.data[RW]"

#undef REGISTER_NAMES
#define REGISTER_NAMES \
 {"r0", "SP", "RTOC", "r3", "r4", "r5", "r6", "r7", 		\
  "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",		\
  "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",	\
  "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",	\
  "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",		\
  "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15",		\
  "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",	\
  "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31",	\
  "mq", "lr", "ctr", "ap",					\
  "cr0", "cr1", "cr2", "cr3", "cr4", "cr5", "cr6", "cr7",	\
  "fpmem" }

#undef ASM_GLOBALIZE_LABEL
#define ASM_GLOBALIZE_LABEL(FILE,NAME)		\
  do { fputs ("\tEXPORT\t", FILE);		\
       RS6000_OUTPUT_BASENAME (FILE, NAME);	\
       if (!strcmp (NAME, "main"))		\
	 fprintf (FILE, "[DS]");		\
       fputs ("\n", FILE);} while (0)

/* PPCAsm only wants [A-Z][a-z][0-9] in identifier names.
   rs6000.h has labels of the form LC..<number>, change to LC_i_<number>. */

#define INTERNAL_LABEL_SUBSTRING        "_i_"

#undef  ASM_OUTPUT_INTERNAL_LABEL
#define ASM_OUTPUT_INTERNAL_LABEL(FILE,PREFIX,NUM)      \
        fprintf (FILE, "%s%s%d:\n", PREFIX, INTERNAL_LABEL_SUBSTRING, NUM)

#undef  ASM_OUTPUT_INTERNAL_LABEL_PREFIX
#define ASM_OUTPUT_INTERNAL_LABEL_PREFIX(FILE,PREFIX)   \
  fprintf (FILE, "%s%s", PREFIX, INTERNAL_LABEL_SUBSTRING)

#undef  ASM_GENERATE_INTERNAL_LABEL
#define ASM_GENERATE_INTERNAL_LABEL(LABEL,PREFIX,NUM)   \
  sprintf (LABEL, "*%s%s%d", PREFIX, INTERNAL_LABEL_SUBSTRING, NUM)


#define	PPCASM_FLOAT_HACK	1	/* PPCAsm floats are weird. */

/* This is how to output an assembler line defining a `double' constant.  */

#undef	ASM_OUTPUT_DOUBLE
#define ASM_OUTPUT_DOUBLE(FILE, VALUE)                                  \
  {                                                                     \
    if (PPCASM_FLOAT_HACK || REAL_VALUE_ISINF (VALUE)                                        \
        || REAL_VALUE_ISNAN (VALUE)                                     \
        || REAL_VALUE_MINUS_ZERO (VALUE))                               \
      {                                                                 \
        long t[2];                                                      \
        REAL_VALUE_TO_TARGET_DOUBLE ((VALUE), t);                       \
        fprintf (FILE, "\tDC.L\t0x%08lx, 0x%08lx\n",                \
                t[0] & 0xffffffff, t[1] & 0xffffffff);                  \
      }                                                                 \
    else                                                                \
      {                                                                 \
        char str[30];                                                   \
        REAL_VALUE_TO_DECIMAL (VALUE, "%.20e", str);                    \
        fprintf (FILE, "\t.double 0d%s\n", str);                        \
      }                                                                 \
  }

/* This is how to output an assembler line defining a `float' constant.  */

#undef	ASM_OUTPUT_FLOAT
#define ASM_OUTPUT_FLOAT(FILE, VALUE)                                   \
  {                                                                     \
    if (PPCASM_FLOAT_HACK || REAL_VALUE_ISINF (VALUE)                                        \
        || REAL_VALUE_ISNAN (VALUE)                                     \
        || REAL_VALUE_MINUS_ZERO (VALUE))                               \
      {                                                                 \
        long t;                                                         \
        REAL_VALUE_TO_TARGET_SINGLE ((VALUE), t);                       \
        fprintf (FILE, "\tDC.L\t0x%08lx\n", t & 0xffffffff);              \
      }                                                                 \
    else                                                                \
      {                                                                 \
        char str[30];                                                   \
        REAL_VALUE_TO_DECIMAL ((VALUE), "%.20e", str);                  \
        fprintf (FILE, "\t.float 0d%s\n", str);                         \
      }                                                                 \
  }

/* This is how to output an assembler line defining an `int' constant.  */

#undef	ASM_OUTPUT_DOUBLE_INT
#define ASM_OUTPUT_DOUBLE_INT(FILE,VALUE)                               \
do {                                                                    \
  if (TARGET_32BIT)                                                     \
    {                                                                   \
      assemble_integer (operand_subword ((VALUE), 0, 0, DImode),        \
                        UNITS_PER_WORD, 1);                             \
      assemble_integer (operand_subword ((VALUE), 1, 0, DImode),        \
                        UNITS_PER_WORD, 1);                             \
    }                                                                   \
  else                                                                  \
    {                                                                   \
      fputs ("\tDC.LL\t", FILE);                                        \
      output_addr_const (FILE, (VALUE));                                \
      putc ('\n', FILE);                                                \
    }                                                                   \
} while (0)

/* Macro to output a special constant pool entry.  Go to WIN if we output
   it.  Otherwise, it is written the usual way.

   On the RS/6000, toc entries are handled this way.  */

#undef	ASM_OUTPUT_SPECIAL_POOL_ENTRY
#define ASM_OUTPUT_SPECIAL_POOL_ENTRY(FILE, X, MODE, ALIGN, LABELNO, WIN)  \
{ if (ASM_OUTPUT_SPECIAL_POOL_ENTRY_P (X))	\
    {						\
      ppcasm_output_toc (FILE, X, LABELNO);		\
      goto WIN;					\
    }						\
}

#undef	ASM_OUTPUT_INT
#define ASM_OUTPUT_INT(FILE,VALUE)  \
( fputs ("\tDC.L\t", FILE),                     \
  output_addr_const (FILE, (VALUE)),            \
  putc ('\n', FILE))

/* Likewise for `char' and `short' constants.  */

#undef	ASM_OUTPUT_SHORT
#define ASM_OUTPUT_SHORT(FILE,VALUE)  \
( fputs ("\tDC.W\t", FILE),                    \
  output_addr_const (FILE, (VALUE)),            \
  putc ('\n', FILE))

#undef	ASM_OUTPUT_CHAR
#define ASM_OUTPUT_CHAR(FILE,VALUE)  \
( fputs ("\tDC.B\t", FILE),                     \
  output_addr_const (FILE, (VALUE)),            \
  putc ('\n', FILE))

/* This is how to output an assembler line for a numeric constant byte.  */

#undef	ASM_OUTPUT_BYTE
#define ASM_OUTPUT_BYTE(FILE,VALUE)  \
  fprintf (FILE, "\tDC.B\t0x%x\n", (VALUE))

/* This is how to output an assembler line to define N characters starting
   at P to FILE.  */

#undef	ASM_OUTPUT_ASCII
#define ASM_OUTPUT_ASCII(FILE, P, N)  ppcasm_output_ascii ((FILE), (P), (N))

/* This is how to output an element of a case-vector that is absolute.
   (RS/6000 does not use such vectors, but we must define this macro
   anyway.)   */

#undef	ASM_OUTPUT_ADDR_VEC_ELT
#define ASM_OUTPUT_ADDR_VEC_ELT(FILE, VALUE)            \
  do { char buf[100];                                   \
       fputs ((TARGET_32BIT) ? "\tDC.L\t" : "\tDC.LL\t", FILE); \
       ASM_GENERATE_INTERNAL_LABEL (buf, "L", VALUE);   \
       assemble_name (FILE, buf);                       \
       putc ('\n', FILE);                               \
     } while (0)

/* This is how to output an element of a case-vector that is relative.  */

#undef	ASM_OUTPUT_ADDR_DIFF_ELT
#define ASM_OUTPUT_ADDR_DIFF_ELT(FILE, VALUE, REL)      \
  do { char buf[100];                                   \
       fputs ((TARGET_32BIT) ? "\tDC.L\t" : "\tDC.LL\t", FILE); \
       ASM_GENERATE_INTERNAL_LABEL (buf, "L", VALUE);   \
       assemble_name (FILE, buf);                       \
       fputs(" - ", FILE);                                \
       ASM_GENERATE_INTERNAL_LABEL (buf, "L", REL);     \
       assemble_name (FILE, buf);                       \
       putc ('\n', FILE);                               \
     } while (0)

/* This is how to output an assembler line
   that says to advance the location counter
   to a multiple of 2**LOG bytes.  */

#undef	ASM_OUTPUT_ALIGN
#define ASM_OUTPUT_ALIGN(FILE,LOG)      \
  if ((LOG) != 0)                       \
    fprintf (FILE, ((LOG) > 2) ? "\tALIGN\t2\t;; was %d, too big\n" : \
				"\tALIGN\t%d\n", (LOG))

#undef	ASM_OUTPUT_SKIP
#define ASM_OUTPUT_SKIP(FILE,SIZE)  \
  fprintf (FILE, "\tDS.B\t%d\n", (SIZE))

/* PPCAsm handles COMM and LCOMM differently from as.  Inhibit COMM usage. */

#undef	CC1_SPEC
#define	CC1_SPEC	"-fno-common"
#undef	CC1PLUS_SPEC
#define	CC1PLUS_SPEC	"-fno-common"


/* This says how to output an assembler line
   to define a global common symbol.  */

#undef	ASM_OUTPUT_ALIGNED_COMMON
#define ASM_OUTPUT_ALIGNED_COMMON(FILE, NAME, SIZE, ALIGNMENT)  \
  do { fputs ("\tCOMM\t", (FILE));                      \
       RS6000_OUTPUT_BASENAME ((FILE), (NAME));         \
       if ( 0 ) /* ((SIZE) > 4) */                      \
         fprintf ((FILE), ",%d,3\n", (SIZE));           \
       else                                             \
         fprintf( (FILE), ", %d\n", (SIZE));             \
  } while (0)

/* This says how to output an assembler line
   to define a local common symbol.  */
#undef	ASM_OUTPUT_LOCAL
#define ASM_OUTPUT_LOCAL(FILE, NAME, SIZE,ROUNDED)      \
  do { fputs ("\tLCOMM\t", (FILE));                     \
       fprintf ((FILE), "%d, ", (SIZE));		\
       RS6000_OUTPUT_BASENAME ((FILE), (NAME));         \
       /*fprintf ((FILE), ",%d,%s\n", (SIZE), xcoff_bss_section_name);*/ \
       putc ('\n', FILE);				\
     } while (0)
/* FORGET IT -- TRY USING DS.B for ASM_OUTPUT_LOCAL */

#undef  ASM_OUTPUT_LOCAL
#define ASM_OUTPUT_LOCAL(FILE, NAME, SIZE,ROUNDED)      \
  do {  private_data_section();				\
	RS6000_OUTPUT_BASENAME ((FILE), (NAME));	\
	fprintf ((FILE), " DS.B %d\n", (SIZE));		\
     } while (0)

/* Store in OUTPUT a string (made with alloca) containing
   an assembler-name for a local static variable named NAME.
   LABELNO is an integer which is different for each call.  */

#undef	ASM_FORMAT_PRIVATE_NAME
#define ASM_FORMAT_PRIVATE_NAME(OUTPUT, NAME, LABELNO)  \
( (OUTPUT) = (char *) alloca (strlen ((NAME)) + 24),    \
  sprintf ((OUTPUT), "__private_%s%s%d", (NAME), INTERNAL_LABEL_SUBSTRING, (LABELNO)))

  /* Output something to inform GDB that this compilation was by GCC.
     Unfortunately this was "gcc2_compiled.", i.e., with a period as the
     last character in the label name, which won't work under PPCAsm. */

#define	ASM_IDENTIFY_GCC(FILE)	fprintf(FILE, "gcc2_compiled:\n")

/* Handle the mac68k alignment pragmas, etc. */

#define	APPLE_MAC68K_ALIGNMENT	1
#undef  HANDLE_PRAGMA
#define HANDLE_PRAGMA(finput, tree) handle_pragma (finput, tree)

/* Prototypes for our functions in macos.c */

extern void ppcasm_output_ascii(), ppcasm_output_prolog(), 
			ppcasm_output_epilog(), ppcasm_output_toc(),
			ppcasm_output_labelref();

extern int handle_pragma();

