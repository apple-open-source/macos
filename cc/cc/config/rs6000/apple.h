/* Target definitions for GNU compiler for PowerPC running Rhapsody.
   Copyright 1997 Apple Computer, Inc. (unpublished)  */

/* The macro NEXT_SEMANTICS is supposed to indicate that the *host*
   is OPENSTEP on Mach or Rhapsody, though currently this macro
   is commonly used to simply flag changes we have made to FSF's code.  */
#define NEXT_SEMANTICS

/* The macro APPLE_RHAPSODY is used to indicate that the *target* is Rhapsody.
   FSF doesn't like use of macros that indicate what the target is,
   so if we hope to send patches to FSF, we'll have to change uses
   of this macro to ones that describe particular features.  */
#define APPLE_RHAPSODY 1
#define DEFAULT_ABI ABI_RHAPSODY
#define RS6000_ABI_NAME "Apple Rhapsody"

#define MACHO_PIC 1

/* PIC temp stuff */
#define PIC_OFFSET_TABLE_REGNUM	31

/* Base register for access to local variables of the function.  */
#define FRAME_POINTER_REGNUM 30

/* For now, we never want to generate TOC-based code.  */
#define TARGET_NO_TOC		1
#define	TARGET_TOC		0

/* Macros related to the switches that specify default alignment of fields
   within structs.  */
#define MASK_ALIGN_MAC68K	0x200000
#define TARGET_ALIGN_MAC68K	(target_flags & MASK_ALIGN_MAC68K)

/* Macros related to the switches which enable or disable generation
   of fused multiply-add instructions.  */
#define MASK_FUSED_MADD		0x400000
#define TARGET_FUSED_MADD	(target_flags & MASK_FUSED_MADD)

/* Macros related to the switches which enable or disable generation
   of code that assumes that functions that are called by name can be
   arbitrarily far away from the call site.  */
#define MASK_LONG_BRANCH	0x800000
#define TARGET_LONG_BRANCH	(target_flags & MASK_LONG_BRANCH)

#define SUBTARGET_SWITCHES						\
  {"align-mac68k",	MASK_ALIGN_MAC68K},				\
  {"no-align-mac68k",	- MASK_ALIGN_MAC68K},				\
  {"align-power",	- MASK_ALIGN_MAC68K},				\
  {"no-align-power",	MASK_ALIGN_MAC68K},				\
  {"fused-madd",	MASK_FUSED_MADD},				\
  {"no-fused-madd",	- MASK_FUSED_MADD},				\
  {"long-branch",	MASK_LONG_BRANCH},				\
  {"no-long-branch",	- MASK_LONG_BRANCH},

/* Sometimes certain combinations of command options do not make sense
   on a particular target machine.  You can define a macro
   `OVERRIDE_OPTIONS' to take account of this.  This macro, if
   defined, is executed once just after all the command options have
   been parsed.

   The macro SUBTARGET_OVERRIDE_OPTIONS is provided for subtargets, to
   get control.  */

#define SUBTARGET_OVERRIDE_OPTIONS					\
do {									\
  extern int flag_dave_indirect;					\
  if (flag_dave_indirect) flag_pic = 2;					\
  if (flag_pic == 2) flag_no_function_cse = 1;				\
  if (TARGET_LONG_BRANCH && flag_pic)					\
    error ("-mlong-branch can only be used in combination with -static."); \
  if (TARGET_ALIGN_MAC68K) maximum_field_alignment = 16;		\
} while (0)

#include "rs6000/rs6000.h"
#include "next/nextstep.h"

#define TARGET_ARCHITECTURE \
  { { "ppc", 0 },		/* Accept.  */	       		\
    { "m98k", 0 },		/* Accept.  */	       		\
    { "ppc-nomult", -MASK_MULTIPLE },	/*   */	\
    { "ppc-nostr", -MASK_STRING }}

/* The default -arch flag in the driver */

#define DEFAULT_TARGET_ARCH "ppc"

#undef  TARGET_DEFAULT
#define TARGET_DEFAULT (MASK_POWERPC | MASK_MULTIPLE | MASK_STRING | \
 MASK_NEW_MNEMONICS | MASK_NO_FP_IN_TOC | MASK_NO_SUM_IN_TOC | MASK_FUSED_MADD)

/* Enable code that assumes that functions that are called by name can be
   arbitrarily far away from the call site.  */

#define RS6000_LONG_BRANCH
#define OUTPUT_COMPILER_STUB

/* No parameters are declared because rtl.h hasn't been #include'd yet
   at this point.  */

char *output_call ();

/* Add the following switches as temporary work arounds for some bugs.  */
#define CC1_SPEC \
 "%{!static:%{!fno-PIC:%{!fPIC:-fPIC}} %{O:-fforce-mem}%{O1:-fforce-mem}%{O3:-fno-inline-functions}} -mno-string"
/* According to the NEWS file, -fhandle-exceptions is known to be buggy in
   conjunction with -O.  One can certainly avoid a lot of problems by disabling
   inlining when -fhandle-exceptions is present on the command line.  */
