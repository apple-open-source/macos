/* Operating system specific defines to be used when targeting GCC for
   OPENSTEP for Windows NT 3.x on an i386.
   Copyright (C) 1995-1996 NeXT Software, Inc.

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
Boston, MA 02111-1307, USA. */

#ifndef NEXT_OBJC_RUNTIME
#define NEXT_OBJC_RUNTIME
#endif
#ifndef NEXT_PDO
#define NEXT_PDO
#endif

#define NEXT_CPP_PRECOMP

#include "i386/win-nt.h"

#undef SIZE_TYPE
#define SIZE_TYPE "unsigned int"

#undef CPP_PREDEFINES
#define CPP_PREDEFINES "-Di386 -DWIN32 -D_WIN32 -Dwinnt -DWINNT \
  -D_M_IX86=300 -D_X86_=1 -D__STDC__=0 -DALMOST_STDC \
  -DNeXT_PDO -D_MT -D_DLL -D__LITTLE_ENDIAN__ -D__ARCHITECTURE__=\\\"i386\\\" \
  -Asystem(winnt) -Acpu(i386) -Amachine(i386)"

#undef CPP_SPEC
#define CPP_SPEC "%{posix:-D_POSIX_SOURCE} %{F*} %{static:}"

/* According to the NEWS file, -fhandle-exceptions is known to be buggy in
   conjunction with -O.  One can certainly avoid a lot of problems by disabling
   inlining when -fhandle-exceptions is present on the command line.  */
#define CC1PLUS_SPEC "%{fhandle-exceptions:-fno-inline -fkeep-inline-functions}"

#define ASM_FINAL_SPEC \
  "%{gcodeview*:\nStabsToCodeview %{c:%{o*:%*}%{!o*:%b.o}}%{!c:%U.o} %{g*}} \
   %{scatter:\nScatterPrep %{c:%{o*:%*}%{!o*:%b.o}}%{!c:%U.o}}"

#undef LIB_SPEC
#define LIB_SPEC \
  "%{dll:dllmain.o nsregisterdll.o} %{bundle:dllmain.o nsregisterdll.o} \
   %{dynamiclib:dllmain.o nsregisterdll.o} \
   nextpdo.lib msvcrt.lib kernel32.lib %{wing:wing32.lib} \
   wsock32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib \
   shell32.lib ole32.lib oleaut32.lib uuid.lib %{!shared:-lgcc}"

#define LIBGCC_SPEC ""

#undef LINK_SPEC
#define LINK_SPEC "%{F*} %{framework*} -nodefaultlib -align:0x1000 \
  %{gcodeview:%{!gcodeview0:-debug -debugtype:both -pdb:none}} %{undefined*} \
  %{image_base*} %{!v:-nologo} \
  %{dll} %{!dll:%{bundle:-dll} \
		%{!bundle:%{!win:-subsystem:console -entry:mainCRTStartup} \
			  %{win:-subsystem:windows -entry:WinMainCRTStartup}}}"

#undef TARGET_VERSION
#define TARGET_VERSION  fprintf( stderr, " (i386-applepdo-winnt3.51)" );

#undef STANDARD_EXEC_PREFIX
#define STANDARD_EXEC_PREFIX "/Developer/Libraries/gcc-lib/"

#undef STANDARD_STARTFILE_PREFIX
#define STANDARD_STARTFILE_PREFIX "/Developer/Libraries/"

#undef TOOLDIR_BASE_PREFIX
#define TOOLDIR_BASE_PREFIX "/Developer/"

#undef GPLUSPLUS_INCLUDE_DIR
#define GPLUSPLUS_INCLUDE_DIR "/Developer/Headers/g++/"

#undef GCC_INCLUDE_DIR
#define GCC_INCLUDE_DIR "/usr/local/gcc/"

#undef LOCAL_INCLUDE_DIR
#define LOCAL_INCLUDE_DIR "/Local/Developer/Headers"

#undef INCLUDE_DEFAULTS
#define INCLUDE_DEFAULTS						\
  {									\
    { GPLUSPLUS_INCLUDE_DIR, 1, 1 },					\
    { "/Library/Frameworks/System.framework/PrivateHeaders", 0, 1 },	\
    { "/Library/Frameworks/System.framework/Headers", 0 },		\
    { "/Developer/Headers", 0 },					\
    { LOCAL_INCLUDE_DIR, 0, 1 },					\
    { 0, 0, 0 }								\
  }

