/* Target definitions for GNU compiler for PowerPC running Mac OS X.
   Copyright 1997 Apple Computer, Inc. (unpublished)  */

/* The macro NEXT_SEMANTICS is supposed to indicate that the *host*
   is OPENSTEP on Mach or Mac OS X, though currently this macro
   is commonly used to simply flag changes we have made to FSF's code.  */
#define NEXT_SEMANTICS

#define DEFAULT_ABI ABI_MACOSX
#define RS6000_ABI_NAME "Apple Mac OS X"

#define MACHO_PIC 1

/* ZOE -- zero overhead exceptions.  We support DWARF2_UNWIND_INFO -- or
   rather, enough of it to support exception unwinding.  */

#define DWARF2_UNWIND_INFO	1

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
/* Compatibility mode with Mac OS X release 1.0.   */
#define MASK_ALIGN_MACOSX1	0x400000
#define TARGET_ALIGN_MACOSX1	(target_flags & MASK_ALIGN_MACOSX1)

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
  {"align-macosx1",	MASK_ALIGN_MACOSX1},				\
  {"no-align-macosx1",	- (MASK_ALIGN_MAC68K | MASK_ALIGN_MACOSX1)},	\
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

#if 0
/* For Mac OS X, we can use R2 as a volatile register.  */

#define FIXED_R2  0             /* R2 is not a FIXED reg.  */
#define ALLOC_R2_AS_VOLATILE 2, /* allocate R2 as a volatile reg, please.  */
#define ALLOC_R2_AS_FIXED       /* DON'T alloc R2 as a fixed reg.  */
#endif

#include "rs6000/rs6000.h"
#include "apple/openstep.h"

#define TARGET_ARCHITECTURE \
  { { "ppc", 0 },		/* Accept.  */	       		\
    { "m98k", 0 },		/* Accept.  */	       		\
    { "ppc-nomult", -MASK_MULTIPLE },	/*   */	\
    { "ppc-nostr", -MASK_STRING }}

/* The default -arch flag in the driver */

#define DEFAULT_TARGET_ARCH "ppc"

#undef  TARGET_DEFAULT
#define TARGET_DEFAULT (MASK_POWERPC | MASK_MULTIPLE | MASK_NEW_MNEMONICS \
 | MASK_NO_FP_IN_TOC | MASK_NO_SUM_IN_TOC)

/* Enable code that assumes that functions that are called by name can be
   arbitrarily far away from the call site.  */

#define RS6000_LONG_BRANCH
#define OUTPUT_COMPILER_STUB

/* No parameters are declared because rtl.h hasn't been #include'd yet
   at this point.  */

char *output_call ();

/* Add -fforce-mem and -fno-inline-functions as temporary work arounds
   for some bugs.  */
#define CC1_SPEC \
 "%{!static:-fPIC %{O:-fforce-mem}%{O1:-fforce-mem}%{O3:-fno-inline-functions}} -Wno-four-char-constants%{fdump-syms: -fsyntax-only}"

#define CC1PLUS_SPEC "-fnew-abi -fno-honor-std"

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
#elif defined (MACOSX) || defined (RC_RELEASE_Flask)
#define CPP_PREDEFINES "-D__ppc__ -D__NATURAL_ALIGNMENT__ -D__MACH__ -D__BIG_ENDIAN__ -D__APPLE__"
#else /* predefined macros in Mac OS X Server 1.1 */
#define CPP_PREDEFINES "-Dppc -D__NATURAL_ALIGNMENT__ -D__MACH__ -D__BIG_ENDIAN__ -D__ARCHITECTURE__=\"ppc\" -D__APPLE__"
#endif

/* Removed for coalesced symbols:  */
/* #undef JUMP_TABLES_IN_TEXT_SECTION  */

/* We need to define the size of the JMP_BUF used (see except.c), otherwise
   gcc will use FIRST_PSEUDO_REGISTER, which is completely wrong for AltiVec.

   This comment is from "/usr/include/ppc/setjmp.h":

	_JBLEN is number of ints required to save the following:
	r1, r2, r13-r31, lr, cr, ctr, xer, sig  == 26 ints
	fr14 -  fr31 = 18 doubles = 36 ints
	vmask, 32 vector registers = 129 ints
	2 ints to get all the elements aligned

   The above comment is WRONG, I think -- we only need to save 12 vector
   registers (v20-v31).  */