#define CC1PLUS_SPEC "%{fhandle-exceptions:-fno-inline -fkeep-inline-functions} -fsquangle"

#undef SELECT_RTX_SECTION
#define SELECT_RTX_SECTION(mode, rtx) const_section()

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

/* Boundary (in *bits*) on which stack pointer should be aligned.  */

#undef	STACK_BOUNDARY
#define STACK_BOUNDARY 128

/* Names to predefine in the preprocessor for this target machine.  */
#undef CPP_PREDEFINES
#ifdef MAC_OS_X_SERVER_1_0
#define CPP_PREDEFINES "-Dppc -DNATURAL_ALIGNMENT -DNeXT -Dunix -D__MACH__ -D__BIG_ENDIAN__ -D__ARCHITECTURE__=\"ppc\" -D__APPLE__"
#elif defined (MACOSX)
#define CPP_PREDEFINES "-D__ppc__ -D__NATURAL_ALIGNMENT__ -D__MACH__ -D__BIG_ENDIAN__ -D__APPLE__"
#else /* predefined macros in Mac OS X Server 1.1 */
#define CPP_PREDEFINES "-Dppc -D__NATURAL_ALIGNMENT__ -D__MACH__ -D__BIG_ENDIAN__ -D__ARCHITECTURE__=\"ppc\" -D__APPLE__"
#endif

#undef JUMP_TABLES_IN_TEXT_SECTION

#undef ASM_DECLARE_FUNCTION_NAME

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
  "mq", "lr", "ctr", "ap",					\
  "cr0", "cr1", "cr2", "cr3", "cr4", "cr5", "cr6", "cr7",	\
  "fpmem" }

/* The number of a register (other than zero) that can always be clobbered
   by a called function without saving it first.  */

#define ORDINARY_REG_NO (11)

/* The number of the first register that will be used if profiling to save
   parameters passed in general purpose registers before calling mcount.
   (r30 and r31 are reserved for the frame pointer and PIC base,
   respectively.)  */

#define RS6000_LAST_REG (frame_pointer_needed ? 29 : (flag_pic ? 30 : 31))

/* Specify the cost of a branch insn; roughly the number of extra insns that
   should be added to avoid a branch.

   Set this to 1 as a work around for a bug (probably in the jump or cse phase)
   that causes code to be erroneously eliminated as dead code.  */

#undef  BRANCH_COST
#define BRANCH_COST 1

/* Structs and unions are always returned in memory.  */

#undef DEFAULT_PCC_STRUCT_RETURN
#define DEFAULT_PCC_STRUCT_RETURN 1

/* This macro gets just the user-specified name out of the string
   in a SYMBOL_REF.  We don't need to do anything fancy,
   so we discard the * if any and that's all.  */

#undef STRIP_NAME_ENCODING
#define STRIP_NAME_ENCODING(VAR,SYMBOL_NAME) \
  (VAR) = ((SYMBOL_NAME) + ((SYMBOL_NAME)[0] == '*'))

/* Output at beginning of assembler file.
   For now, catch all (erroneous) uses of registers r2 and r13.  */

#undef	ASM_FILE_START
#define ASM_FILE_START(FILE) \
  fprintf (FILE, ".macro mul\n\tcror 0, 0, 0\n\tcror 0, 0, 0\n\tcror 0, 0, 0\n\tmul$0 $1,$2,$3\n.endmacro\n.macro div\n\tcror 0, 0, 0\n\tcror 0, 0, 0\n\tcror 0, 0, 0\n\tdiv$0 $1,$2,$3\n.endmacro\n")

/* This outputs NAME to FILE.  */

#undef  RS6000_OUTPUT_BASENAME
#define RS6000_OUTPUT_BASENAME(FILE, NAME)	\
    assemble_name ((FILE), (NAME));

/* This is how to output the definition of a user-level label named NAME,
   such as the label on a static function or variable NAME.  */

#undef ASM_OUTPUT_LABEL
#define ASM_OUTPUT_LABEL(FILE,NAME)	\
  do { assemble_name (FILE, NAME); fputs (":\n", FILE); } while (0)

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

#undef ASM_OUTPUT_ASCII
#define ASM_OUTPUT_ASCII(FILE, P, N)  apple_output_ascii ((FILE), (P), (N))
extern void apple_output_ascii ();

/* Make sure jump tables have the same alignment as other pointers.  */

#undef  ASM_OUTPUT_CASE_LABEL
#define ASM_OUTPUT_CASE_LABEL(FILE,PREFIX,NUM,TABLEINSN)	\
{ ASM_OUTPUT_ALIGN (FILE, 1); ASM_OUTPUT_INTERNAL_LABEL (FILE, PREFIX, NUM); }

/* This says how to output an assembler line
   to define a global common symbol.  */

#undef  ASM_OUTPUT_ALIGNED_COMMON
#define ASM_OUTPUT_COMMON(FILE, NAME, SIZE, ROUNDED)	\
  do { fputs (".comm ", (FILE));			\
       RS6000_OUTPUT_BASENAME ((FILE), (NAME));		\
       fprintf ((FILE), ",%d\n", (SIZE)); } while (0)

