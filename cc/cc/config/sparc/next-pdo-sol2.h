/* Definitions of target machine for GNU compiler, for Sun SPARC.
   Copyright (C) 1987, 1988, 1989, 1992 Free Software Foundation, Inc.
   Contributed by Michael Tiemann (tiemann@cygnus.com).

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

/* Note that some other tm.h files include this one and then override
   many of the definitions that relate to assembler syntax.  */

#include "sparc/sol2.h"

#define USE_GAS

#define NEXT_OBJC_RUNTIME

#define NEXT_PDO

/* Enable code that handles Apple's Pascal strings.  */

#define PASCAL_STRINGS

/* Enable code that handles four-character constants.  */

#define FOUR_CHAR_CONSTANTS

#undef INIT_SECTION_ASM_OP
#undef CTORS_SECTION_ASM_OP
#undef DTORS_SECTION_ASM_OP

#define INIT_SECTION_ASM_OP	 ".section\t\".init\",#alloc"
#define CTORS_SECTION_ASM_OP     "\t.section\t\".ctors\",#alloc,#execinstr\n"
#define DTORS_SECTION_ASM_OP     "\t.section\t\".dtors\",#alloc,#execinstr\n"

#define INIT_SECTION_PREAMBLE   asm ("restore")

/* 
 * overrides for the funky places we put stuff
 *	there was a bug in the beta where the /usr was not prefixed
 */
#undef  GPLUSPLUS_INCLUDE_DIR
#define GPLUSPLUS_INCLUDE_DIR "/Library/Frameworks/System.framework/Headers/g++"
#undef	INCLUDE_DEFAULTS
#define INCLUDE_DEFAULTS						\
  {									\
    { GPLUSPLUS_INCLUDE_DIR, 1, 1},					\
    { LOCAL_INCLUDE_DIR, 0, 1},						\
    { TOOL_INCLUDE_DIR, 0, 0 },						\
    { GCC_INCLUDE_DIR, 0, 0},						\
    { "/Library/Frameworks/System.framework/Headers", 0},		\
    { "/Library/Frameworks/System.framework/PrivateHeaders", 0, 1},	\
    { STANDARD_INCLUDE_DIR, 0, 0},					\
    { 0, 0, 0}								\
  }

/* Report errors to make application. */

#define REPORT_EVENT(TYPE, NAME, FILE, LINE, MSG, ARG1, ARG2, ARG3)	\
  make_support (TYPE, NAME, FILE, LINE, MSG, ARG1, ARG2, ARG3)
#define V_REPORT_EVENT(TYPE, NAME, FILE, LINE, MSG, AP)			\
  v_make_support (TYPE, NAME, FILE, LINE, MSG, AP)

/* Give methods pretty symbol names on NeXT. */

#undef	OBJC_GEN_METHOD_LABEL
#define OBJC_GEN_METHOD_LABEL(BUF,IS_INST,CLASS_NAME,CAT_NAME,SEL_NAME,NUM) \
  do { if (CAT_NAME)							\
	 sprintf (BUF, "%c[%s(%s) %s]", (IS_INST) ? '-' : '+',		\
		  (CLASS_NAME), (CAT_NAME), (SEL_NAME));		\
       else								\
	 sprintf (BUF, "%c[%s %s]", (IS_INST) ? '-' : '+',		\
		  (CLASS_NAME), (SEL_NAME));				\
     } while (0)

/* Wrap new method names in quotes so the assembler doesn't gag.
   Make Objective-C internal symbols local.  */

/* work around the gnu'ism of the @ at the begining of the symbol name */

#undef	ASM_OUTPUT_LABELREF
#define ASM_OUTPUT_LABELREF(FILE,NAME)	\
  do {  \
	if (NAME[1] == '+' || NAME[1] == '-') \
		fprintf (FILE, "\"%s\"", NAME+1); \
	else if (NAME[0] == '+' || NAME[0] == '-') \
		fprintf (FILE, "\"%s\"", NAME); \
	else if (!strncmp (NAME, "_OBJC_", 6)) \
		fprintf (FILE, "L%s", NAME);   \
	else if (!strncmp (NAME, ".objc_class_name_", 17))		\
		fprintf (FILE, "%s", NAME);				\
	else fprintf ((FILE), "%s", NAME); } while (0)

#define PTRDIFF_TYPE "int"
/* In 2.4 it should work to delete this.
   #define SIZE_TYPE "int"  */

/* Omit frame pointer at high optimization levels.  */
  
#define OPTIMIZATION_OPTIONS(OPTIMIZE) \
{  								\
  if (OPTIMIZE >= 2) 						\
    {								\
      flag_omit_frame_pointer = 1;				\
    }								\
}

#undef WORD_SWITCH_TAKES_ARG
#define WORD_SWITCH_TAKES_ARG(STR)				\
  (DEFAULT_WORD_SWITCH_TAKES_ARG (STR)				\
   || !strcmp (STR, "target") 					\
   || !strcmp (STR, "assert")					\
   || !strcmp (STR, "arch")					\
   || !strcmp (STR, "filelist")					\
   || !strcmp (STR, "framework"))

#undef WORD_SWITCH
#define WORD_SWITCH(STR) \
  (WORD_SWITCH_TAKES_ARG (STR) \
   || !strcmp (STR, "bsd") \
   || !strcmp (STR, "object") \
   || !strcmp (STR, "ObjC") \
   || !strcmp (STR, "all_load"))

#define OBJC_FORWARDING_REG_OFFSET(ISREG, OFF, REGNO) \
  do { OFF =  (4 * ((REGNO) - 24));                   \
       ISREG = 0; } while (0)

/* Names to predefine in the preprocessor for this target machine.  */

/* The GCC_NEW_VARARGS macro is so that old versions of gcc can compile
   new versions, which have an incompatible va-sparc.h file.  This matters
   because gcc does "gvarargs.h" instead of <varargs.h>, and thus gets the
   wrong varargs file when it is compiled with a different version of gcc.  */

#undef CPP_PREDEFINES
#define CPP_PREDEFINES \
  "-Dsparc -Dsun -Dunix -D__GCC_NEW_VARARGS__ -D__svr4__ -D__SVR4 \
   -DNeXT_PDO -D__BIG_ENDIAN__ -D__ARCHITECTURE__=\"sparc\" \
   -Asystem(unix) -Asystem(svr4) -Acpu(sparc) -Amachine(sparc)"

#undef TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (sparc-nextpdo-sunos5.3)");

/* FIXME: This is a workaound for bug in objc-act.c, static strings need
   to be in the writable data section, so now we make everything writable.
 */
#undef CONST_SECTION_ASM_OP
#define CONST_SECTION_ASM_OP	".section\t.data"

/* According to the NEWS file, -fhandle-exceptions is known to be buggy in
   conjunction with -O.  One can certainly avoid a lot of problems by disabling
   inlining when -fhandle-exceptions is present on the command line.  */
#define CC1PLUS_SPEC "%{fhandle-exceptions:-fno-inline -fkeep-inline-functions}"

#undef LIB_SPEC
#define LIB_SPEC \
  "%{compat-bsd:-lucb -lsocket -lnsl -lelf -laio }%{!shared:%{!symbolic:%{!nopdolib:-lpdo }-lnsl -lthread -lsocket -lc -lm -lc}}"

#undef TARGET_DEFAULT
#define TARGET_DEFAULT (MASK_APP_REGS + MASK_EPILOGUE + MASK_FPU + MASK_HARD_QUAD)