#define JMP_BUF_SIZE (26 + 36 + 129 + 4)

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
  "fpmem", "vrsave",						\
  "v0",  "v1",  "v2",  "v3",  "v4",  "v5",  "v6",  "v7",	\
  "v8",  "v9", "v10", "v11", "v12", "v13", "v14", "v15",	\
  "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",	\
  "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31"	\
}


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

/* Define the default processor.  */

#undef  PROCESSOR_DEFAULT
#define PROCESSOR_DEFAULT PROCESSOR_PPC750

/* Output at beginning of assembler file.  */

#undef	ASM_FILE_START
#define ASM_FILE_START(FILE) \
  if (rs6000_cpu == PROCESSOR_PPC604 || rs6000_cpu == PROCESSOR_PPC604e) \
    fprintf (FILE, ".macro mul\n\tcror 0, 0, 0\n\tcror 0, 0, 0\n\tcror 0, 0, 0\n\tmul$0 $1,$2,$3\n.endmacro\n.macro div\n\tcror 0, 0, 0\n\tcror 0, 0, 0\n\tcror 0, 0, 0\n\tdiv$0 $1,$2,$3\n.endmacro\n"); \
  else fprintf (FILE, ".macro mul\n\tmul$0 $1,$2,$3\n.endmacro\n.macro div\n\tdiv$0 $1,$2,$3\n.endmacro\n")

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
{ ASM_OUTPUT_ALIGN (FILE, 2); ASM_OUTPUT_INTERNAL_LABEL (FILE, PREFIX, NUM); }

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

/* Inline saving of vector registers: save code bloat by saving inline if
   there's only ONE register to save.  */
#undef VECTOR_SAVE_INLINE
#define VECTOR_SAVE_INLINE(FIRST_REG)	((FIRST_REG) == 109)

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
       (SYMBOL_REF_FLAG(X) || !strcmp (language_string, "GNU C++") \
       || !strcmp (language_string, "GNU Obj-C++"))))

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

/* For AltiVec, BIGGEST_ALIGNMENT is 128.
   ROUND_TYPE_ALIGN is required for doubleword alignment of
   structures which have a 'double' as their first member.

   So I've commented out the redefinitions of all these; the definitions
   in "rs6000.h" are (more) correct.  */

/* No data type wants to be aligned rounder than this.  */
#undef  BIGGEST_ALIGNMENT
#define BIGGEST_ALIGNMENT (flag_altivec ? 128 : 32)
extern int flag_altivec;

#undef ADJUST_FIELD_ALIGN
/* AIX word-aligns FP doubles but doubleword-aligns 64-bit ints.
   <tur29May99> Make ADJUST_FIELD_ALIGN align on 16-byte boundary
                when we've got a vector. 
   <tur15Jul99> Because of the kernel 32-bit alignment problems, only
		align vector fields to COMPUTED, otherwise 32-bits. 
   <FF27Jun01>  Simplified in alignment clean up.  */
#define ADJUST_FIELD_ALIGN(FIELD, COMPUTED)		\
  (((COMPUTED) == RS6000_VECTOR_ALIGNMENT)		\
   ? RS6000_VECTOR_ALIGNMENT				\
   : MIN ((COMPUTED), 32))

#undef ROUND_TYPE_ALIGN
/* AIX increases natural record alignment to doubleword if the first
   field is an FP double while the FP fields remain word aligned.
   <tur28May99> Adjusted so vectors are similarly treated (a la MrC.)
   <FF27Jun01>  Fixed many incompatibilities with OS 9 alignment  */

extern int round_type_align (union tree_node*, int, int); /* macosx.c  */
#define ROUND_TYPE_ALIGN(STRUCT, COMPUTED, SPECIFIED)	\
  round_type_align(STRUCT, COMPUTED, SPECIFIED)

/* For stor-layout.c : no matter what MAXIMUM_FIELD_ALIGNMENT is set to,
   vectors *MUST* have 128-bit alignment.  */

