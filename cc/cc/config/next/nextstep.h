/* nextstep.h -- operating system specific defines to be used when
   targeting GCC for NeXTSTEP.
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
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/* Use new NeXT include file search path.
   In a cross compiler with NeXT as target, don't expect
   the host to use Next's directory scheme.  */

#if defined(NeXT) || defined(__APPLE__) || !defined(CROSS_COMPILE)
#undef	INCLUDE_DEFAULTS
#if 0
#define INCLUDE_DEFAULTS				\
  {							\
    { "/NextDeveloper/Headers/g++", 1, 1},		\
    { "/NextDeveloper/Headers", 0},			\
    { "/NextDeveloper/Headers/ansi", 0},		\
    { "/NextDeveloper/Headers/bsd", 0},			\
    { "/LocalDeveloper/Headers", 0, 1},			\
    { "/LocalDeveloper/Headers/ansi", 0},		\
    { "/LocalDeveloper/Headers/bsd", 0},		\
/*  { "/NextDeveloper/2.0CompatibleHeaders", 0}, */	\
    { STANDARD_INCLUDE_DIR, 0},				\
    { "/usr/include/bsd", 0},				\
    { "FakeSystemHeaders", 0},				\
    { 0, 0}						\
  }
#endif /* 0 */
#ifdef OPENSTEP
#define INCLUDE_DEFAULTS						\
  {									\
    { "/NextLibrary/Frameworks/System.framework/PrivateHeaders", 0, 1},	\
    { "/NextLibrary/Frameworks/System.framework/PrivateHeaders/bsd", 0},\
    { "/NextLibrary/Frameworks/System.framework/Headers", 0}, 		\
    { "/NextLibrary/Frameworks/System.framework/Headers/ansi", 0}, 	\
    { "/NextLibrary/Frameworks/System.framework/Headers/bsd", 0}, 	\
    { "/NextDeveloper/Headers/g++", 1, 1},				\
    { "/NextDeveloper/Headers", 0},					\
    { "/NextDeveloper/Headers/ansi", 0},				\
    { "/NextDeveloper/Headers/bsd", 0},					\
    { "/LocalDeveloper/Headers", 0, 1},					\
    { STANDARD_INCLUDE_DIR, 0},						\
    { 0, 0}								\
  }
#elif defined (MACOSX)
#define INCLUDE_DEFAULTS						\
  {									\
    { LOCAL_INCLUDE_DIR "/c++", 1, 1},					\
    { "/System/Library/Frameworks/System.framework/Headers/c++", 1, 1}, \
    { LOCAL_INCLUDE_DIR, 0},						\
    { LOCAL_INCLUDE_DIR "/gcc/darwin/2.7.2.1", 0},			\
    { "/System/Library/Frameworks/System.framework/Headers", 0}, 	\
    { STANDARD_INCLUDE_DIR, 0},						\
    { 0, 0}								\
  }
#elif defined (RC_RELEASE_Flask)
#define INCLUDE_DEFAULTS						\
  {									\
    { LOCAL_INCLUDE_DIR "/c++", 1, 1},					\
    { "/System/Library/Frameworks/System.framework/Headers/c++", 1, 1}, \
    { "/System/Library/Frameworks/System.framework/Headers/g++", 1, 1}, \
    { LOCAL_INCLUDE_DIR, 0},						\
    { "/System/Library/Frameworks/System.framework/PrivateHeaders", 0, 1}, \
    { "/System/Library/Frameworks/System.framework/PrivateHeaders/bsd", 0},\
    { "/System/Library/Frameworks/System.framework/Headers", 0}, 	\
    { STANDARD_INCLUDE_DIR, 0},						\
    { 0, 0}								\
  }
#else /* !OPENSTEP && !MACOSX && !RC_RELEASE_Flask - probably Mac OS X Server */
#define INCLUDE_DEFAULTS						\
  {									\
    { "/System/Library/Frameworks/System.framework/Headers/g++", 1, 1}, \
    { "/System/Library/Frameworks/System.framework/PrivateHeaders", 0, 1}, \
    { "/System/Library/Frameworks/System.framework/PrivateHeaders/bsd", 0},\
    { "/System/Library/Frameworks/System.framework/Headers", 0}, 	\
    { STANDARD_INCLUDE_DIR, 0},						\
    { LOCAL_INCLUDE_DIR, 0},						\
    { 0, 0}								\
  }
#endif /* !OPENSTEP */
#define REL3COMPAT_INCLUDE_DEFAULTS			\
  {							\
    { "/LocalDeveloper/3.xCompatibleHeaders", 0}, 	\
    { "/LocalDeveloper/3.xCompatibleHeaders/ansi", 0}, 	\
    { "/LocalDeveloper/3.xCompatibleHeaders/bsd", 0}, 	\
    { "/NextDeveloper/3.xCompatibleHeaders", 0}, 	\
    { "/NextDeveloper/3.xCompatibleHeaders/ansi", 0}, 	\
    { "/NextDeveloper/3.xCompatibleHeaders/bsd", 0}, 	\
    { "/NextDeveloper/Headers/g++", 1, 1},		\
    { "/LocalDeveloper/Headers", 0}, 			\
    { "/NextDeveloper/Headers", 0}, 			\
    { 0, 0}						\
  }
#endif /* !CROSS_COMPILE */

/* Report errors to make application. */

#define REPORT_EVENT(TYPE, NAME, FILE, LINE, MSG, ARG1, ARG2, ARG3)	\
  make_support (TYPE, NAME, FILE, LINE, MSG, ARG1, ARG2, ARG3)
#define V_REPORT_EVENT(TYPE, NAME, FILE, LINE, MSG, AP)			\
  v_make_support (TYPE, NAME, FILE, LINE, MSG, AP)
  