#define WIFSIGNALED(S) ((S) != SUCCESS_EXIT_CODE && (S) != FATAL_EXIT_CODE)
#define WIFEXITED(S) 1
#define WEXITSTATUS(S) (S)

/* Report errors to make application. */

#define REPORT_EVENT(TYPE, NAME, FILE, LINE, MSG, ARG1, ARG2, ARG3)	\
  make_support (TYPE, NAME, FILE, LINE, MSG, ARG1, ARG2, ARG3)
#define V_REPORT_EVENT(TYPE, NAME, FILE, LINE, MSG, AP)			\
  v_make_support (TYPE, NAME, FILE, LINE, MSG, AP)

/* Enable code that handles Apple's Pascal strings.  */

#define PASCAL_STRINGS

/* Enable code that handles four-character constants.  */

#define FOUR_CHAR_CONSTANTS

/* For profiling, use the MS "_penter" name, and make sure it gets called
 * before the prologue.
 */
#define PROFILE_BEFORE_PROLOGUE
#undef	FUNCTION_PROFILER
#define FUNCTION_PROFILER(FILE, LABELNO)				\
{									\
	fprintf(FILE,"\tcall __penter\n");				\
}

#undef CONST_SECTION_FUNCTION 
#define CONST_SECTION_FUNCTION					\
void								\
const_section ()						\
{								\
  if (in_section != in_const)					\
    {								\
      fprintf (asm_out_file, "\t.const\n");			\
      in_section = in_const;					\
    }								\
}

#undef CTOR_SECTION_FUNCTION
#define CTOR_SECTION_FUNCTION					\
void								\
ctor_section ()							\
{								\
  if (in_section != in_ctor)					\
    {								\
      fprintf (asm_out_file, "\t.section .CRT$XCU\n");		\
      in_section = in_ctor;					\
    }								\
}

#undef DTOR_SECTION_FUNCTION
#define DTOR_SECTION_FUNCTION					\
void								\
dtor_section ()							\
{								\
  if (in_section != in_dtor)					\
    { /* This allows for stuff at beginning and end of .dtor section.  */ \
      fprintf (asm_out_file, "\t.section .dtor$M\n");		\
      in_section = in_dtor;					\
    }								\
}

#undef EXTRA_SECTIONS
#define EXTRA_SECTIONS in_ctor, in_dtor, in_const

#undef EXTRA_SECTION_FUNCTIONS
#define EXTRA_SECTION_FUNCTIONS					\
  CONST_SECTION_FUNCTION					\
  CTOR_SECTION_FUNCTION						\
  DTOR_SECTION_FUNCTION

#undef READONLY_DATA_SECTION 
#define READONLY_DATA_SECTION() const_section()

/* SELECT_SECTION implementation for ObjC sections.
 * Copied from original nextstep.h.
 *
 * Currently the ObjC data is broken out into only
 * two different sections, depending upon whether they
 * are ever written on or not.
 *
 * We can modify the objc_blah_section macros later to
 * add specific named sections.
 */

#define objc_cls_meth_section		data_section
#define objc_inst_meth_section		data_section
#define objc_cat_cls_meth_section 	data_section
#define objc_cat_inst_meth_section 	data_section
#define objc_class_vars_section 	data_section
#define objc_instance_vars_section 	data_section
#define objc_cat_cls_meth_section 	data_section
#define objc_cls_refs_section 		data_section
#define objc_class_section 		data_section
#define objc_meta_class_section 	data_section
#define objc_category_section 		data_section
#define objc_selector_refs_section 	data_section
#define objc_symbols_section 		data_section
#define objc_module_info_section 	data_section
#define objc_cat_inst_meth_section 	data_section
#define objc_cat_cls_meth_section 	data_section
#define objc_cat_cls_meth_section 	data_section
#define objc_protocol_section 		data_section
#define objc_string_object_section	data_section

#define objc_class_names_section 	readonly_data_section
#define objc_meth_var_names_section 	readonly_data_section
#define objc_meth_var_types_section 	readonly_data_section