#define __PIN_ALIGN_TO_MAX(DESIRED)			\
	(((DESIRED) == RS6000_VECTOR_ALIGNMENT) ? RS6000_VECTOR_ALIGNMENT : \
		MIN ((DESIRED), (unsigned) maximum_field_alignment))
 
/* Define to add warnings for mismatched alignment and sizes of records using
   the new rules and the older OS X 10.0.x rules.  */

#define APPLE_ALIGN_CHECK
#ifdef APPLE_ALIGN_CHECK
/* warn_osx1_size_align - 1: warn about struct size changes,
			  2: warn about field alignment changes.  */
extern int warn_osx1_size_align;
extern unsigned get_type_size_as_int (const union tree_node *);
extern int type_has_different_size_in_osx1 (const union tree_node *);
extern const char *get_type_name_string (const union tree_node *);
extern void apple_align_check (union tree_node *rec, unsigned const_size,
			int osx1_size_delta, union tree_node *var_size);
#endif
/* Optional warnings for sub-optimal field alignment.  For OS X, misaligned
   floating point fields will cause horrendously expensive kernel traps.  */

#define WARN_POOR_FIELD_ALIGN(FIELD, ALIGN)				\
	do {								\
	  tree t = (TREE_CODE (TREE_TYPE (FIELD)) == ARRAY_TYPE)	\
		? get_inner_array_type (FIELD) : TREE_TYPE (FIELD);	\
	  if ((TYPE_MODE (t) == SFmode && ALIGN % 32 != 0)		\
	     || (TYPE_MODE (t) == DFmode && ALIGN % 32 != 0))		\
	    warning_with_decl (FIELD, "`%s' has non-optimal alignment"	\
		" (%d bytes, should be 4)", (ALIGN % 32) / 8);		\
	} while (0)


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

/* An attempt to ensure that we do things right in machopic.c.
   This is called from varasm.c, assemble_start_function(), with
   the decl being written to the asm file, as well as the name by
   which that decl is known.  */

extern void machopic_define_decl (); /* machopic.c  */

#define MACHOPIC_DEFINE_DECL(decl, name) machopic_define_decl (decl, name)

/* Vector stuff: most vector insn patterns look like "%3 %0,%1,%2"
   where operands[3] is a SYMBOL_REF with the name of the *opcode*.
   To stop OUTPUT_OPERAND () prepending an underscore to what it
   (legitimately) believes is a symbol operand, we have to special-case
   outputting the opcode ourselves.  Luckily gcc has this mechanism...

   STREAM is the asm file.  PTR is the char* representing the pattern.  */

#define ASM_OUTPUT_OPCODE(STREAM, PTR)					\
  do {if (PTR[0] == '%' && PTR[1] >= '0' && PTR[1] <= '9'		\
	  && GET_CODE (recog_operand[PTR[1]-'0']) == SYMBOL_REF) {	\
	  fputs (XSTR (recog_operand[PTR[1]-'0'], 0), STREAM);		\
	PTR += 2; /* skip the "%3 " bit */ }} while (0)

/* Given an rtx X being reloaded into a reg required to be      
   in class CLASS, return the class of reg to actually use.     
   In general this is just CLASS; but on some machines
   in some cases it is preferable to use a more restrictive class.
  
   On the RS/6000, we have to return NO_REGS when we want to reload a
   floating-point CONST_DOUBLE to force it to be copied to memory.

   Don't allow R0 when loading the address of, or otherwise furtling with,
   a SYMBOL_REF.  */

#undef PREFERRED_RELOAD_CLASS
#define PREFERRED_RELOAD_CLASS(X,CLASS)					\
  ((GET_CODE (X) == CONST_DOUBLE					\
    && GET_MODE_CLASS (GET_MODE (X)) == MODE_FLOAT)			\
   ? NO_REGS								\
   : (GET_CODE (X) == SYMBOL_REF || GET_CODE (X) == HIGH)		\
     ? BASE_REGS : (CLASS))