#undef	EXTRA_FORMAT_FUNCTIONS
#define EXTRA_FORMAT_FUNCTIONS \
      "NXPrintf",	FALSE,	2,	FALSE,	\
      "NXScanf",	TRUE,	2,	FALSE,	\
      "NXVPrintf",	FALSE,	2,	TRUE,	\
      "NXVScanf",	TRUE,	2,	TRUE,	\
      "DPSPrintf",	FALSE,	2,	FALSE,	\
      "bsd_sprintf",	FALSE,	2,	FALSE,	\
      "bsd_vsprintf",	FALSE,	2,	TRUE,

/* Make the compiler look here for standard stuff */

#undef STANDARD_EXEC_PREFIX
#ifdef OPENSTEP
#define STANDARD_EXEC_PREFIX "/lib/"
#else
#define STANDARD_EXEC_PREFIX "/usr/libexec/"
#endif

/* Name of the command that invokes the compiler - used in g++.c.  */

#undef  GCC_NAME
#define GCC_NAME "cc"

/* Never try linking with -lm - used in g++.c.  */

#define NO_MATH_LIBRARY

/* Make -fnext-runtime the default.  */

#define NEXT_OBJC_RUNTIME

/* Support -arch xx flags */

#define NEXT_FAT_OUTPUT

/* support the precompiled header cpp */

#define NEXT_CPP_PRECOMP

/* Define NO_IMPLICIT_EXTERN_C if all system header files support C++ as well
   as C (i.e., they contain `extern "C" { ... }' constructs where
   necessary, to prevent the C++ compiler from mangling the names of
   system functions).  On Rhapsody & Openstep, we still need to use the
   formerly-common GNU method of handling system header files in C++, which
   is for the compiler to pretend that the file's contents are enclosed in
   `extern "C" { ... }' constructs.  */

/* #define NO_IMPLICIT_EXTERN_C */

/* change semantics of things around the compiler... */

#define NEXT_SEMANTICS

/* make the library functions in libgcc have only one underbar
   in front of them.  */

#define NEXT_LIBGCC_NAMES

/* Enable code that understands "modern" Objective C syntax.  */

#define MODERN_OBJC_SYNTAX

/* Enable code that processes pragmas controlling how fields are aligned
   within structures.  */

#define APPLE_MAC68K_ALIGNMENT

/* Enable code that handles Apple's Pascal strings.  */

#define PASCAL_STRINGS

/* Enable code that handles four-character constants.  */

#define FOUR_CHAR_CONSTANTS

/* We have atexit.  */

#define HAVE_ATEXIT

/* Define an empty body for the function do_global_dtors() in libgcc2.c.  */

#define DO_GLOBAL_DTORS_BODY

/* Enable recent gcc to compile under the old gcc in Next release 1.0.  */

#define __inline inline

/* The string value for __SIZE_TYPE__.  */

#ifndef SIZE_TYPE
#define SIZE_TYPE "long unsigned int"
#endif

/* Type used for ptrdiff_t, as a string used in a declaration.  */

#undef  PTRDIFF_TYPE
#define PTRDIFF_TYPE "int"

#ifdef OPENSTEP
/* wchar_t is unsigned short */

#undef	WCHAR_TYPE
#define WCHAR_TYPE "short unsigned int"
#undef	WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 16
#else
/* wchar_t is int.  */

#undef	WCHAR_TYPE
#define WCHAR_TYPE "int"
#undef	WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 32
#endif

/* Don't default to pcc-struct-return, because gcc is the only compiler, and
   we want to retain compatibility with older gcc versions.  */

#undef	DEFAULT_PCC_STRUCT_RETURN
#define DEFAULT_PCC_STRUCT_RETURN 0

/* These compiler options take n arguments.  */

#undef	WORD_SWITCH_TAKES_ARG
#ifdef OPENSTEP
#define WORD_SWITCH_TAKES_ARG(STR)	 	\
  (DEFAULT_WORD_SWITCH_TAKES_ARG (STR) ? 1 :	\
   !strcmp (STR, "read_only_relocs") ? 1 :	\
   !strcmp (STR, "segalign") ? 1 :		\
   !strcmp (STR, "seg1addr") ? 1 :		\
   !strcmp (STR, "undefined") ? 1 :		\
   !strcmp (STR, "dylib_file") ? 1 :		\
   !strcmp (STR, "segaddr") ? 2 :		\
   !strcmp (STR, "sectobjectsymbols") ? 2 :	\
   !strcmp (STR, "segprot") ? 3 :		\
   !strcmp (STR, "sectcreate") ? 3 :		\
   !strcmp (STR, "sectalign") ? 3 :		\
   !strcmp (STR, "segcreate") ? 3 :		\
   !strcmp (STR, "sectorder") ? 3 :		\
   !strcmp (STR, "siff-mask") ? 1 :		\
   !strcmp (STR, "siff-filter") ? 1 :		\
   !strcmp (STR, "siff-warning") ? 1 :		\
   !strcmp (STR, "arch") ? 1 :			\
   !strcmp (STR, "NEXTSTEP-deployment-target") ? 1 : \
   !strcmp (STR, "pagezero_size") ? 1 :		\
   !strcmp (STR, "dylinker_install_name") ? 1 :	\
   !strcmp (STR, "framework") ? 1 :		\
   !strcmp (STR, "filelist") ? 1 :		\
   !strcmp (STR, "image_base") ? 1 :		\
   !strcmp (STR, "install_name") ? 1 :		\
   !strcmp (STR, "arch_only") ? 1 :		\
   !strcmp (STR, "compatibility_version") ? 1 :	\
   !strcmp (STR, "current_version") ? 1 :	\
   !strcmp (STR, "init") ? 1 :			\
   0)
