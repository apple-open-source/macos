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

#include "sparc/sparc.h"

#define USE_GAS

#define NEXT_OBJC_RUNTIME

#define NEXT_PDO


/* 
 * overrides for the funky places we put stuff
 *	there was a bug in the beta where the /usr was not prefixed
 */
#undef  GPLUSPLUS_INCLUDE_DIR
#define GPLUSPLUS_INCLUDE_DIR "/usr/NextDeveloper/Headers/g++"
#undef	INCLUDE_DEFAULTS
#define INCLUDE_DEFAULTS				\
  {							\
    { GPLUSPLUS_INCLUDE_DIR, 1},			\
    { GCC_INCLUDE_DIR, 0},				\
    { TOOL_INCLUDE_DIR, 0},				\
    { LOCAL_INCLUDE_DIR, 0},				\
    { "/usr/NextDeveloper/Headers", 0},			\
    { "/usr/NextDeveloper/Headers/ansi", 0},		\
    { "/usr/LocalDeveloper/Headers", 0},		\
    { "/usr/LocalDeveloper/Headers/ansi", 0},		\
    { STANDARD_INCLUDE_DIR, 0},				\
    { 0, 0}						\
  }

/* RIPPED OFF FROM nextstep.h */

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
	else if (!strncmp (NAME, "_OBJC_MODULES", 13)) \
		fprintf (FILE, "%s", NAME);   \
	else if (!strncmp (NAME, "_OBJC_", 6)) \
		fprintf (FILE, "L%s", NAME);   \
	else if (!strncmp (NAME, ".objc_class_name_", 17))		\
		fprintf (FILE, "%s", NAME);				\
	else fprintf ((FILE), "_%s", NAME); } while (0)


#undef LIB_SPEC
#define LIB_SPEC "%{!p:%{!pg:-L/usr/NextDeveloper/lib %{!nopdo:-lpdo} -lc -lm}}%{p:-L/usr/NextDeveloper/lib %{!nopdo:-lpdo} -lc_p -lm_p}%{pg:-L/usr/NextDeveloper/lib %{!nopdo:-lpdo} -lc_p -lm_p}"

/* Provide required defaults for linker -e and -d switches.  */

#undef LINK_SPEC
#define LINK_SPEC \
 "%{!nostdlib:%{!r*:%{!e*:-e start}}} -dc -dp %{static|pg|p:-Bstatic} %{assert*}"

#undef WORD_SWITCH
#define WORD_SWITCH(STR) \
  (WORD_SWITCH_TAKES_ARG (STR) \
   || !strcmp (STR, "bsd") \
   || !strcmp (STR, "object") \
   || !strcmp (STR, "ObjC") \
   || !strcmp (STR, "all_load"))

#undef WORD_SWITCH_TAKES_ARG
#define WORD_SWITCH_TAKES_ARG(STR)				\
  (DEFAULT_WORD_SWITCH_TAKES_ARG (STR)				\
   || !strcmp (STR, "target") 					\
   || !strcmp (STR, "assert")					\
   || !strcmp (STR, "arch")					\
   || !strcmp (STR, "filelist")					\
   || !strcmp (STR, "framework"))


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
  "-Dsparc -Dsun -Dunix -D__GCC_NEW_VARARGS__ -DNeXT_PDO -D__BIG_ENDIAN__ -D__ARCHITECTURE__=\"sparc\" \
   -Asystem(unix) -Asystem(bsd) -Acpu(sparc) -Amachine(sparc)"

#undef TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (sparc-nextpdo-sunos4.1.3)");

#undef SELECT_SECTION
#define SELECT_SECTION(T,RELOC)						\
{									\
  if (TREE_CODE (T) == VAR_DECL)					\
    {									\
      if (TREE_READONLY (T) && ! TREE_SIDE_EFFECTS (T)			\
	  && DECL_ALIGN (T) <= MAX_TEXT_ALIGN				\
	  && ! (flag_pic && (RELOC))                                    \
          && strncmp(IDENTIFIER_POINTER (DECL_NAME (T)), "_OBJC_", 6))	\
	text_section ();						\
      else								\
	data_section ();						\
    }									\
  else if (TREE_CODE (T) == CONSTRUCTOR				        \
	   && TREE_TYPE (T)					        \
	   && TREE_CODE (TREE_TYPE (T)) == RECORD_TYPE		        \
	   && TYPE_NAME (TREE_TYPE (T))				        \
	   && TREE_CODE (TYPE_NAME (TREE_TYPE (T))) == IDENTIFIER_NODE  \
	   && IDENTIFIER_POINTER (TYPE_NAME (TREE_TYPE (T))))           \
     {                                                                  \
       if ((!strcmp (IDENTIFIER_POINTER (TYPE_NAME (TREE_TYPE (T))),    \
		    "NSConstantString")) ||				\
	   (!strcmp (IDENTIFIER_POINTER (TYPE_NAME (TREE_TYPE (T))),    \
		    "NXConstantString")) ||				\
	   (!strcmp (IDENTIFIER_POINTER (TYPE_NAME (TREE_TYPE (T))),    \
		     "_objc_protocol")))                                \
	  data_section ();				                \
       else if ((TREE_READONLY (T) || TREE_CONSTANT (T))		\
		&& !TREE_SIDE_EFFECTS (T))				\
	  text_section ();					        \
       else								\
	  data_section ();						\
      }									\
  else if (TREE_CODE (T) == CONSTRUCTOR)				\
    {									\
      if (flag_pic != 0 && (RELOC) != 0)				\
	data_section ();						\
    }									\
  else if (*tree_code_type[(int) TREE_CODE (T)] == 'c')			\
    {									\
      if ((TREE_CODE (T) == STRING_CST && flag_writable_strings)	\
	  || TYPE_ALIGN (TREE_TYPE (T)) > MAX_TEXT_ALIGN)		\
	data_section ();						\
      else								\
	text_section ();						\
    }									\
}

#define NEXT_FRAMEWORKS_DEFAULT			\
    {"/usr/LocalLibrary/Frameworks", 0},	\
    {"/usr/NextLibrary/Frameworks", 0}, 


#undef TARGET_DEFAULT
#define TARGET_DEFAULT (MASK_APP_REGS + MASK_EPILOGUE + MASK_FPU + MASK_HARD_QUAD)