/* Compute the cost of computing a constant rtl expression RTX    
   whose rtx-code is CODE.  The body of this macro is a portion
   of a switch statement.  If the code is computed here,
   return it with a return statement.  Otherwise, break from the switch.
   
   On the RS/6000, if it is valid in the insn, it is free.  So this
   always returns 0.
   THE ABOVE IS NOT QUITE TRUE.  Constants which are not representable
   in 16 bits -- either upper or lower halves of a word -- are not free.  */
   
#define CONST_S16BIT_P(K) ((unsigned HOST_WIDE_INT) ((K)+0x8000) < 0x10000)
#define CONST_U16BIT_P(K) (((unsigned HOST_WIDE_INT) (K)) < 0x10000)
#define PPC_CONST_IS_FREE_P(K)					\
	((CONST_S16BIT_P (K) || CONST_U16BIT_P (K))		\
	 || (((unsigned HOST_WIDE_INT)(K)) & 0x0000FFFF) == 0)

#undef CONST_COSTS
#define CONST_COSTS(RTX,CODE,OUTER_CODE)			\
  case CONST_INT:						\
    return (PPC_CONST_IS_FREE_P (INTVAL (RTX))) ? 0 : COSTS_N_INSNS (2); \
  case LABEL_REF:						\
  case SYMBOL_REF:						\
  case HIGH:							\
  case CONST:							\
  case CONST_DOUBLE:						\
    return 0;

/* If no code is generated for the exception_receiver or 
   nonlocal_goto_receiver patterns, don't bother emitting them; this
   results in a much nicer jump optimiser pass.  */

#define NO_EH_RECEIVER_CODE_NECESSARY_P()	(flag_pic)

#define EH_CLEANUPS_SEPARATE_SECTION

/* 01Jul2001: Attempt to get EH cleanup code in a separate section.
   I now do this by adding extra note types.  */

#ifdef EH_CLEANUPS_SEPARATE_SECTION

#define EXTRA_NOTE_NAMES	, "NOTE_INSN_EH_CLEANUP_BEG",		\
				  "NOTE_INSN_EH_CLEANUP_END"

/* rtl.h: NOTE_INSN_BASIC_BLOCK is -20.  */
#define NOTE_INSN_EH_CLEANUP_BEG	-21
#define NOTE_INSN_EH_CLEANUP_END	-22

#define NOTE_EH_CLEANUP_BEG_P(INSN)					\
	(GET_CODE (INSN) == NOTE					\
	 && NOTE_LINE_NUMBER (INSN) == NOTE_INSN_EH_CLEANUP_BEG)
#define NOTE_EH_CLEANUP_END_P(INSN)					\
	(GET_CODE (INSN) == NOTE					\
	 && NOTE_LINE_NUMBER (INSN) == NOTE_INSN_EH_CLEANUP_END)

extern void prune_and_graft_eh_cleanup_info ();

/* rs6000_reorg is not used anymore.  */
#undef MACHINE_DEPENDENT_REORG
#define MACHINE_DEPENDENT_REORG(INSNS)	prune_and_graft_eh_cleanup_info (INSNS)

/* Used in final_scan_insn ().  */
extern const char begin_apple_cleanup_section_name_tag[],
		  end_apple_cleanup_section_name_tag[];

#define PREPROCESS_INTERNAL_LABEL(FILE, LABEL)				\
  do {									\
    if (flag_separate_eh_cleanup_section) {				\
      const char *name = LABEL_NAME (LABEL);				\
      if (name == begin_apple_cleanup_section_name_tag			\
	  || name == end_apple_cleanup_section_name_tag)		\
	handle_eh_tagged_label (FILE, LABEL);				\
    }									\
  } while (0)

/* Deliberately screw up shorten-branches if we're in a different section.  */
#define EH_LABEL_ALIGN_VALUE	15
#define LABEL_ALIGN(LABEL)						\
	((LABEL_NAME (LABEL) == begin_apple_cleanup_section_name_tag)	\
		? get_eh_label_align_value (LABEL) : 0)

#undef ASM_OUTPUT_ALIGN
#define ASM_OUTPUT_ALIGN(FILE,LOG)					\
  if ((LOG) != 0 && (LOG) != EH_LABEL_ALIGN_VALUE)			\
    fprintf (FILE, "\t.align %d\n", (LOG))   