#else
#define WORD_SWITCH_TAKES_ARG(STR)	 	\
  (DEFAULT_WORD_SWITCH_TAKES_ARG (STR) ? 1 :	\
   !strcmp (STR, "read_only_relocs") ? 1 :	\
   !strcmp (STR, "segalign") ? 1 :		\
   !strcmp (STR, "seg1addr") ? 1 :		\
   !strcmp (STR, "undefined") ? 1 :		\
   !strcmp (STR, "dylib_file") ? 1 :		\
   !strcmp (STR, "segaddr") ? 2 :		\
   !strcmp (STR, "sectobjectsymbols") ? 2 :	\
   !strcmp (STR, "segprot") ? 3 :		\
   !strcmp (STR, "sectcreate") ? 3 :		\
   !strcmp (STR, "sectalign") ? 3 :		\
   !strcmp (STR, "segcreate") ? 3 :		\
   !strcmp (STR, "sectorder") ? 3 :		\
   !strcmp (STR, "segs_read_only_addr") ? 1 :	\
   !strcmp (STR, "segs_read_write_addr") ? 1 :	\
   !strcmp (STR, "seg_addr_table") ? 1 :	\
   !strcmp (STR, "umbrella") ? 1 :		\
   !strcmp (STR, "siff-mask") ? 1 :		\
   !strcmp (STR, "siff-filter") ? 1 :		\
   !strcmp (STR, "siff-warning") ? 1 :		\
   !strcmp (STR, "arch") ? 1 :			\
   !strcmp (STR, "NEXTSTEP-deployment-target") ? 1 : \
   !strcmp (STR, "pagezero_size") ? 1 :		\
   !strcmp (STR, "dylinker_install_name") ? 1 :	\
   !strcmp (STR, "framework") ? 1 :		\
   !strcmp (STR, "filelist") ? 1 :		\
   !strcmp (STR, "image_base") ? 1 :		\
   !strcmp (STR, "install_name") ? 1 :		\
   !strcmp (STR, "arch_only") ? 1 :		\
   !strcmp (STR, "compatibility_version") ? 1 :	\
   !strcmp (STR, "current_version") ? 1 :	\
   !strcmp (STR, "init") ? 1 :			\
   !strcmp (STR, "header-mapfile") ? 1 :	\
   !strcmp (STR, "precomp-trustfile") ? 1 :	\
   0)
#endif

#undef	WORD_SWITCH
#ifdef OPENSTEP
#define WORD_SWITCH(STR)			\
  (WORD_SWITCH_TAKES_ARG (STR)			\
   || !strcmp (STR, "bsd")			\
   || !strcmp (STR, "object")			\
   || !strcmp (STR, "ObjC")			\
   || !strcmp (STR, "dylinker")			\
   || !strcmp (STR, "dynamic")			\
   || !strcmp (STR, "static")			\
   || !strcmp (STR, "rel3compat")		\
   || !strcmp (STR, "threeThreeMethodEncoding") \
   || !strcmp (STR, "bundle")			\
   || !strcmp (STR, "dynamiclib")		\
   || !strcmp (STR, "output_for_dyld")		\
   || !strcmp (STR, "keep_private_externs")	\
   || !strcmp (STR, "all_load"))
#else
#define WORD_SWITCH(STR)			\
  (WORD_SWITCH_TAKES_ARG (STR)			\
   || !strcmp (STR, "object")			\
   || !strcmp (STR, "ObjC")			\
   || !strcmp (STR, "dylinker")			\
   || !strcmp (STR, "dynamic")			\
   || !strcmp (STR, "static")			\
   || !strcmp (STR, "bundle")			\
   || !strcmp (STR, "dynamiclib")		\
   || !strcmp (STR, "output_for_dyld")		\
   || !strcmp (STR, "keep_private_externs")	\
   || !strcmp (STR, "all_load"))
#endif

/* Machine dependent cpp options.  */

#define STRINGIFY_THIS(x) # x
#define REALLY_STRINGIFY(x) STRINGIFY_THIS(x)

#undef	CPP_SPEC
#ifdef OPENSTEP
#ifndef NX_RELEASE
#define NX_RELEASE REALLY_STRINGIFY(NEXT_RELEASE_MAJOR) \
		   REALLY_STRINGIFY(NEXT_RELEASE_MINOR)
#endif
#define CPP_SPEC "%{!traditional: -D__STDC__}            \
                  %{posixstrict:-D_POSIX_SOURCE}         \
                  %{!posixstrict:%{bsd:-D__STRICT_BSD__} \
                  %{posix:-D_POSIX_SOURCE}               \
                  %{!ansi:-D_NEXT_SOURCE}}               \
    		  %{mdisable-fpregs:-D__NO_FP__}	 \
		  %{F*}	%{ansi} %{fno-asm}	         \
		  %{rel3compat:}		 	 \
		  -DNX_COMPILER_RELEASE_3_0=300          \
		  -DNX_COMPILER_RELEASE_3_1=310          \
		  -DNX_COMPILER_RELEASE_3_2=320          \
		  -DNX_COMPILER_RELEASE_3_3=330          \
		  -DNX_CURRENT_COMPILER_RELEASE=" NX_RELEASE "0" " \
		  -DNS_TARGET=" NX_RELEASE "		 \
		  -DNS_TARGET_MAJOR=" REALLY_STRINGIFY(NEXT_RELEASE_MAJOR) " \
		  -DNS_TARGET_MINOR=" REALLY_STRINGIFY(NEXT_RELEASE_MINOR) " \
		  %{static:-D__STATIC__}%{!static:-D__DYNAMIC__} \
                  %{MD:-MD %M} %{MMD:-MMD %M}"
#else
#define CPP_SPEC "%{!traditional: -D__STDC__}			\
    		  %{mdisable-fpregs:-D__NO_FP__}		\
		  %{F*}	%{ansi} %{fno-asm}			\
		  -D__APPLE_CC__=" REALLY_STRINGIFY(APPLE_CC) "	\
		  %{static:-D__STATIC__}%{!static:-D__DYNAMIC__}\
		  %{malign-mac68k:-U__NATURAL_ALIGNMENT__}	\
		  %{mno-align-power:-U__NATURAL_ALIGNMENT__}	\
                  %{MD:-MD %M} %{MMD:-MMD %M}"
#endif

#define ASM_SPEC \
  "%{I*} %{force_cpusubtype_ALL} %{dynamic}%{static} -arch %T %{@:-arch_multiple}"

/* Machine dependent ld options.  */