#undef	SELECT_SECTION
#define SELECT_SECTION(exp,reloc)				\
  do								\
    {								\
      if (TREE_CODE (exp) == STRING_CST)			\
	{							\
	  if (flag_writable_strings)				\
	    data_section ();					\
	  else 							\
	    readonly_data_section ();				\
	}							\
      else if ((TREE_CODE (exp) == INTEGER_CST)			\
	       || (TREE_CODE (exp) == REAL_CST))		\
        {							\
	    readonly_data_section ();				\
	}							\
      else if (TREE_CODE (exp) == CONSTRUCTOR				\
	       && TREE_TYPE (exp)					\
	       && TREE_CODE (TREE_TYPE (exp)) == RECORD_TYPE		\
	       && TYPE_NAME (TREE_TYPE (exp))				\
	       && TREE_CODE (TYPE_NAME (TREE_TYPE (exp))) == IDENTIFIER_NODE \
	       && IDENTIFIER_POINTER (TYPE_NAME (TREE_TYPE (exp))))	\
	{								\
	  if (!strcmp (IDENTIFIER_POINTER (TYPE_NAME (TREE_TYPE (exp))),\
			"NSConstantString"))				\
	    objc_string_object_section ();				\
	  else if (!strcmp (IDENTIFIER_POINTER (TYPE_NAME (TREE_TYPE (exp))), \
			"NXConstantString"))				\
	    objc_string_object_section ();				\
	  else if ((TREE_READONLY (exp) || TREE_CONSTANT (exp))		\
		   && !TREE_SIDE_EFFECTS (exp))				\
	    readonly_data_section ();					\
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
	    readonly_data_section ();                                   \
	  else								\
	    data_section ();						\
	}								\
      else if (TREE_CODE (exp) == VAR_DECL)				\
	{								\
	  if ((flag_pic && reloc)					\
	      || !TREE_READONLY (exp) || TREE_SIDE_EFFECTS (exp)	\
	      || !DECL_INITIAL (exp)					\
	      || (DECL_INITIAL (exp) != error_mark_node			\
		  && !TREE_CONSTANT (DECL_INITIAL (exp))))		\
	    data_section ();						\
	  else								\
	    readonly_data_section ();					\
	}								\
      else if (!strcmp (lang_identify (), "cplusplus"))			\
	/* The default should probably be readonly_data_section (),	\
	   regardless of whether the compiler is Objective-C or		\
	   Objective-C++, but this can cause data structures for	\
	   Objective-C++ constant strings to be put in the data section	\
	   (see Tracker	bug 71979).  A better fix for this bug should	\
	   be investigated.  */						\
	data_section ();						\
      else								\
	readonly_data_section ();					\
    }									\
  while (0)

#undef ASM_OUTPUT_ALIGN
#define ASM_OUTPUT_ALIGN(FILE,LOG) \
	if ((LOG)!=0) fprintf ((FILE), "\t.align %d\n", 1<<(LOG))

/* gdb needs a null N_SO at the end of each file for scattered loading. */

#undef	DBX_OUTPUT_MAIN_SOURCE_FILE_END
#define DBX_OUTPUT_MAIN_SOURCE_FILE_END(FILE, FILENAME)			\
  fprintf (FILE,							\
	   "\t.text\n\t.stabs \"\",%d,0,0,Letext\nLetext:\n",		\
	   N_SO)

/* These are all set here (or unset here!) so that struct
   returns are done in registers if they will fit. See the
   additional comments in config/winnt/winnt.h for other
   issues involved with struct returns. */
#undef PCC_STRUCT_RETURN
#define DEFAULT_PCC_STRUCT_RETURN 0

#define WORD_SWITCH_TAKES_ARG(STR)				\
  (DEFAULT_WORD_SWITCH_TAKES_ARG (STR)				\
   || !strcmp (STR, "arch")					\
   || !strcmp (STR, "filelist")					\
   || !strcmp (STR, "framework")				\
   || !strcmp (STR, "undefined")				\
   || !strcmp (STR, "image_base"))

#define WORD_SWITCH(STR)			\
  (WORD_SWITCH_TAKES_ARG(STR)			\
   || !strcmp (STR, "bundle"))

#include <winnt-pdo.h>