/* Don't treat addresses involving labels differently from symbol names.
   Previously, references to labels generated pc-relative addressing modes
   while references to symbol names generated absolute addressing modes.  */

#undef	GO_IF_INDEXABLE_BASE(X, ADDR)
#define GO_IF_INDEXABLE_BASE(X, ADDR)	\
{  if (LEGITIMATE_BASE_REG_P (X)) goto ADDR; }

/* This accounts for the return pc and saved fp on the m68k. */

#define OBJC_FORWARDING_STACK_OFFSET 8
#define OBJC_FORWARDING_MIN_OFFSET 8

/* Define cutoff for using external functions to save floating point.
   Currently on Rhapsody, always use inline stores */

#undef	FP_SAVE_INLINE
#define FP_SAVE_INLINE(FIRST_REG) ((FIRST_REG) < 64)

/* Nonzero if the constant value X is a legitimate general operand
   when generating PIC code.  It is given that flag_pic is on and 
   that X satisfies CONSTANT_P or is a CONST_DOUBLE.  */

/* For now, punt when processing C++ source code, since it is unknown whether
   all symbols that are actually defined in the source file being processed
   are marked as such.  So pretend that all operands are legitimate; otherwise,
   it's not possible to build libg++'s gperf.  */

extern char *language_string;
#define LEGITIMATE_PIC_OPERAND_P(X)	\
  (! symbolic_operand (X)		\
   || machopic_operand_p (X)		\
   || ((GET_CODE(X) == SYMBOL_REF) &&	\
       (SYMBOL_REF_FLAG(X) || !strcmp (language_string, "GNU C++"))))

/* Output before instructions.  */

#undef TEXT_SECTION_ASM_OP
#define TEXT_SECTION_ASM_OP ".text"

/* Output before writable data.  */

#undef DATA_SECTION_ASM_OP
#define DATA_SECTION_ASM_OP ".data"

/* Turn off FP constants in TOC */
#undef ASM_OUTPUT_SPECIAL_POOL_ENTRY

/* Turn off TOC reload slot following calls */
#undef RS6000_CALL_GLUE
#define RS6000_CALL_GLUE ""


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

/* Assembler defaults symbols to external except those starting with L */
#undef ASM_OUTPUT_EXTERNAL
#undef ASM_OUTPUT_EXTERNAL_LIBCALL

#undef DEFAULT_SIGNED_CHAR
#define DEFAULT_SIGNED_CHAR (1)

#undef ASM_OUTPUT_FLOAT
#define ASM_OUTPUT_FLOAT(FILE,VALUE)		\
  fprintf (FILE, "\t.single 0d%.20e\n", (VALUE))

/* Not clear if we need to override ASM_OUTPUT_ADDR_VEC_ELT and 
 * ASM_OUTPUT_ADDR_DIFF_ELT from rs6000.h as was done in cc-218.
 */

/* No data type wants to be aligned rounder than this.  */
#undef  BIGGEST_ALIGNMENT
#define BIGGEST_ALIGNMENT (TARGET_32BIT ? 32 : 64)

#undef STRICT_ALIGNMENT
#define STRICT_ALIGNMENT 1

/* This works around a bug in rs6000.h that causes the compiler to crash
   when processing empty structs with no fields.  */
#undef ROUND_TYPE_ALIGN

#define RELOAD_PIC_REGISTER reload_ppc_pic_register()

/* More PIC temp stuff */
#define INITIALIZE_PIC initialize_pic ()
#undef  FINALIZE_PIC
#define FINALIZE_PIC finalize_pic ()
#define SMALL_INT(X) ((unsigned) (INTVAL(X) + 0x4000) < 0x8000)

#if 0
/* Output assembler code for a block containing the constant parts
   of a trampoline, leaving space for the variable parts.

   The trampoline should set the static chain pointer to value placed
   into the trampoline and should branch to the specified routine.

   On the RS/6000, this is not code at all, but merely a data area,
   since that is the way all functions are called.  The first word is
   the address of the function, the second word is the TOC pointer (r2),
   and the third word is the static chain value.  */

#undef  TRAMPOLINE_TEMPLATE
#define TRAMPOLINE_TEMPLATE(FILE) { fprintf (FILE, "\t.long 0, 0, 0, 0\n"); }
#endif

/* Length in units of the trampoline for entering a nested function.
   This keeps stack optimally aligned, and prevents an unresolved symbol from
   cropping up when linking with libgcc.  */

#undef  TRAMPOLINE_SIZE
#define TRAMPOLINE_SIZE 16

/* Emit RTL insns to initialize the variable parts of a trampoline.
   FNADDR is an RTX for the address of the function's pure code.
   CXT is an RTX for the static chain value for the function.  */

#define INITIALIZE_TRAMPOLINE(ADDR, FNADDR, CXT)		\
  rs6000_initialize_trampoline (ADDR, FNADDR, CXT)