#undef	LINK_SPEC
#ifdef OPENSTEP
#define LINK_SPEC "%{Z} %{M} %{F*} \
%{execute*} %{preload*} %{fvmlib*} \
%{segalign*} %{seg1addr*} %{segaddr*} %{segprot*} \
%{pagezero_size*} %{dylib_file*} \
%{seglinkedit*} %{noseglinkedit*} %{read_only_relocs} \
%{sectcreate*} %{sectalign*} %{sectobjectsymbols}\
%{segcreate*} %{Mach*} %{whyload} %{w} \
%{sectorder*} %{whatsloaded} %{ObjC} %{all_load} %{object} \
%{dylinker} %{dylinker_install_name*} %{output_for_dyld} \
%{keep_private_externs} %{prebind} %{noprebind}"
#else
#define LINK_SPEC "%{Z} %{M} %{F*} \
%{execute*} %{preload*} %{fvmlib*} \
%{segalign*} %{seg1addr*} %{segaddr*} %{segprot*} \
%{pagezero_size*} %{dylib_file*} \
%{segs_read_*} %{seg_addr_table*} \
%{seglinkedit*} %{noseglinkedit*} %{read_only_relocs} \
%{sectcreate*} %{sectalign*} %{sectobjectsymbols}\
%{segcreate*} %{Mach*} %{whyload} %{w} \
%{sectorder*} %{whatsloaded} %{ObjC} %{all_load} %{object} \
%{dylinker} %{dylinker_install_name*} %{output_for_dyld} \
%{keep_private_externs} %{prebind} %{noprebind}"
#endif

/* Machine dependent libraries.  */

#undef	LIB_SPEC
#if 0
#define LIB_SPEC "%{!bundle:%{!posix*:%{!pg:-lsys_s}%{pg:-lsys_p}}%{posix*:-lposix}}"
#elif defined (OPENSTEP) && !defined (RC_RELEASE_Grail)
#define LIB_SPEC \
     "%{static:%{!dynamiclib:%{!pg:-lsys_s}%{pg:-lsys_p}}} \
      %{!static:%{!pg:-framework System}%{pg:-framework System,_profile}}"
#else
#define LIB_SPEC \
     "%{!static:%{!pg:-framework System}%{pg:-framework System,_profile}}"
#endif

#undef LIBGCC_SPEC
#define LIBGCC_SPEC "%{!shared:%{static:%{!dynamiclib:-lcc}} \
			      %{!static:-lcc_dynamic}}"

/* We specify crt0.o as -lcrt0.o so that ld will search the library path. */

#undef	STARTFILE_SPEC
#define STARTFILE_SPEC  \
"%{!dynamiclib:%{bundle:%{!static:-lbundle1.o}} \
   %{!bundle:%{pg:%{static:-lgcrt0.o} \
		  %{!static:%{object:-lgcrt0.o} \
			    %{!object:%{preload:-lgcrt0.o} \
				      %{!preload:-lgcrt1.o}}}} \
	    %{!pg:%{static:-lcrt0.o} \
		  %{!static:%{object:-lcrt0.o} \
			    %{!object:%{preload:-lcrt0.o} \
				      %{!preload:-lcrt1.o}}}}}}"

/* Why not? */

#undef	DOLLARS_IN_IDENTIFIERS
#define DOLLARS_IN_IDENTIFIERS 2

/* Allow #sscs (but don't do anything). */

#define SCCS_DIRECTIVE

/* We use Dbx symbol format.  */

#undef	SDB_DEBUGGING_INFO
#undef	XCOFF_DEBUGGING_INFO
#define DBX_DEBUGGING_INFO

/* This saves a fair amount of space. */

#undef	DBX_CONTIN_LENGTH
#define DBX_CONTIN_LENGTH 0

/* These screw up NeXT's gdb at the moment, so don't use them. */

#undef	DBX_OUTPUT_MAIN_SOURCE_DIRECTORY
#define DBX_OUTPUT_MAIN_SOURCE_DIRECTORY(FILE, FILENAME)

/* These come from bsd386.h, but are specific to sequent, so make sure
   they don't bite us.  */

#undef	DBX_NO_XREFS
#undef	DBX_CONTIN_LENGTH

/* gdb needs a null N_SO at the end of each file for scattered loading. */

#undef	DBX_OUTPUT_MAIN_SOURCE_FILE_END
#define DBX_OUTPUT_MAIN_SOURCE_FILE_END(FILE, FILENAME)			\
  fprintf (FILE,							\
	   "\t.text\n\t.stabs \"%s\",%d,0,0,Letext\nLetext:\n",		\
	   "" , N_SO)

/* Don't use .gcc_compiled symbols to communicate with GDB;
   They interfere with numerically sorted symbol lists. */

#undef	ASM_IDENTIFY_GCC
#define ASM_IDENTIFY_GCC(asm_out_file)
#undef	INIT_SECTION_ASM_OP
#define INIT_SECTION_ASM_OP
#undef	INVOKE__main

#define ASM_OUTPUT_ZEROFILL(FILE, NAME, SIZE, ALIGNMENT)  	\
do { fputs (".zerofill __DATA, __common, ", (FILE));		\
        assemble_name ((FILE), (NAME));				\
        fprintf ((FILE), ", %u, %u\n", (SIZE), (ALIGNMENT)); 	\
	in_section = no_section;				\
      } while (0)

#undef	ASM_OUTPUT_CONSTRUCTOR
#define ASM_OUTPUT_CONSTRUCTOR(FILE,NAME)                       \
  do { if (flag_pic) mod_init_section ();                       \
       else constructor_section ();                             \
       ASM_OUTPUT_ALIGN (FILE, 1);                              \
       fprintf (FILE, "\t.long ");                              \
       assemble_name (FILE, NAME);                              \
       fprintf (FILE, "\n");                                    \
       if (!flag_pic)                                    	\
	 fprintf (FILE, ".reference .constructors_used\n");     \
      } while (0)

#undef	ASM_OUTPUT_DESTRUCTOR
#define ASM_OUTPUT_DESTRUCTOR(FILE,NAME)                        \
  do if (!flag_pic) {						\
    destructor_section ();					\
    ASM_OUTPUT_ALIGN (FILE, 1);					\
    fprintf (FILE, "\t.long ");					\
    assemble_name (FILE, NAME);					\
    fprintf (FILE, "\n");					\
    fprintf (FILE, ".reference .destructors_used\n");		\
  } while (0)