#endif	/* EH_CLEANUPS_SEPARATE_SECTION  */

/* Mach-O PPC DWARF2 exception-handling support.  */

#ifdef DWARF2_UNWIND_INFO

#define DWARF_FRAME_RETURN_COLUMN       65

/* "default cfa framesize" -- 80 bytes.  This is the number that
   gets put into the default/initial CIE; functions with different
   framesizes override that in their own prologs.  */

#define RS6000_DEF_CFA_FRAMESIZE	80

/* Mark R13-31,F14-F31,CR2-CR4,LR,V20-V31,VRSAVE as requiring preservation.
   Mark R0-R12,F0-F13,CR0-CR1,CR5-CR7,V0-V19 as initially undefined.

   26Apr01: Mark CR5 (reg 73) as requiring preservation.  It's not supposed
   to be used.

   I am unsure what to do about FLAG_ALTIVEC for now -- is it possible to
   mention vector regs here only when it's on? (tur01Jul2001: Apparently.)  */

#define DWARF2_TARGET_CALLING_CONVENTION()			\
	{							\
	  dwarf2out_same_value_regs (NULL, 13, 31);		\
	  dwarf2out_same_value_regs (NULL, 46, 63);		\
	  dwarf2out_same_value_regs (NULL, 70, 73);		\
	  dwarf2out_same_value_regs (NULL, 65, 65);		\
	  if (flag_altivec) {					\
	    dwarf2out_same_value_regs (NULL, 77, 77);		\
	    dwarf2out_same_value_regs (NULL, 98, 109);		\
	  }							\
	  dwarf2out_undefined_regs (NULL, 0, 12);		\
	  dwarf2out_undefined_regs (NULL, 32, 45);		\
	  dwarf2out_undefined_regs (NULL, 68, 69);		\
	  dwarf2out_undefined_regs (NULL, 74, 75);		\
	  if (!TARGET_POWERPC)	/* MQ  */			\
	    dwarf2out_undefined_regs (NULL, 64, 64);		\
	  if (flag_altivec)					\
	    dwarf2out_undefined_regs (NULL, 78, 97);		\
	  dwarf2out_def_cfa (NULL, 1, RS6000_DEF_CFA_FRAMESIZE);\
	}

/* TARGET_EH_REG_SET_P should return 1 if REG, being restored from a stack
   frame during exception unwinding, requires treatment other than just being
   copied up to the caller's frame or set immediately.  Altivec's VRsave is
   just such a reg as the caller may not have a stack slot, and there is no
   mechanism for gcc to get or set it.  (It's only ever set/restored in
   prologs and epilogs.)

   DO_TARGET_EH_REG_SET should actually do the copy, setting REG immediately.
   Is there an interrupt window problem here?  */
   
#define TARGET_EH_REG_SET_P(REG)	((REG) == 77)
#define DO_TARGET_EH_REG_SET(REG, MEMADDR)			\
	do {							\
	  unsigned long vrs = *(unsigned long *)(MEMADDR);	\
	  if ((REG) != 77) abort ();				\
	  asm ("mtspr VRsave,%0" : /*NOOUTPUTS*/ : "r" (vrs));	\
	} while (0)

/* This vile hack checks if the opcode at PC+LEN-4 is a 'blr'.  Ugh.    
   It's how we determine which FDE table goes with a particular coalesced
   function when there were multiple different-sized copies of that     
   function originally!  The PC and LEN fields are from the FDE; the
   LEN represents the length of the entire function.  So (ahem, cough) we     
   just need to see whether the opcode at PC[LEN-4] is a 'blr'.  This   
   is not foolproof by any means.  */

#define PLAUSIBLE_PC_RANGE(PC, LEN)				\
	((*(unsigned int *)(((char *)(PC))+(size_t)(LEN)-4)) == 0x4e800020)

#if 0
/* debugging unwinding stuff  */
#define TRACE_DWARF2_UNWIND     1 
#undef ASM_COMMENT_START
#define ASM_COMMENT_START ";#"
#endif
#endif	/* DWARF2_UNWIND_INFO  */

#include "apple/embedded.h"