#define CALL_DESTRUCTOR_DYNAMICALLY(dfndecl) \
  call_destructor_dynamically (dfndecl)
#define DYNAMIC_DESTRUCTORS (flag_pic != 0)

/* Don't output a .file directive.  That is only used by the assembler for
   error reporting.  */
#undef	ASM_FILE_START
#define ASM_FILE_START(FILE)

#undef	ASM_FILE_END
#define ASM_FILE_END(FILE)					\
  do {								\
    extern char *language_string;				\
    if (strcmp (language_string, "GNU C++") == 0)		\
      {								\
	constructor_section ();					\
	destructor_section ();					\
	ASM_OUTPUT_ALIGN (FILE, 1);				\
      }								\
  } while (0)

/* How to parse #pragma's */

#undef	HANDLE_PRAGMA
#define HANDLE_PRAGMA(finput) handle_pragma (finput, &get_directive_line)

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

#undef	ASM_OUTPUT_LABELREF
#define ASM_OUTPUT_LABELREF(FILE,NAME)	\
  do { if (NAME[0] == '+' || NAME[0] == '-') fprintf (FILE, "\"%s\"", NAME); \
       else if (!strncmp (NAME, "_OBJC_", 6)) fprintf (FILE, "L%s", NAME);   \
       else if (!strncmp (NAME, ".objc_class_name_", 17))		\
	 fprintf (FILE, "%s", NAME);					\
       else fprintf (FILE, "_%s", NAME); } while (0)

#undef	ALIGN_ASM_OP
#define ALIGN_ASM_OP		".align"

#undef	ASM_OUTPUT_ALIGN
#define ASM_OUTPUT_ALIGN(FILE,LOG)	\
  if ((LOG) != 0)			\
    fprintf (FILE, "\t%s %d\n", ALIGN_ASM_OP, (LOG))

/* Ensure correct alignment of bss data.  */

#undef	ASM_OUTPUT_ALIGNED_LOCAL
#define ASM_OUTPUT_ALIGNED_LOCAL(FILE, NAME, SIZE, ALIGN) \
( fputs (".lcomm ", (FILE)),				\
  assemble_name ((FILE), (NAME)),			\
  fprintf ((FILE), ",%u,%u\n", (SIZE), floor_log2 ((ALIGN) / BITS_PER_UNIT)))

/* Output #ident as a .ident.  */

#undef	ASM_OUTPUT_IDENT
#define ASM_OUTPUT_IDENT(FILE, NAME)

/* The maximum alignment which the object file format can support.
   For NeXT's Mach-O format, this is 2^15.  */

#undef	MAX_OFILE_ALIGNMENT
#define MAX_OFILE_ALIGNMENT 0x8000

/* Create new Mach-O sections. */

#undef	SECTION_FUNCTION
#define SECTION_FUNCTION(FUNCTION, SECTION, DIRECTIVE, WAS_TEXT, OBJC)	\
void									\
FUNCTION ()								\
{									\
  extern void text_section ();					 	\
  extern void objc_section_init ();					\
  extern int flag_no_mach_text_sections;				\
  									\
  if (WAS_TEXT && flag_no_mach_text_sections)       			\
    text_section ();							\
  else if (in_section != SECTION)					\
    {									\
      if (OBJC)								\
	objc_section_init ();						\
      data_section ();							\
      fprintf (asm_out_file, "%s\n", DIRECTIVE);			\
      in_section = SECTION;						\
    }									\
}									\

#define ALIAS_SECTION(enum_value, alias_name) 		\
do { if (!strcmp (alias_name, name)) \
        section_alias[enum_value] = alias ? get_identifier (alias) : 0;   \
   } while (0)

#undef	EXTRA_SECTIONS
#define EXTRA_SECTIONS					\
  in_const, in_const_data, in_cstring, in_literal4, in_literal8,	\
  in_constructor, in_destructor, in_mod_init,		\
  in_objc_class, in_objc_meta_class, in_objc_category,	\
  in_objc_class_vars, in_objc_instance_vars,		\
  in_objc_cls_meth, in_objc_inst_meth,			\
  in_objc_cat_cls_meth, in_objc_cat_inst_meth,		\
  in_objc_selector_refs,				\
  in_objc_selector_fixup,				\
  in_objc_symbols, in_objc_module_info,			\
  in_objc_protocol, in_objc_string_object,		\
  in_objc_constant_string_object,			\
  in_objc_class_names, in_objc_meth_var_names,		\
  in_objc_meth_var_types, in_objc_cls_refs, 		\
  in_machopic_nl_symbol_ptr,				\
  in_machopic_lazy_symbol_ptr,				\
  in_machopic_symbol_stub,				\
  in_machopic_picsymbol_stub

#undef	EXTRA_SECTION_FUNCTIONS
#define EXTRA_SECTION_FUNCTIONS			\
SECTION_FUNCTION (const_section,		\
                  in_const,			\
                  ".const", 1, 0)		\
SECTION_FUNCTION (const_data_section,		\
                  in_const_data,		\
                  ".const_data", 1, 0)		\
SECTION_FUNCTION (cstring_section,		\
		  in_cstring,			\
		  ".cstring", 1, 0)		\
SECTION_FUNCTION (literal4_section,		\
		  in_literal4,			\
		  ".literal4", 1, 0)		\
SECTION_FUNCTION (literal8_section,		\
		  in_literal8,			\
		  ".literal8", 1, 0)		\
SECTION_FUNCTION (constructor_section,		\
		  in_constructor,		\
		  ".constructor", 0, 0)		\
SECTION_FUNCTION (mod_init_section,		\
		  in_mod_init,			\
		  ".mod_init_func", 0, 0)	\
SECTION_FUNCTION (destructor_section,		\
		  in_destructor,		\
		  ".destructor", 0, 0)		\
SECTION_FUNCTION (objc_class_section,		\
		  in_objc_class,		\
		  ".objc_class", 0, 1)		\
SECTION_FUNCTION (objc_meta_class_section,	\
		  in_objc_meta_class,		\
		  ".objc_meta_class", 0, 1)	\
SECTION_FUNCTION (objc_category_section,	\
		  in_objc_category,		\
		".objc_category", 0, 1)		\
SECTION_FUNCTION (objc_class_vars_section,	\
		  in_objc_class_vars,		\
		  ".objc_class_vars", 0, 1)	\
SECTION_FUNCTION (objc_instance_vars_section,	\
		  in_objc_instance_vars,	\
		  ".objc_instance_vars", 0, 1)	\
SECTION_FUNCTION (objc_cls_meth_section,	\
		  in_objc_cls_meth,		\
		  ".objc_cls_meth", 0, 1)	\
SECTION_FUNCTION (objc_inst_meth_section,	\
		  in_objc_inst_meth,		\
		  ".objc_inst_meth", 0, 1)	\
SECTION_FUNCTION (objc_cat_cls_meth_section,	\
		  in_objc_cat_cls_meth,		\
		  ".objc_cat_cls_meth", 0, 1)	\
SECTION_FUNCTION (objc_cat_inst_meth_section,	\
		  in_objc_cat_inst_meth,	\
		  ".objc_cat_inst_meth", 0, 1)	\
SECTION_FUNCTION (objc_selector_refs_section,	\
		  in_objc_selector_refs,	\
		  ".objc_message_refs", 0, 1)	\
SECTION_FUNCTION (objc_selector_fixup_section,	\
		  in_objc_selector_fixup,	\
		  ".section __OBJC, __sel_fixup", 0, 1)	\
SECTION_FUNCTION (objc_symbols_section,		\
		  in_objc_symbols,		\
		  ".objc_symbols", 0, 1)	\
SECTION_FUNCTION (objc_module_info_section,	\
		  in_objc_module_info,		\
		  ".objc_module_info", 0, 1)	\
SECTION_FUNCTION (objc_protocol_section,	\
		  in_objc_protocol,		\
		  ".objc_protocol", 0, 1)	\
SECTION_FUNCTION (objc_string_object_section,	\
		  in_objc_string_object,	\
		  ".objc_string_object", 0, 1)	\
SECTION_FUNCTION (objc_constant_string_object_section,	\
		  in_objc_constant_string_object,	\
		  ".section __OBJC, __cstring_object", 0, 1)	\
SECTION_FUNCTION (objc_class_names_section,	\
		in_objc_class_names,		\
		".objc_class_names", 0, 1)	\
SECTION_FUNCTION (objc_meth_var_names_section,	\
		in_objc_meth_var_names,		\
		".objc_meth_var_names", 0, 1)	\
SECTION_FUNCTION (objc_meth_var_types_section,	\
		in_objc_meth_var_types,		\
		".objc_meth_var_types", 0, 1)	\
SECTION_FUNCTION (objc_cls_refs_section,	\
		in_objc_cls_refs,		\
		".objc_cls_refs", 0, 1)		\
						\
SECTION_FUNCTION (machopic_lazy_symbol_ptr_section,	\
		in_machopic_lazy_symbol_ptr,		\
		".lazy_symbol_pointer", 0, 0)      	\
SECTION_FUNCTION (machopic_nl_symbol_ptr_section,	\
		in_machopic_nl_symbol_ptr,		\
		".non_lazy_symbol_pointer", 0, 0)      	\
SECTION_FUNCTION (machopic_symbol_stub_section,		\
		in_machopic_symbol_stub,		\
		".symbol_stub", 0, 0)      		\
SECTION_FUNCTION (machopic_picsymbol_stub_section,	\
		in_machopic_picsymbol_stub,		\
		".picsymbol_stub", 0, 0)      		\
							\
void						\
objc_section_init ()				\
{						\
  static int been_here = 0;			\
						\
  if (been_here == 0)				\
    {						\
      been_here = 1;				\
          /* written, cold -> hot */		\
      objc_cat_cls_meth_section ();		\
      objc_cat_inst_meth_section ();		\
      objc_string_object_section ();		\
      objc_constant_string_object_section ();	\
      objc_selector_refs_section ();		\
      objc_selector_fixup_section ();		\
      objc_cls_refs_section ();			\
      objc_class_section ();			\
      objc_meta_class_section ();		\
          /* shared, hot -> cold */    		\
      objc_cls_meth_section ();			\
      objc_inst_meth_section ();		\
      objc_protocol_section ();			\
      objc_class_names_section ();		\
      objc_meth_var_types_section ();		\
      objc_meth_var_names_section ();		\
      objc_category_section ();			\
      objc_class_vars_section ();		\
      objc_instance_vars_section ();		\
      objc_module_info_section ();		\
      objc_symbols_section ();			\
    }						\
} 						\
static tree section_alias[(int)num_sections];		\
void try_section_alias () 			\
{						\
    if (section_alias[in_section])		\
       fprintf (asm_out_file, "%s\n",		\
              IDENTIFIER_POINTER (section_alias[in_section]));	\
}      						\
void alias_section (name, alias)			\
  char *name, *alias;					\
{							\
    ALIAS_SECTION(in_data, "data");			\
    ALIAS_SECTION(in_text, "text");			\
    ALIAS_SECTION(in_const, "const");			\
    ALIAS_SECTION(in_const_data, "const_data");		\
    ALIAS_SECTION(in_cstring, "cstring");		\
    ALIAS_SECTION(in_literal4, "literal4");		\
    ALIAS_SECTION(in_literal8, "literal8");		\
}

#undef	READONLY_DATA_SECTION
#define READONLY_DATA_SECTION const_section

#undef	SELECT_SECTION
#define SELECT_SECTION(exp,reloc)				\
  do								\
    {								\
      if (TREE_CODE (exp) == STRING_CST)			\
	{							\
	  if (flag_writable_strings)				\
	    data_section ();					\
	  else if (TREE_STRING_LENGTH (exp) !=			\
		   strlen (TREE_STRING_POINTER (exp)) + 1)	\
	    readonly_data_section ();				\
	  else							\
	    cstring_section ();					\
	}							\
      else if (TREE_CODE (exp) == INTEGER_CST			\
	       || TREE_CODE (exp) == REAL_CST)			\
        {							\
	  tree size = TYPE_SIZE (TREE_TYPE (exp));		\
	  							\
	  if (TREE_CODE (size) == INTEGER_CST &&		\
	      TREE_INT_CST_LOW (size) == 4 &&			\
	      TREE_INT_CST_HIGH (size) == 0)			\
	    literal4_section ();				\
	  else if (TREE_CODE (size) == INTEGER_CST &&		\
	      TREE_INT_CST_LOW (size) == 8 &&			\
	      TREE_INT_CST_HIGH (size) == 0)			\
	    literal8_section ();				\
	  else							\
	    readonly_data_section ();				\
	}							\
      else if (TREE_CODE (exp) == CONSTRUCTOR				\
	       && TREE_TYPE (exp)					\
	       && TREE_CODE (TREE_TYPE (exp)) == RECORD_TYPE		\
	       && TYPE_NAME (TREE_TYPE (exp)))				\
	{								\
	  tree name = TYPE_NAME (TREE_TYPE (exp));			\
	  if (TREE_CODE (name) == TYPE_DECL)				\
	    name = DECL_NAME (name);					\
	  if (!strcmp (IDENTIFIER_POINTER (name), "NSConstantString"))	\
	    objc_constant_string_object_section ();			\
	  else if (!strcmp (IDENTIFIER_POINTER (name), "NXConstantString")) \
	    objc_string_object_section ();				\
	  else if ((TREE_READONLY (exp) || TREE_CONSTANT (exp))		\
		&& !TREE_SIDE_EFFECTS (exp))				\
                  { if (flag_pic && reloc ) const_data_section ();      \
                    else readonly_data_section (); }                    \
	  else								\
	    data_section ();						\
      }									\
      else if (TREE_CODE (exp) == VAR_DECL &&				\
	       DECL_NAME (exp) &&					\
	       TREE_CODE (DECL_NAME (exp)) == IDENTIFIER_NODE &&	\
	       IDENTIFIER_POINTER (DECL_NAME (exp)) &&			\
	       !strncmp (IDENTIFIER_POINTER (DECL_NAME (exp)), "_OBJC_", 6)) \
	{								\
	  const char *name = IDENTIFIER_POINTER (DECL_NAME (exp));	\
	  								\
	  if (!strncmp (name, "_OBJC_CLASS_METHODS_", 20))		\
	    objc_cls_meth_section ();					\
	  else if (!strncmp (name, "_OBJC_INSTANCE_METHODS_", 23))	\
	    objc_inst_meth_section ();					\
	  else if (!strncmp (name, "_OBJC_CATEGORY_CLASS_METHODS_", 20)) \
	    objc_cat_cls_meth_section ();				\
	  else if (!strncmp (name, "_OBJC_CATEGORY_INSTANCE_METHODS_", 23)) \
	    objc_cat_inst_meth_section ();				\
	  else if (!strncmp (name, "_OBJC_CLASS_VARIABLES_", 22))	\
	    objc_class_vars_section ();					\
	  else if (!strncmp (name, "_OBJC_INSTANCE_VARIABLES_", 25))	\
	    objc_instance_vars_section ();				\
	  else if (!strncmp (name, "_OBJC_CLASS_PROTOCOLS_", 22))	\
	    objc_cat_cls_meth_section ();				\
	  else if (!strncmp (name, "_OBJC_CLASS_NAME_", 17))		\
	    objc_class_names_section ();				\
	  else if (!strncmp (name, "_OBJC_METH_VAR_NAME_", 20))		\
	    objc_meth_var_names_section ();				\
	  else if (!strncmp (name, "_OBJC_METH_VAR_TYPE_", 20))		\
	    objc_meth_var_types_section ();				\
	  else if (!strncmp (name, "_OBJC_CLASS_REFERENCES", 22))	\
	    objc_cls_refs_section ();					\
	  else if (!strncmp (name, "_OBJC_CLASS_", 12))			\
	    objc_class_section ();					\
	  else if (!strncmp (name, "_OBJC_METACLASS_", 16))		\
	    objc_meta_class_section ();					\
	  else if (!strncmp (name, "_OBJC_CATEGORY_", 15))		\
	    objc_category_section ();					\
	  else if (!strncmp (name, "_OBJC_SELECTOR_REFERENCES", 25))	\
	    objc_selector_refs_section ();				\
	  else if (!strncmp (name, "_OBJC_SELECTOR_FIXUP", 20))		\
	    objc_selector_fixup_section ();				\
	  else if (!strncmp (name, "_OBJC_SYMBOLS", 13))		\
	    objc_symbols_section ();					\
	  else if (!strncmp (name, "_OBJC_MODULES", 13))		\
	    objc_module_info_section ();				\
	  else if (!strncmp (name, "_OBJC_PROTOCOL_INSTANCE_METHODS_", 32)) \
	    objc_cat_inst_meth_section ();                              \
	  else if (!strncmp (name, "_OBJC_PROTOCOL_CLASS_METHODS_", 29)) \
	    objc_cat_cls_meth_section ();                               \
	  else if (!strncmp (name, "_OBJC_PROTOCOL_REFS_", 20))         \
	    objc_cat_cls_meth_section ();                               \
	  else if (!strncmp (name, "_OBJC_PROTOCOL_", 15))              \
	    objc_protocol_section ();                                   \
	  else if ((TREE_READONLY (exp) || TREE_CONSTANT (exp))		\
		&& !TREE_SIDE_EFFECTS (exp))     			\
             { if (flag_pic && reloc ) const_data_section ();             \
               else readonly_data_section (); }                       	\
	  else								\
	    data_section ();						\
	}								\
      else if ((TREE_READONLY (exp) || TREE_CONSTANT (exp))	\
	       && !TREE_SIDE_EFFECTS (exp))				\
                 { if (flag_pic && reloc ) const_data_section ();             \
                   else readonly_data_section (); }                       	\
      else								\
        data_section ();						\
      try_section_alias ();	\
    }									\
  while (0)

#undef	SELECT_RTX_SECTION
#define SELECT_RTX_SECTION(mode, rtx)					\
  do									\
    {									\
      if (GET_MODE_SIZE(mode) == 8)					\
	literal8_section();						\
      else if (GET_MODE_SIZE(mode) == 4)				\
	literal4_section();						\
      else								\
	const_section ();						\
    }									\
  while (0)


#ifdef OPENSTEP
#define LINK_COMMAND_SPEC \
"%{!fsyntax-only: \
   %{!c:%{!M:%{!MM:%{!E:%{!precomp:%{!S: \
      %{dynamiclib:/bin/libtool -arch_only %T %{!all_load:-noall_load} %J \
			%{current_version:}%{compatibility_version:}%{init*}} \
     %{!dynamiclib:%{.C:/bin/ld++}%{.M:/bin/ld++}%{.cc:/bin/ld++} \
		  %{!.C:%{!.M:%{!.cc:%{ObjC++:/bin/ld++}%{!ObjC++:/bin/ld}}}} \
		  -arch %T %{@:-arch_multiple}} \
			%{arch_errors_fatal} %l %X %{force_cpusubtype_ALL}\
			%{@:-o %f%g-%T.out}%{!@:%{o}%{!o:%{dynamiclib:-o a.out}}} \
                        %{NEXTSTEP-deployment-target} \
			%{static}%{!static:-dynamic} \
			%{bundle:-bundle %{!dynamiclib:%{image_base}}} \
			%{A} %{d} %{e*} %{m} %{N} %{n} %{p} \
			%{r} %{s} %{S} %{T*} %{t} %{u*} %{X} %{x} %{z} %{y*} \
			%{!A:%{!nostdlib:%{!nostartfiles:%S}}}\
			%{L*} %o %{!nostdlib:%G %L %{!A:%E}} \
     %{!dynamiclib:\n/bin/objcunique %{prebind} %{noprebind} \
		   %{@:%f%g-%T.out} %{!@:%{o*:%*}}}}}}}}}}"
#else
#define LINK_COMMAND_SPEC \
"%{!fsyntax-only: \
   %{!c:%{!M:%{!MM:%{!E:%{!precomp:%{!S: \
      %{dynamiclib:/usr/bin/libtool -arch_only %T %{!all_load:-noall_load} %J \
		%{current_version:}%{compatibility_version:}%{init*}} \
     %{!dynamiclib:/usr/bin/ld -arch %T %{@:-arch_multiple}} \
		%{arch_errors_fatal} %l %X %{force_cpusubtype_ALL}\
		%{@:-o %f%g-%T.out}%{!@:%{o}%{!o:%{dynamiclib:-o a.out}}} \
                %{NEXTSTEP-deployment-target} \
		%{static}%{!static:-dynamic} \
		%{bundle:-bundle %{!dynamiclib:%{image_base}}} \
		%{A} %{d} %{e*} %{m} %{N} %{n} %{p} \
		%{r} %{s} %{Si}%{Sn} %{T*} %{t} %{u*} %{X} %{x} %{z} %{y*} \
		%{!A:%{!nostdlib:%{!nostartfiles:%S}}} \
		%{L*} %o %{!nostdlib:%G %L %{!A:%E}} \
		%{.C:\\| /usr/bin/c++filt}%{.M:\\| /usr/bin/c++filt} \
		%{.cc:\\| /usr/bin/c++filt}%{.cxx:\\| /usr/bin/c++filt} \
		%{.cp:\\| /usr/bin/c++filt}%{.cpp:\\| /usr/bin/c++filt} \
		%{!.C:%{!.M:%{!.cc:%{!.cxx:%{!.cp:%{!.cpp:%{ObjC++:\\| /usr/bin/c++filt}}}}}}}}}}}}}}"
#endif


#define DECLARE_UNRESOLVED_REFERENCE(NAME)				\
    do { extern FILE* asm_out_file; 					\
	 if (flag_pic) fprintf (asm_out_file, "\t.lazy_reference ");	\
	 else fprintf (asm_out_file, "\t.reference ");			\
	 assemble_name (asm_out_file, NAME);				\
	 fprintf (asm_out_file, "\n");					\
       } while (0)


#define DECLARE_CLASS_REFERENCE(NAME) \
    do { extern FILE* asm_out_file; fprintf (asm_out_file, "\t");	\
         assemble_name (asm_out_file, NAME); 				\
	 fprintf (asm_out_file, "=0\n");				\
	 assemble_global (NAME);					\
       } while (0)

#define GO_IF_CPLUSPLUS_INCLUDE_NAME(NAME,LABEL)		\
 do { char *_p = (NAME);					\
      _next: switch (*_p) { case 0: break;			\
      case 'c': case 'C': case 'G': case 'g':			\
      if (_p[1] == '+' && _p[2] == '+') goto LABEL; break;	\
      default: _p += 1; goto _next; }} while (0)

#undef ASM_GLOBALIZE_LABEL
#define ASM_GLOBALIZE_LABEL(FILE,NAME)	\
 do { const char* _x = (NAME); if (!!strncmp (_x, "_OBJC_", 6)) { \
  (fputs (".globl ", FILE), assemble_name (FILE, _x), fputs ("\n", FILE)); \
 }} while (0)

#define ASM_PRIVATE_EXTERNIZE_LABEL(FILE, NAME) \
 do { const char* _x = (NAME); if (!!strncmp (_x, "_OBJC_", 6)) { \
  fputs (".private_extern ", FILE); assemble_name (FILE, _x); \
  fputs ("\n", FILE); }} while (0)

#ifdef NEXT_LIBGCC_NAMES
#include "next/nextstep.def"
#endif

#ifndef JUMP_TABLES_IN_TEXT_SECTION
#define JUMP_TABLES_IN_TEXT_SECTION 1
#endif
