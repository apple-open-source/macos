/* Target definitions for Darwin (Mac OS X) systems.
   Copyright (C) 1989, 1990, 1991, 1992, 1993, 2000, 2001, 2002
   Free Software Foundation, Inc.
   Contributed by Apple Computer Inc.

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

/* The definitions in this file are common to all processor types
   running Darwin, which is the kernel for Mac OS X.  Darwin is
   basically a BSD user layer laid over a Mach kernel, then evolved
   for many years (at NeXT) in parallel with other Unix systems.  So
   while the runtime is a somewhat idiosyncratic Mach-based thing,
   other definitions look like they would for a BSD variant.  */

/* Although NeXT ran on many different architectures, as of Jan 2001
   the only supported Darwin targets are PowerPC and x86.  */

/* Technically, STANDARD_EXEC_PREFIX should be /usr/libexec/, but in
   practice this makes it hard to install new compilers elsewhere, so
   leave it undefined and expect system builders to set configure args
   correctly.  */

/* Suppress g++ attempt to link in the math library automatically.
   (Some Darwin versions have a libm, but they seem to cause problems
   for C++ executables.)  */

#ifndef CONFIG_DARWIN_H
#define CONFIG_DARWIN_H

/* APPLE LOCAL PFE */
#ifdef PFE
/* For PFE machopic_function_base_name() always places its string in
   a PFE-allocated buffer.  In addition it is always the same buffer
   so that all rtl references that point to this buffer point to that
   single buffer.  PFE_SAVESTRING ensures it will never map strings
   in this buffer to some other PFE-allocated buffer.  This is
   necessary since when this buffer changes on the load side all
   rtl references to it reflect that change.  */
#define MACHOPIC_FUNCTION_BASE_NAME() (machopic_function_base_name())
#else
#define MACHOPIC_FUNCTION_BASE_NAME() (machopic_function_base_name())
#endif /* PFE */

#define MATH_LIBRARY ""

/* We have atexit.  */

#define HAVE_ATEXIT

/* Define an empty body for the function do_global_dtors() in libgcc2.c.  */

#define DO_GLOBAL_DTORS_BODY

/* The string value for __SIZE_TYPE__.  */

#ifndef SIZE_TYPE
#define SIZE_TYPE "long unsigned int"
#endif

/* Type used for ptrdiff_t, as a string used in a declaration.  */

#undef  PTRDIFF_TYPE
#define PTRDIFF_TYPE "int"

/* wchar_t is int.  */

#undef	WCHAR_TYPE
#define WCHAR_TYPE "int"
#undef	WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 32

/* APPLE LOCAL begin size of bool  */
/* `bool' has size and alignment `4', on Darwin.  */
#undef	BOOL_TYPE_SIZE
#define BOOL_TYPE_SIZE 32
/* APPLE LOCAL end size of bool  */

/* Default to using the NeXT-style runtime, since that's what is
   pre-installed on Darwin systems.  */

#define NEXT_OBJC_RUNTIME

/* Don't default to pcc-struct-return, because gcc is the only compiler, and
   we want to retain compatibility with older gcc versions.  */

#undef	DEFAULT_PCC_STRUCT_RETURN
#define DEFAULT_PCC_STRUCT_RETURN 0

/* APPLE LOCAL begin flag translation */
/* This table intercepts weirdo options whose names would interfere
   with normal driver conventions, and either translates them into
   standardly-named options, or adds a 'Z' so that they can get to
   specs processing without interference.

   Do not expand a linker option to "-Xlinker -<option>", since that
   forfeits the ability to control via spec strings later.  However,
   as a special exception, do this translation with -filelist, because
   otherwise the driver will think there are no input files and quit.
   (The alternative would be to hack the driver to recognize -filelist
   specially, but it's simpler to use the translation table.)

   Note that an option name with a prefix that matches another option
   name, that also takes an argument, needs to be modified so the
   prefix is different, otherwise a '*' after the shorter option will
   match with the longer one.  */
/* Ignore -dynamic for now */
#define TARGET_OPTION_TRANSLATE_TABLE \
  { "-all_load", "-Zall_load" },  \
  { "-allowable_client", "-Zallowable_client" },  \
  { "-arch_errors_fatal", "-Zarch_errors_fatal" },  \
  { "-bundle", "-Zbundle" },  \
  { "-bundle_loader", "-Zbundle_loader" },  \
  { "-weak_reference_mismatches", "-Zweak_reference_mismatches" },  \
  { "-dependency-file", "-MF" }, \
  { "-dylib_file", "-Zdylib_file" }, \
  { "-dynamic", " " },  \
  { "-dynamiclib", "-Zdynamiclib" },  \
  { "-seg_addr_table_filename", "-Zseg_addr_table_filename" }, \
  { "-filelist", "-Xlinker -filelist -Xlinker" },  \
  { "-flat_namespace", "-Zflat_namespace" },  \
  { "-force_cpusubtype_ALL", "-Zforce_cpusubtype_ALL" },  \
  { "-force_flat_namespace", "-Zforce_flat_namespace" },  \
  { "-framework", "-Zframework" },  \
  { "-image_base", "-Zimage_base" },  \
  { "-init", "-Zinit" },  \
  { "-install_name", "-Zinstall_name" },  \
  { "-multiply_defined_unused", "-Zmultiplydefinedunused" },  \
  { "-multiply_defined", "-Zmultiply_defined" },  \
  { "-static", "-static -Wa,-static" },  \
  { "-traditional-cpp", "-no-cpp-precomp" }
/* APPLE LOCAL end flag translation */

/* APPLE LOCAL framework headers */
/* Need to look for framework headers.  */

#define FRAMEWORK_HEADERS

/* APPLE LOCAL -framework */
/* These compiler options take n arguments.  */  
   
#undef  WORD_SWITCH_TAKES_ARG
/* APPLE LOCAL begin -precomp-trustfile */
/* APPLE LOCAL begin -arch */
/* APPLE LOCAL begin linker flags */
#define WORD_SWITCH_TAKES_ARG(STR)		\
  (DEFAULT_WORD_SWITCH_TAKES_ARG (STR) ? 1 :	\
   !strcmp (STR, "Zallowable_client") ? 1 :	\
   !strcmp (STR, "arch") ? 1 :			\
   !strcmp (STR, "arch_only") ? 1 :		\
   !strcmp (STR, "Zbundle_loader") ? 1 :	\
   !strcmp (STR, "client_name") ? 1 :		\
   !strcmp (STR, "compatibility_version") ? 1 : \
   !strcmp (STR, "current_version") ? 1 :	\
   !strcmp (STR, "Zdylib_file") ? 1 :		\
   !strcmp (STR, "Zframework") ? 1 :		\
   !strcmp (STR, "Zimage_base") ? 1 :		\
   !strcmp (STR, "Zinit") ? 1 :			\
   !strcmp (STR, "Zinstall_name") ? 1 :		\
   !strcmp (STR, "Zmultiplydefinedunused") ? 1 : \
   !strcmp (STR, "Zmultiply_defined") ? 1 :	\
   !strcmp (STR, "precomp-trustfile") ? 1 :	\
   !strcmp (STR, "read_only_relocs") ? 1 :	\
   !strcmp (STR, "sectcreate") ? 3 :		\
   !strcmp (STR, "sectorder") ? 3 :		\
   !strcmp (STR, "Zseg_addr_table_filename") ?1 :\
   !strcmp (STR, "seg1addr") ? 1 :		\
   !strcmp (STR, "segprot") ? 3 :		\
   !strcmp (STR, "seg_addr_table") ? 1 :	\
   !strcmp (STR, "sub_library") ? 1 :		\
   !strcmp (STR, "sub_umbrella") ? 1 :		\
   !strcmp (STR, "umbrella") ? 1 :		\
   !strcmp (STR, "undefined") ? 1 :		\
   !strcmp (STR, "Zweak_reference_mismatches") ? 1 : \
   !strcmp (STR, "pagezero_size") ? 1 :		\
   !strcmp (STR, "segs_read_only_addr") ? 1 :	\
   !strcmp (STR, "segs_read_write_addr") ? 1 :	\
   !strcmp (STR, "sectalign") ? 3 :		\
   !strcmp (STR, "sectobjectsymbols") ? 2 :	\
   !strcmp (STR, "segcreate") ? 3 :		\
   !strcmp (STR, "dylinker_install_name") ? 1 :	\
   0)
/* APPLE LOCAL end -precomp-trustfile */
/* APPLE LOCAL end -arch */
/* APPLE LOCAL end linker flags */

/* Don't warn about MacOS-style 'APPL' four-char-constants.  */

#undef WARN_FOUR_CHAR_CONSTANTS
#define WARN_FOUR_CHAR_CONSTANTS 0

/* Machine dependent cpp options.  */

#undef	CPP_SPEC
/* APPLE LOCAL -precomp-trustfile, -arch */
#define CPP_SPEC "%{static:-D__STATIC__}%{!static:-D__DYNAMIC__} \
		  %{precomp-trustfile} %{arch}"


/* APPLE LOCAL cc1plus spec  */
#undef CC1PLUS_SPEC
#define CC1PLUS_SPEC "-D__private_extern__=extern"

/* APPLE LOCAL asm flags */
#define ASM_SPEC "-arch %T \
  %{Zforce_cpusubtype_ALL:-force_cpusubtype_ALL} \
  %{!Zforce_cpusubtype_ALL:%{faltivec:-force_cpusubtype_ALL}}"

/* APPLE LOCAL begin linker flags */
/* This is mostly a clone of the standard LINK_COMMAND_SPEC, plus
   framework, precomp, libtool, and fat build additions.  Also we
   don't specify a second %G after %L because libSystem is
   self-contained and doesn't need to link against libgcc.a.  */
/* In general, random Darwin linker flags should go into LINK_SPEC
   instead of LINK_COMMAND_SPEC.  The command spec is better for
   specifying the handling of options understood by generic Unix
   linkers, and for positional arguments like libraries.  */
#define LINK_COMMAND_SPEC "\
%{!fdump=*:%{!fsyntax-only:%{!precomp:%{!c:%{!M:%{!MM:%{!E:%{!S:\
    %{!Zdynamiclib:%(linker)}%{Zdynamiclib:/usr/bin/libtool} \
    %{!Zdynamiclib:-arch %T %{@:-arch_multiple}} \
    %{Zdynamiclib:-arch_only %T} \
    %l %X %{d} %{s} %{t} %{Z} \
    %{!Zdynamiclib:%{A} %{e*} %{m} %{N} %{n} %{r} %{u*} %{x} %{z}} \
    %{@:-o %f%u.out}%{!@:%{o*}%{!o:-o a.out}} \
    %{!Zdynamiclib:%{!A:%{!nostdlib:%{!nostartfiles:%S}}}} \
    %{L*} %(link_libgcc) %o %{!nostdlib:%{!nodefaultlibs:%G %L}} \
    %{!A:%{!nostdlib:%{!nostartfiles:%E}}} %{T*} %{F*} \
    %{!--help:%{!no-c++filt|c++filt:| c++filt3 }} }}}}}}}}"

/* Note that the linker
   output is always piped through c++filt (unless -no-c++filt is
   specified) to ensure error messages have demangled C++ names.
   We do this even for C.  */
/* nice idea, needs some work
   "%{!no-c++filt|c++filt:| " STANDARD_BINDIR_PREFIX cppfilt " }}}}}}}}" */

/* Please keep the random linker options in alphabetical order (modulo
   'Z' and 'no' prefixes).  Options that can only go to one of libtool
   or ld must be listed twice, under both !Zdynamiclib and
   Zdynamiclib, with one of the cases reporting an error.  */
/* Note that options taking arguments may appear multiple times on a
   command line with different arguments each time, so put a * after
   their names so all of them get passed.  */
#define LINK_SPEC  \
  "%{static}%{!static:-dynamic} \
   %{!Zdynamiclib: \
     %{Zbundle:-bundle} \
     %{Zbundle_loader*:-bundle_loader %*} \
     %{client_name*} \
     %{compatibility_version*:%e-compatibility_version only allowed with -dynamiclib} \
     %{current_version*:%e-current_version only allowed with -dynamiclib} \
     %{Zforce_cpusubtype_ALL:-force_cpusubtype_ALL} \
     %{Zforce_flat_namespace:-force_flat_namespace} \
     %{Zinstall_name*:%e-install_name only allowed with -dynamiclib} \
     %{keep_private_externs} \
     %{private_bundle} \
    } \
   %{Zdynamiclib: \
     %{Zbundle:%e-bundle not allowed with -dynamiclib} \
     %{Zbundle_loader*:%e-bundle_loader not allowed with -dynamiclib} \
     %{client_name*:%e-client_name not allowed with -dynamiclib} \
     %{compatibility_version*} \
     %{current_version*} \
     %{Zforce_cpusubtype_ALL:%e-force_cpusubtype_ALL not allowed with -dynamiclib} \
     %{Zforce_flat_namespace:%e-force_flat_namespace not allowed with -dynamiclib} \
     %{Zinstall_name*:-install_name %*} \
     %{keep_private_externs:%e-keep_private_externs not allowed with -dynamiclib} \
     %{private_bundle:%e-private_bundle not allowed with -dynamiclib} \
    } \
   %{Zall_load:-all_load}%{Zdynamiclib:%{!Zall_load:-noall_load}} \
   %{Zallowable_client*:-allowable_client %*} \
   %{Zarch_errors_fatal:-arch_errors_fatal} \
   %{Zdylib_file*:-dylib_file %*} \
   %{Zflat_namespace:-flat_namespace} \
   %{Zframework*:-framework %*} \
   %{headerpad_max_install_names*} \
   %{Zimage_base*:-image_base %*} \
   %{Zinit*:-init %*} \
   %{nomultidefs} \
   %{Zmultiply_defined*:-multiply_defined %*} \
   %{Zmultiplydefinedunused*:-multiply_defined_unused %*} \
   %{prebind} %{noprebind} %{prebind_all_twolevel_modules} \
   %{read_only_relocs} \
   %{sectcreate*} %{sectorder*} %{seg1addr*} %{segprot*} %{seg_addr_table*} \
   %{Zseg_addr_table_filename*:-seg_addr_table_filename %*} \
   %{sub_library*} %{sub_umbrella*} \
   %{twolevel_namespace} %{twolevel_namespace_hints} \
   %{umbrella*} \
   %{undefined*} \
   %{Zweak_reference_mismatches*:-weak_reference_mismatches %*} \
   %{X} \
   %{y*} \
   %{w} \
   %{pagezero_size*} %{segs_read_*} %{seglinkedit} %{noseglinkedit}  \
   %{sectalign*} %{sectobjectsymbols*} %{segcreate*} %{whyload} \
   %{whatsloaded} %{dylinker_install_name*} \
   %{dylinker} %{Mach} "

/* Machine dependent libraries.  */

#undef	LIB_SPEC
#define LIB_SPEC "%{!static:-lSystem}"

#undef LIBGCC_SPEC
#define LIBGCC_SPEC "%{static:-lgcc_static} \
                     %{!static:-lgcc}"

/* We specify crt0.o etc as -lcrt0.o so that ld will search the
   library path.  */

/* APPLE LOCAL begin C++ EH */
#undef	STARTFILE_SPEC
#define STARTFILE_SPEC  \
  "%{!Zdynamiclib:%{Zbundle:%{!static:-lbundle1.o}} \
     %{!Zbundle:%{pg:%{static:-lgcrt0.o} \
                     %{!static:%{object:-lgcrt0.o} \
                               %{!object:%{preload:-lgcrt0.o} \
                                 %{!preload:-lgcrt1.o -lcrtbegin.o}}}} \
                %{!pg:%{static:-lcrt0.o} \
                      %{!static:%{object:-lcrt0.o} \
                                %{!object:%{preload:-lcrt0.o} \
                                  %{!preload:-lcrt1.o -lcrtbegin.o}}}}}}"
/* APPLE LOCAL end C++ EH */

#undef	DOLLARS_IN_IDENTIFIERS
#define DOLLARS_IN_IDENTIFIERS 2

/* Allow #sccs (but don't do anything). */

#define SCCS_DIRECTIVE

/* We use Dbx symbol format.  */

#define DBX_DEBUGGING_INFO

/* Also enable Dwarf 2 as an option.  */

#define DWARF2_DEBUGGING_INFO

#define PREFERRED_DEBUGGING_TYPE DBX_DEBUG

/* APPLE LOCAL begin gdb only used symbols */
/* Support option to generate stabs for only used symbols. */

#define DBX_ONLY_USED_SYMBOLS
/* APPLE LOCAL end gdb only used symbols */

/* When generating stabs debugging, use N_BINCL entries.  */

#define DBX_USE_BINCL

/* There is no limit to the length of stabs strings.  */

#define DBX_CONTIN_LENGTH 0

/* gdb needs a null N_SO at the end of each file for scattered loading. */

#undef	DBX_OUTPUT_MAIN_SOURCE_FILE_END
#define DBX_OUTPUT_MAIN_SOURCE_FILE_END(FILE, FILENAME)			\
do { text_section ();							\
     fprintf (FILE,							\
	      "\t.stabs \"%s\",%d,0,0,Letext\nLetext:\n", "" , N_SO);	\
   } while (0)

/* Our profiling scheme doesn't LP labels and counter words.  */

#define NO_PROFILE_COUNTERS

#undef	INIT_SECTION_ASM_OP
#define INIT_SECTION_ASM_OP

/* APPLE LOCAL static structors in __StaticInit section */
#define STATIC_INIT_SECTION "__TEXT,__StaticInit,regular,pure_instructions"

#undef	INVOKE__main

#define TARGET_ASM_CONSTRUCTOR  machopic_asm_out_constructor
#define TARGET_ASM_DESTRUCTOR   machopic_asm_out_destructor


/* Don't output a .file directive.  That is only used by the assembler for
   error reporting.  */

#undef	ASM_FILE_START
#define ASM_FILE_START(FILE)

/* APPLE LOCAL PFE */
#undef	ASM_FILE_END
#define ASM_FILE_END(FILE)						\
  if (pfe_operation != PFE_DUMP)					\
    do {								\
      machopic_finish (asm_out_file);                            	\
      if (strcmp (lang_hooks.name, "GNU C++") == 0)			\
	{								\
	  constructor_section ();					\
	  destructor_section ();					\
	  ASM_OUTPUT_ALIGN (FILE, 1);					\
	}								\
    } while (0)

/* APPLE LOCAL begin darwin native */
#undef ASM_OUTPUT_LABEL
#define ASM_OUTPUT_LABEL(FILE,NAME)	\
  do { assemble_name (FILE, NAME); fputs (":\n", FILE); } while (0)

#define ASM_OUTPUT_SKIP(FILE,SIZE)  \
  fprintf (FILE, "\t.space %d\n", SIZE)
/* APPLE LOCAL end darwin native */

/* Give ObjcC methods pretty symbol names. */

#undef	OBJC_GEN_METHOD_LABEL
#define OBJC_GEN_METHOD_LABEL(BUF,IS_INST,CLASS_NAME,CAT_NAME,SEL_NAME,NUM) \
  do { if (CAT_NAME)							\
	 sprintf (BUF, "%c[%s(%s) %s]", (IS_INST) ? '-' : '+',		\
		  (CLASS_NAME), (CAT_NAME), (SEL_NAME));		\
       else								\
	 sprintf (BUF, "%c[%s %s]", (IS_INST) ? '-' : '+',		\
		  (CLASS_NAME), (SEL_NAME));				\
     } while (0)

/* The machopic_define_name calls are telling the machopic subsystem
   that the name *is* defined in this module, so it doesn't need to
   reference them indirectly.  */

#undef ASM_DECLARE_OBJECT_NAME
#define ASM_DECLARE_OBJECT_NAME(FILE, NAME, DECL)			\
  do {									\
    const char *xname = NAME;                                           \
    if (GET_CODE (XEXP (DECL_RTL (DECL), 0)) != SYMBOL_REF)             \
      xname = IDENTIFIER_POINTER (DECL_NAME (DECL));                    \
    /* APPLE LOCAL  coalescing  */					\
    if (! DECL_IS_COALESCED_OR_WEAK (DECL))                             \
      if ((TREE_STATIC (DECL)                                           \
	 && (!DECL_COMMON (DECL) || !TREE_PUBLIC (DECL)))               \
        || DECL_INITIAL (DECL))                                         \
      machopic_define_name (xname);                                     \
    if ((TREE_STATIC (DECL)                                             \
	 && (!DECL_COMMON (DECL) || !TREE_PUBLIC (DECL)))               \
        || DECL_INITIAL (DECL))                                         \
      ENCODE_SECTION_INFO (DECL);  \
    ASM_OUTPUT_LABEL (FILE, xname);                                     \
  } while (0)

#define ASM_DECLARE_FUNCTION_NAME(FILE, NAME, DECL)                     \
  do {									\
    const char *xname = NAME;                                           \
    if (GET_CODE (XEXP (DECL_RTL (DECL), 0)) != SYMBOL_REF)             \
      xname = IDENTIFIER_POINTER (DECL_NAME (DECL));                    \
    /* APPLE LOCAL  coalescing  */					\
    if (! DECL_IS_COALESCED_OR_WEAK (DECL))				\
      if ((TREE_STATIC (DECL)                                           \
	 && (!DECL_COMMON (DECL) || !TREE_PUBLIC (DECL)))               \
        || DECL_INITIAL (DECL))                                         \
      machopic_define_name (xname);                                     \
    if ((TREE_STATIC (DECL)                                             \
	 && (!DECL_COMMON (DECL) || !TREE_PUBLIC (DECL)))               \
        || DECL_INITIAL (DECL))                                         \
      ENCODE_SECTION_INFO (DECL);					\
    ASM_OUTPUT_LABEL (FILE, xname);                                     \
  } while (0)

/* Wrap new method names in quotes so the assembler doesn't gag.
   Make Objective-C internal symbols local.  */

#undef	ASM_OUTPUT_LABELREF
#define ASM_OUTPUT_LABELREF(FILE,NAME)	\
  do {									\
       STRIP_NAME_ENCODING (NAME, NAME);  \
       if (NAME[0] == '&' || NAME[0] == '*')				\
         {								\
           int len = strlen (NAME);					\
	   if (len > 6 && !strcmp ("$stub", NAME + len - 5))		\
	     machopic_validate_stub_or_non_lazy_ptr (NAME, 1);		\
	   else if (len > 7 && !strcmp ("$stub\"", NAME + len - 6))	\
	     machopic_validate_stub_or_non_lazy_ptr (NAME, 1);		\
	   else if (len > 14 && !strcmp ("$non_lazy_ptr", NAME + len - 13)) \
	     machopic_validate_stub_or_non_lazy_ptr (NAME, 0);		\
	   /* APPLE LOCAL begin Objective-C++ */			\
	   if (NAME[1] != '"' && name_needs_quotes (&NAME[1]))		\
	     fprintf (FILE, "\"%s\"", &NAME[1]);			\
	   else								\
	     fputs (&NAME[1], FILE); 					\
	   /* APPLE LOCAL end Objective-C++ */				\
	 }								\
       else if (NAME[0] == '+' || NAME[0] == '-')			\
         fprintf (FILE, "\"%s\"", NAME);				\
       else if (!strncmp (NAME, "_OBJC_", 6))				\
         fprintf (FILE, "L%s", NAME);					\
       else if (!strncmp (NAME, ".objc_class_name_", 17))		\
	 fprintf (FILE, "%s", NAME);					\
	 /* APPLE LOCAL begin Objective-C++  */				\
       else if (NAME[0] != '"' && name_needs_quotes (NAME))		\
	 fprintf (FILE, "\"%s\"", NAME);				\
	 /* APPLE LOCAL end Objective-C++  */				\
       else								\
         fprintf (FILE, "_%s", NAME);					\
  } while (0)

/* APPLE LOCAL begin darwin native */
/* Output before executable code.  */
#undef TEXT_SECTION_ASM_OP
#define TEXT_SECTION_ASM_OP ".text"

/* Output before writable data.  */

#undef DATA_SECTION_ASM_OP
#define DATA_SECTION_ASM_OP ".data"
/* APPLE LOCAL end darwin native */

#undef	ALIGN_ASM_OP
#define ALIGN_ASM_OP		".align"

#undef	ASM_OUTPUT_ALIGN
#define ASM_OUTPUT_ALIGN(FILE,LOG)	\
  if ((LOG) != 0)			\
    fprintf (FILE, "\t%s %d\n", ALIGN_ASM_OP, (LOG))

/* Ensure correct alignment of bss data.  */

#undef	ASM_OUTPUT_ALIGNED_DECL_LOCAL
#define ASM_OUTPUT_ALIGNED_DECL_LOCAL(FILE, DECL, NAME, SIZE, ALIGN) \
  do {  \
    fputs (".lcomm ", (FILE));				\
    assemble_name ((FILE), (NAME));			\
    fprintf ((FILE), ",%u,%u\n", (SIZE), floor_log2 ((ALIGN) / BITS_PER_UNIT)); \
    if ((DECL) && ((TREE_STATIC (DECL)                                             \
	 && (!DECL_COMMON (DECL) || !TREE_PUBLIC (DECL)))               \
        || DECL_INITIAL (DECL)))                                         \
      ENCODE_SECTION_INFO (DECL);  \
    if ((DECL) && ((TREE_STATIC (DECL)                                             \
	 && (!DECL_COMMON (DECL) || !TREE_PUBLIC (DECL)))               \
        || DECL_INITIAL (DECL)))                                         \
      machopic_define_name (NAME);                                     \
  } while (0)

/* Output nothing for #ident.  */

#undef	ASM_OUTPUT_IDENT
#define ASM_OUTPUT_IDENT(FILE, NAME)

/* The maximum alignment which the object file format can support.
   For Mach-O, this is 2^15.  */

#undef	MAX_OFILE_ALIGNMENT
#define MAX_OFILE_ALIGNMENT 0x8000

/* APPLE LOCAL C++ EH */
#define ASM_OUTPUT_DWARF_DELTA(FILE,SIZE,LABEL1,LABEL2)  \
  darwin_asm_output_dwarf_delta (FILE, SIZE, LABEL1, LABEL2, 0)

#define ASM_OUTPUT_RELOC_DWARF_DELTA(FILE,SIZE,LABEL1,LABEL2)  \
  darwin_asm_output_dwarf_delta (FILE, SIZE, LABEL1, LABEL2, 1)

/* Create new Mach-O sections. */

#undef	SECTION_FUNCTION
#define SECTION_FUNCTION(FUNCTION, SECTION, DIRECTIVE, OBJC)		\
extern void FUNCTION PARAMS ((void));					\
void									\
FUNCTION ()								\
{									\
  if (in_section != SECTION)						\
    {									\
      if (OBJC)								\
	objc_section_init ();						\
      data_section ();							\
      if (asm_out_file)							\
	fprintf (asm_out_file, "%s\n", DIRECTIVE);			\
      in_section = SECTION;						\
    }									\
}									\

#define ALIAS_SECTION(enum_value, alias_name) 				\
do { if (!strcmp (alias_name, name))					\
       section_alias[enum_value] = (alias ? get_identifier (alias) : 0);  \
   } while (0)

/* Darwin uses many types of special sections.  */

#undef	EXTRA_SECTIONS
#define EXTRA_SECTIONS					\
  in_const, in_const_data, in_cstring, in_literal4, in_literal8,	\
  in_constructor, in_destructor, in_mod_init, in_mod_term,		\
  in_objc_class, in_objc_meta_class, in_objc_category,	\
  in_objc_class_vars, in_objc_instance_vars,		\
  in_objc_cls_meth, in_objc_inst_meth,			\
  in_objc_cat_cls_meth, in_objc_cat_inst_meth,		\
  in_objc_selector_refs,				\
  in_objc_selector_fixup,				\
  in_objc_symbols, in_objc_module_info,			\
  in_objc_protocol, in_objc_string_object,		\
  in_objc_constant_string_object,			\
  /* APPLE LOCAL constant cfstrings */			\
  in_cfstring_constant_object,				\
  in_objc_class_names, in_objc_meth_var_names,		\
  in_objc_meth_var_types, in_objc_cls_refs, 		\
  in_machopic_nl_symbol_ptr,				\
  in_machopic_lazy_symbol_ptr,				\
  in_machopic_symbol_stub,				\
  in_machopic_picsymbol_stub,				\
  in_darwin_exception, in_darwin_eh_frame,		\
  num_sections

#undef	EXTRA_SECTION_FUNCTIONS
#define EXTRA_SECTION_FUNCTIONS			\
static void objc_section_init PARAMS ((void));	\
SECTION_FUNCTION (const_section,		\
                  in_const,			\
                  ".const", 0)			\
SECTION_FUNCTION (const_data_section,		\
                  in_const_data,		\
                  ".const_data", 0)		\
SECTION_FUNCTION (cstring_section,		\
		  in_cstring,			\
		  ".cstring", 0)		\
SECTION_FUNCTION (literal4_section,		\
		  in_literal4,			\
		  ".literal4", 0)		\
SECTION_FUNCTION (literal8_section,		\
		  in_literal8,			\
		  ".literal8", 0)		\
SECTION_FUNCTION (constructor_section,		\
		  in_constructor,		\
		  ".constructor", 0)		\
SECTION_FUNCTION (mod_init_section,		\
		  in_mod_init,			\
		  ".mod_init_func", 0)	\
SECTION_FUNCTION (mod_term_section, \
		  in_mod_term,			\
		  ".mod_term_func", 0)	\
SECTION_FUNCTION (destructor_section,		\
		  in_destructor,		\
		  ".destructor", 0)		\
SECTION_FUNCTION (objc_class_section,		\
		  in_objc_class,		\
		  ".objc_class", 1)		\
SECTION_FUNCTION (objc_meta_class_section,	\
		  in_objc_meta_class,		\
		  ".objc_meta_class", 1)	\
SECTION_FUNCTION (objc_category_section,	\
		  in_objc_category,		\
		".objc_category", 1)		\
SECTION_FUNCTION (objc_class_vars_section,	\
		  in_objc_class_vars,		\
		  ".objc_class_vars", 1)	\
SECTION_FUNCTION (objc_instance_vars_section,	\
		  in_objc_instance_vars,	\
		  ".objc_instance_vars", 1)	\
SECTION_FUNCTION (objc_cls_meth_section,	\
		  in_objc_cls_meth,		\
		  ".objc_cls_meth", 1)	\
SECTION_FUNCTION (objc_inst_meth_section,	\
		  in_objc_inst_meth,		\
		  ".objc_inst_meth", 1)	\
SECTION_FUNCTION (objc_cat_cls_meth_section,	\
		  in_objc_cat_cls_meth,		\
		  ".objc_cat_cls_meth", 1)	\
SECTION_FUNCTION (objc_cat_inst_meth_section,	\
		  in_objc_cat_inst_meth,	\
		  ".objc_cat_inst_meth", 1)	\
SECTION_FUNCTION (objc_selector_refs_section,	\
		  in_objc_selector_refs,	\
		  ".objc_message_refs", 1)	\
SECTION_FUNCTION (objc_selector_fixup_section,	\
		  in_objc_selector_fixup,	\
		  ".section __OBJC, __sel_fixup", 1)	\
SECTION_FUNCTION (objc_symbols_section,		\
		  in_objc_symbols,		\
		  ".objc_symbols", 1)	\
SECTION_FUNCTION (objc_module_info_section,	\
		  in_objc_module_info,		\
		  ".objc_module_info", 1)	\
SECTION_FUNCTION (objc_protocol_section,	\
		  in_objc_protocol,		\
		  ".objc_protocol", 1)	\
SECTION_FUNCTION (objc_string_object_section,	\
		  in_objc_string_object,	\
		  ".objc_string_object", 1)	\
SECTION_FUNCTION (objc_constant_string_object_section,	\
		  in_objc_constant_string_object,	\
		  ".section __OBJC, __cstring_object", 1)	\
/* APPLE LOCAL begin constant cfstrings */	\
/* Unlike constant NSStrings, constant CFStrings do not live */\
/* in the __OBJC segment since they may also occur in pure C */\
/* or C++ programs.  */\
SECTION_FUNCTION (cfstring_constant_object_section,	\
		  in_cfstring_constant_object,	\
		  ".section __DATA, __cfstring", 0)	\
/* APPLE LOCAL end constant cfstrings */	\
SECTION_FUNCTION (objc_class_names_section,	\
		in_objc_class_names,		\
		".objc_class_names", 1)	\
SECTION_FUNCTION (objc_meth_var_names_section,	\
		in_objc_meth_var_names,		\
		".objc_meth_var_names", 1)	\
SECTION_FUNCTION (objc_meth_var_types_section,	\
		in_objc_meth_var_types,		\
		".objc_meth_var_types", 1)	\
SECTION_FUNCTION (objc_cls_refs_section,	\
		in_objc_cls_refs,		\
		".objc_cls_refs", 1)		\
						\
SECTION_FUNCTION (machopic_lazy_symbol_ptr_section,	\
		in_machopic_lazy_symbol_ptr,		\
		".lazy_symbol_pointer", 0)      	\
SECTION_FUNCTION (machopic_nl_symbol_ptr_section,	\
		in_machopic_nl_symbol_ptr,		\
		".non_lazy_symbol_pointer", 0)      	\
SECTION_FUNCTION (machopic_symbol_stub_section,		\
		in_machopic_symbol_stub,		\
		".symbol_stub", 0)      		\
SECTION_FUNCTION (machopic_picsymbol_stub_section,	\
		in_machopic_picsymbol_stub,		\
		".picsymbol_stub", 0)      		\
SECTION_FUNCTION (darwin_exception_section,		\
		in_darwin_exception,			\
                /* APPLE LOCAL eh in data segment */  \
		".section __DATA,__gcc_except_tab", 0)	\
SECTION_FUNCTION (darwin_eh_frame_section,		\
		in_darwin_eh_frame,			\
                /* APPLE LOCAL eh in data segment */    \
		 ".section " EH_FRAME_SECTION_NAME ",__eh_frame" EH_FRAME_SECTION_ATTR, 0)  \
							\
static void					\
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
static tree section_alias[(int) num_sections];	\
static void try_section_alias PARAMS ((void));	\
static void try_section_alias () 		\
{						\
    if (section_alias[in_section] && asm_out_file) \
      fprintf (asm_out_file, "%s\n",		\
	       IDENTIFIER_POINTER (section_alias[in_section]));	\
}      						\
int darwin_named_section_is (const char* name)		\
{  /* This is in this macro because it refers to \
      variables that are local to varasm.c */	\
  return (in_section == in_named		\
	  && strcmp (in_named_name, name) == 0);   \
}
	

#if 0
static void alias_section PARAMS ((const char *, const char *)); \
static void alias_section (name, alias)			\
     const char *name, *alias;				\
{							\
    ALIAS_SECTION (in_data, "data");			\
    ALIAS_SECTION (in_text, "text");			\
    ALIAS_SECTION (in_const, "const");			\
    ALIAS_SECTION (in_const_data, "const_data");	\
    ALIAS_SECTION (in_cstring, "cstring");		\
    ALIAS_SECTION (in_literal4, "literal4");		\
    ALIAS_SECTION (in_literal8, "literal8");		\
}
#endif

#undef	READONLY_DATA_SECTION
#define READONLY_DATA_SECTION const_section

#undef	SELECT_SECTION
#define SELECT_SECTION(exp,reloc,align)				\
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
	  /* APPLE LOCAL begin constant strings */			\
	  extern int flag_next_runtime;					\
	  extern char *constant_string_class_name;			\
	  /* APPLE LOCAL end constant strings */			\
	  tree name = TYPE_NAME (TREE_TYPE (exp));			\
	  if (TREE_CODE (name) == TYPE_DECL)				\
	    name = DECL_NAME (name);					\
	  /* APPLE LOCAL begin constant strings */			\
	  if (constant_string_class_name				\
	      && !strcmp (IDENTIFIER_POINTER (name), 			\
			  constant_string_class_name)) {		\
	    if (flag_next_runtime)					\
	      objc_constant_string_object_section ();			\
	    else							\
	      objc_string_object_section ();				\
	  }								\
	  /* APPLE LOCAL end constant strings */			\
	  else if (TREE_READONLY (exp) || TREE_CONSTANT (exp))		\
	    {								\
	      /* APPLE LOCAL dynamic-no-pic  */				\
	      if (TREE_SIDE_EFFECTS (exp) || MACHOPIC_INDIRECT && reloc)\
		const_data_section ();					\
	      else							\
		readonly_data_section (); 				\
            }								\
	  else								\
	    data_section ();						\
      }									\
	/* APPLE LOCAL begin constant cfstrings */			\
      else if (TREE_CODE (exp) == CONSTRUCTOR				\
	       && TREE_TYPE (exp)					\
	       && TREE_CODE (TREE_TYPE (exp)) == ARRAY_TYPE		\
	       && TREE_OPERAND (exp, 1))				\
	{								\
	  tree name = TREE_OPERAND (exp, 1);				\
	  if (TREE_CODE (name) == TREE_LIST && TREE_VALUE (name)	\
	      && TREE_CODE (TREE_VALUE (name)) == NOP_EXPR		\
	      && TREE_OPERAND (TREE_VALUE (name), 0)			\
	      && TREE_OPERAND (TREE_OPERAND (TREE_VALUE (name), 0), 0))	\
	    {								\
	      name = TREE_OPERAND 					\
		     (TREE_OPERAND (TREE_VALUE (name), 0), 0);		\
	    }								\
	  if (TREE_CODE (name) == VAR_DECL				\
	      && !strcmp (IDENTIFIER_POINTER (DECL_NAME (name)), 	\
			  "__CFConstantStringClassReference"))		\
	    cfstring_constant_object_section ();			\
	  else if (TREE_READONLY (exp) || TREE_CONSTANT (exp))		\
	    {								\
	      /* APPLE LOCAL dynamic-no-pic  */				\
	      if (TREE_SIDE_EFFECTS (exp) || MACHOPIC_INDIRECT && reloc)\
		const_data_section ();					\
	      else							\
		readonly_data_section (); 				\
            }								\
	  else								\
	    data_section ();						\
	}								\
	/* APPLE LOCAL end constant cfstrings */			\
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
		/* APPLE LOCAL  dynamic-no-pic  */			\
             { if (MACHOPIC_INDIRECT && reloc ) const_data_section ();  \
               else readonly_data_section (); }                       	\
	  else								\
	    data_section ();						\
	}								\
      else								\
      /* APPLE LOCAL  darwin_set_section_for_var_p  */			\
      if (darwin_set_section_for_var_p (exp, reloc, align))		\
	;								\
      else if (TREE_READONLY (exp) || TREE_CONSTANT (exp))		\
	{								\
	  /* APPLE LOCAL dynamic-no-pic  */				\
	  if (TREE_SIDE_EFFECTS (exp) || MACHOPIC_INDIRECT && reloc)	\
	    const_data_section ();					\
	  else								\
	    readonly_data_section (); 					\
        }								\
      else								\
        data_section ();						\
      try_section_alias ();						\
    }									\
  while (0)

/* APPLE LOCAL const_data */
/* This can be called with address expressions as "rtx".
   They must go in "const_data" (although "const" will work in some
   cases where the relocation can be resolved at static link time). */
#undef	SELECT_RTX_SECTION
#define SELECT_RTX_SECTION(mode, rtx, align)				\
  do									\
    {									\
      if (GET_MODE_SIZE (mode) == 8)					\
	literal8_section ();						\
      else if (GET_MODE_SIZE (mode) == 4				\
	       && (GET_CODE (rtx) == CONST_INT				\
	           || GET_CODE (rtx) == CONST_DOUBLE))			\
	literal4_section ();						\
      else								\
	const_data_section ();						\
    }									\
  while (0)

#define ASM_DECLARE_UNRESOLVED_REFERENCE(FILE,NAME)			\
    do { 								\
	 if (FILE) {							\
	   /* APPLE LOCAL dynamic-no-pic  */				\
	   if (MACHOPIC_INDIRECT)					\
	     fprintf (FILE, "\t.lazy_reference ");			\
	   else								\
	     fprintf (FILE, "\t.reference ");				\
	   assemble_name (FILE, NAME);					\
	   fprintf (FILE, "\n");					\
	 }                                                              \
       } while (0)

#define ASM_DECLARE_CLASS_REFERENCE(FILE,NAME)				\
    do {								\
	 if (FILE) {							\
	   fprintf (FILE, "\t");					\
	   assemble_name (FILE, NAME); 					\
	   fprintf (FILE, "=0\n");					\
	   assemble_global (NAME);					\
	 }								\
       } while (0)

#undef ASM_GLOBALIZE_LABEL
#define ASM_GLOBALIZE_LABEL(FILE,NAME)	\
 do { const char *const _x = (NAME); if (!!strncmp (_x, "_OBJC_", 6)) { \
  (fputs (".globl ", FILE), assemble_name (FILE, _x), fputs ("\n", FILE)); \
 }} while (0)

/* APPLE LOCAL private extern */
#define ASM_PRIVATE_EXTERNIZE_LABEL(FILE, NAME)				\
 do { const char* _x = (NAME); if (!!strncmp (_x, "_OBJC_", 6)) {	\
  fputs (".private_extern ", FILE); assemble_name (FILE, _x);		\
  fputs ("\n", FILE); }} while (0)
/* APPLE LOCAL end private extern */

#if !defined(APPLE_WEAK_SECTION_ATTRIBUTE) && !defined(APPLE_WEAK_ASSEMBLER_DIRECTIVE)
  #warning Neither weak attributes nor weak directives defined.
  #warning Assuming weak directives.
#define APPLE_WEAK_ASSEMBLER_DIRECTIVE 1
#endif 

/* APPLE LOCAL weak definition */
#ifdef APPLE_WEAK_ASSEMBLER_DIRECTIVE
#define ASM_WEAK_DEFINITIONIZE_LABEL(FILE,  NAME)                       \
 do { const char* _x = (NAME); if (!!strncmp (_x, "_OBJC_", 6)) {	\
  fputs (".weak_definition ", FILE); assemble_name (FILE, _x);		\
  fputs ("\n", FILE); }} while (0)
#endif
/* APPLE LOCAL end weak definition */

#undef ASM_GENERATE_INTERNAL_LABEL
#define ASM_GENERATE_INTERNAL_LABEL(LABEL,PREFIX,NUM)	\
  sprintf (LABEL, "*%s%ld", PREFIX, (long)(NUM))

/* This is how to output an internal numbered label where PREFIX is
   the class of label and NUM is the number within the class.  */

#undef ASM_OUTPUT_INTERNAL_LABEL
#define ASM_OUTPUT_INTERNAL_LABEL(FILE,PREFIX,NUM)	\
  fprintf (FILE, "%s%d:\n", PREFIX, NUM)

/* Since we have a separate readonly data section, define this so that
   jump tables end up in text rather than data.  */

#ifndef JUMP_TABLES_IN_TEXT_SECTION
#define JUMP_TABLES_IN_TEXT_SECTION 1
#endif

/* Symbolic names for various things we might know about a symbol.  */

enum machopic_addr_class {
  MACHOPIC_UNDEFINED,
  MACHOPIC_DEFINED_DATA,
  MACHOPIC_UNDEFINED_DATA,
  MACHOPIC_DEFINED_FUNCTION,
  MACHOPIC_UNDEFINED_FUNCTION
};

/* Macros defining the various PIC cases.  */

/* APPLE LOCAL begin dynamic-no-pic */
#define MACHO_DYNAMIC_NO_PIC_P()	(TARGET_DYNAMIC_NO_PIC)
#define MACHOPIC_INDIRECT	(flag_pic || MACHO_DYNAMIC_NO_PIC_P ())
#define MACHOPIC_JUST_INDIRECT	(flag_pic == 1 || MACHO_DYNAMIC_NO_PIC_P ())
#define MACHOPIC_PURE		(flag_pic == 2 && ! MACHO_DYNAMIC_NO_PIC_P ())

/* APPLE LOCAL PFE */
/* Used to check for consistent target settings between load and dump
   files.  */
#ifdef PFE
#define PFE_CHECK_TARGET_SETTINGS() (darwin_pfe_check_target_settings ())
#endif
/* APPLE LOCAL end dynamic-no-pic */

/* APPLE LOCAL darwin native */
#undef ENCODE_SECTION_INFO
#define ENCODE_SECTION_INFO(DECL)  \
  darwin_encode_section_info (DECL)

/* Be conservative and always redo the encoding.  */

#define REDO_SECTION_INFO_P(DECL) (1)

#define STRIP_NAME_ENCODING(VAR,SYMBOL_NAME)  \
  ((VAR) = ((SYMBOL_NAME[0] == '!') ? (SYMBOL_NAME) + 4 : (SYMBOL_NAME)))

#define GEN_BINDER_NAME_FOR_STUB(BUF,STUB,STUB_LENGTH)		\
  do {								\
    const char *const stub_ = (STUB);				\
    char *buffer_ = (BUF);					\
    strcpy (buffer_, stub_);					\
    if (stub_[0] == '"')					\
      {								\
	strcpy (buffer_ + (STUB_LENGTH) - 1, "_binder\"");	\
      }								\
    else							\
      {								\
	strcpy (buffer_ + (STUB_LENGTH), "_binder");		\
      }								\
  } while (0)

#define GEN_SYMBOL_NAME_FOR_SYMBOL(BUF,SYMBOL,SYMBOL_LENGTH)	\
  do {								\
    const char *const symbol_ = (SYMBOL);			\
    char *buffer_ = (BUF);					\
    if (name_needs_quotes (symbol_) && symbol_[0] != '"')	\
      {								\
	  sprintf (buffer_, "\"%s\"", symbol_);			\
      }								\
    else							\
      {								\
	strcpy (buffer_, symbol_);				\
      }								\
  } while (0)

/* Given a symbol name string, create the lazy pointer version
   of the symbol name.  */

#define GEN_LAZY_PTR_NAME_FOR_SYMBOL(BUF,SYMBOL,SYMBOL_LENGTH)	\
  do {								\
    const char *symbol_ = (SYMBOL);				\
    char *buffer_ = (BUF);					\
    STRIP_NAME_ENCODING (symbol_, symbol_);  \
    if (symbol_[0] == '"')					\
      {								\
        strcpy (buffer_, "\"L");					\
        strcpy (buffer_ + 2, symbol_ + 1);			\
	strcpy (buffer_ + (SYMBOL_LENGTH), "$lazy_ptr\"");	\
      }								\
    else if (name_needs_quotes (symbol_))			\
      {								\
        strcpy (buffer_, "\"L");				\
        strcpy (buffer_ + 2, symbol_);				\
	strcpy (buffer_ + (SYMBOL_LENGTH) + 2, "$lazy_ptr\"");	\
      }								\
    else							\
      {								\
        strcpy (buffer_, "L");					\
        strcpy (buffer_ + 1, symbol_);				\
	strcpy (buffer_ + (SYMBOL_LENGTH) + 1, "$lazy_ptr");	\
      }								\
  } while (0)

#define TARGET_ASM_EXCEPTION_SECTION darwin_exception_section

#define TARGET_ASM_EH_FRAME_SECTION darwin_eh_frame_section

/* APPLE LOCAL begin C++ EH turly 20020208  */
/* The __eh_frame section attributes: a "normal" section by default.  */
#define EH_FRAME_SECTION_ATTR	/*nothing*/

/* The only EH item we can't do PC-relative is the reference to
   __gxx_personality_v0.  So we cheat, since moving the __eh_frame section
   to the DATA segment is expensive.
   We output a 4-byte encoding - including the last 2 chars of the 
   personality function name: {0, 'g', 'v', '0', 0xff}
   (The first zero byte coincides with the "absolute" encoding.)
   This means we can now use DW_EH_PE_pcrel for everything.  And there
   was much rejoicing.  */

#define EH_FRAME_SECTION_NAME	"__TEXT"

#define COALESCED_UNWIND_INFO

#ifdef COALESCED_UNWIND_INFO
#undef EH_FRAME_SECTION_ATTR

#ifdef APPLE_WEAK_SECTION_ATTRIBUTE
#define APPLE_EH_FRAME_WEAK_DEFINITIONS ",weak_definitions"
#else /* APPLE_WEAK_SECTION_ATTRIBUTE */
#define APPLE_EH_FRAME_WEAK_DEFINITIONS ""
#endif /* APPLE_WEAK_SECTION_ATTRIBUTE */

#ifdef APPLE_STRIP_STATIC_SYMS_SECTION_ATTRIBUTE
#define APPLE_EH_FRAME_STRIP_STATIC "+strip_static_syms"
#else /* APPLE_STRIP_STATIC_SYMS_SECTION_ATTRIBUTE */
#define APPLE_EH_FRAME_STRIP_STATIC ""
#endif /* APPLE_STRIP_STATIC_SYMS_SECTION_ATTRIBUTE */

#define EH_FRAME_SECTION_ATTR \
        ",coalesced" APPLE_EH_FRAME_WEAK_DEFINITIONS ",no_toc" APPLE_EH_FRAME_STRIP_STATIC


/* Implicit or explicit template instantiations' EH info are GLOBAL
   symbols.  ("Implicit" here implies "coalesced".)
   Note that .weak_definition is commented out until 'as' supports it.  */


#ifdef APPLE_WEAK_ASSEMBLER_DIRECTIVE
#define APPLE_ASM_WEAK_DEF_FMT_STRING(LAB) \
      (name_needs_quotes(LAB) ? ".weak_definition \"%s.eh\"\n" : ".weak_definition %s.eh\n")
#else /* APPLE_WEAK_ASSEMBLER_DIRECTIVE */
#define APPLE_ASM_WEAK_DEF_FMT_STRING(LAB) \
      (name_needs_quotes(LAB) ? ";.weak_definition \"%s.eh\"\n" : ";.weak_definition %s.eh\n")
#endif /* APPLE_WEAK_ASSEMBLER_DIRECTIVE */

#define ASM_OUTPUT_COAL_UNWIND_LABEL(FILE, LAB, COAL, PUBLIC, PRIVATE_EXTERN) \
  do {									\
    if ((COAL) || (PUBLIC) || (PRIVATE_EXTERN))                         \
      fprintf ((FILE),							\
	       (name_needs_quotes(LAB) ? "%s \"%s.eh\"\n" : "%s %s.eh\n"), \
	       ((PUBLIC) ? ".globl" : ".private_extern"),               \
	       (LAB));							\
    if (COAL)								\
      fprintf ((FILE),							\
	       APPLE_ASM_WEAK_DEF_FMT_STRING(LAB),			\
	       (LAB));							\
    fprintf ((FILE), 							\
	     (name_needs_quotes(LAB) ? "\"%s.eh\":\n" : "%s.eh:\n"),    \
	     (LAB));							\
  } while (0)

#endif	/* COALESCED_UNWIND_INFO  */

#undef ASM_PREFERRED_EH_DATA_FORMAT
#define ASM_PREFERRED_EH_DATA_FORMAT(CODE,GLOBAL)  \
  (((CODE) == 2 && (GLOBAL) == 1) \
   ? (DW_EH_PE_pcrel | DW_EH_PE_indirect) : \
     ((CODE) == 1 || (GLOBAL) == 0) ? DW_EH_PE_pcrel : DW_EH_PE_absptr)

/*** temporarily disabled ("XXX_") for compatibility with -all_load ***/
#define XXX_DW2_ENCODE_PERSONALITY_FUNC(ADDR)				\
  do {									\
      const char *str = XSTR (ADDR, 0);					\
      extern rtx personality_libfunc_used;				\
      personality_libfunc_used = (ADDR);				\
      str += strlen (str) - 2;						\
      dw2_asm_output_data (1, 0, "(encoding");				\
      dw2_asm_output_data (1, 'g', "for");				\
      dw2_asm_output_data (1, str[0], "personality");			\
      dw2_asm_output_data (1, str[1], "function");			\
      STRIP_NAME_ENCODING (str, XSTR (ADDR, 0));			\
      dw2_asm_output_data (1, 0xff, "%s)", str);			\
  } while (0)

/* TRUE if PTR encodes a personality function.
   For now, all we know about is __gxx_personality_v0, encoded
   by {0, 'g', 'v', '0', 0xff}.  */

/*** temporarily disabled ("XXX_") for compatibility with -all_load ***/
#define XXX_DW2_PERSONALITY_FUNC_ENCODED_P(PTR)				\
  (PTR[0] == 0 && PTR[1] == 'g' && PTR[2] == 'v'			\
				&& PTR[3] == '0' && PTR[4] == 0xFF)

/* Fromage to "extract" the encoded personality function.  Bump PTR.  */
/*** temporarily disabled ("XXX_") for compatibility with -all_load ***/
#define XXX_DW2_DECODE_PERSONALITY_FUNC(PTR, PFUNC)			\
  do {									\
    if (DW2_PERSONALITY_FUNC_ENCODED_P (PTR)) {				\
      extern int __gxx_personality_v0 ();				\
      PFUNC = __gxx_personality_v0;					\
      PTR += 5;								\
    }									\
  } while (0)

#define ASM_MAYBE_OUTPUT_ENCODED_ADDR_RTX(ASM_OUT_FILE, ENCODING, SIZE, ADDR, DONE)	\
      if (ENCODING == ASM_PREFERRED_EH_DATA_FORMAT (2, 1)) {				\
	darwin_non_lazy_pcrel (ASM_OUT_FILE, ADDR);					\
	goto DONE;									\
      }


/* Node of KEYMGR_GCC3_LIVE_IMAGE_LIST.  Info about each resident image.  */
struct __live_images {
  unsigned long this_size;                      /* sizeof (live_images)  */
  struct mach_header *mh;                       /* the image info  */
  unsigned long vm_slide;
  void (*destructor)(struct __live_images *);   /* destructor for this  */
  struct __live_images *next;
  unsigned int examined_p;
  void *fde;
  void *object_info;
  unsigned long info[2];                        /* GCC3 use  */
};

/* Define if we want the "struct object" in unwind-dw2-fde.h to be extended
   with a "dwarf_fde_end" field.  */
#define DWARF2_OBJECT_END_PTR_EXTENSION

/* APPLE LOCAL end C++ EH turly 20020208  */

#define REGISTER_TARGET_PRAGMAS(PFILE)                          \
  do {                                                          \
    cpp_register_pragma (PFILE, 0, "mark", darwin_pragma_ignore);  \
    cpp_register_pragma (PFILE, 0, "options", darwin_pragma_options);  \
    /* APPLE LOCAL begin Macintosh alignment 2002-1-22 ff */  \
    cpp_register_pragma (PFILE, 0, "pack", darwin_pragma_pack);  \
    /* APPLE LOCAL end Macintosh alignment 2002-1-22 ff */  \
    cpp_register_pragma (PFILE, 0, "segment", darwin_pragma_ignore);  \
    cpp_register_pragma (PFILE, 0, "unused", darwin_pragma_unused);  \
    /* APPLE LOCAL begin CALL_ON_LOAD/CALL_ON_UNLOAD pragmas  20020202 turly  */ \
    cpp_register_pragma (PFILE, 0, "CALL_ON_LOAD", \
					darwin_pragma_call_on_load); \
    cpp_register_pragma (PFILE, 0, "CALL_ON_UNLOAD", \
					darwin_pragma_call_on_unload); \
    /* APPLE LOCAL end CALL_ON_LOAD/CALL_ON_UNLOAD pragmas  20020202 turly  */ \
    /* APPLE LOCAL begin CALL_ON_MODULE_BIND deprecated 2002-4-10 ff */  \
    cpp_register_pragma (PFILE, 0, "CALL_ON_MODULE_BIND", darwin_pragma_call_on_module_bind);  \
    /* APPLE LOCAL end CALL_ON_MODULE_BIND deprecated 2002-4-10 ff */  \
    /* APPLE LOCAL begin temporary pragmas 2001-07-05 sts */  \
    cpp_register_pragma (PFILE, 0, "CC_NO_MACH_TEXT_SECTIONS", darwin_pragma_cc_no_mach_text_sections);  \
    cpp_register_pragma (PFILE, 0, "CC_OPT_OFF", darwin_pragma_cc_opt_off);  \
    cpp_register_pragma (PFILE, 0, "CC_OPT_ON", darwin_pragma_cc_opt_on);  \
    cpp_register_pragma (PFILE, 0, "CC_OPT_RESTORE", darwin_pragma_cc_opt_restore);  \
    cpp_register_pragma (PFILE, 0, "CC_WRITABLE_STRINGS", darwin_pragma_cc_writable_strings);  \
    cpp_register_pragma (PFILE, 0, "CC_NON_WRITABLE_STRINGS", darwin_pragma_cc_non_writable_strings);  \
    /* APPLE LOCAL end temporary pragmas 2001-07-05 sts */  \
  } while (0)

/* APPLE LOCAL darwin native */
#undef ASM_APP_ON
#define ASM_APP_ON ""
#undef ASM_APP_OFF
#define ASM_APP_OFF ""

/* APPLE LOCAL  coalescing  */
extern void make_decl_coalesced (tree, int private_extern_p);

/* Coalesced symbols are private extern by default.  This behavior can
   be changed with the EXPERIMENTAL export-coalesced flag.  There is 
   not (yet?) any means for coalesced symbols to be selectively exported.  */

#define MAKE_DECL_COALESCED(DECL) \
        make_decl_coalesced (DECL, !flag_export_coalesced)

#define COALESCE_STATIC_THUNK(DECL, PUBLIC) \
        make_decl_coalesced (DECL, !PUBLIC)

extern int flag_coalescing_enabled,
	   flag_coalesce_templates, flag_weak_coalesced_definitions;

/* Coalesced symbols are private extern by default.  This EXPERIMENTAL
   flag will make them global instead.  */
extern int flag_export_coalesced;

#define COALESCING_ENABLED_P()  (flag_coalescing_enabled && MACHOPIC_INDIRECT)

#define COALESCING_TEMPLATES_P(DECL)				\
        (COALESCING_ENABLED_P () && flag_coalesce_templates)

#define MARK_TEMPLATE_COALESCED(DECL)					\
  do {									\
    if (COALESCING_TEMPLATES_P (DECL)) {				\
      int explicit = TREE_PUBLIC (DECL)					\
	&& (DECL_EXPLICIT_INSTANTIATION (DECL)				\
	    /* Or an explicitly instantiated function.  */		\
	    || (TREE_CODE (DECL) == FUNCTION_DECL			\
		&& DECL_INTERFACE_KNOWN (DECL)				\
		&& DECL_NOT_REALLY_EXTERN (DECL))			\
	    /* Or a non-common VAR_DECL.  */				\
	    || (TREE_CODE (DECL) == VAR_DECL && ! DECL_COMMON (DECL)));	\
      if (!explicit							\
	  || /*it IS explicit, but*/ !flag_weak_coalesced_definitions)	\
        MAKE_DECL_COALESCED (DECL);					\
    }							\
  } while (0)

#undef TARGET_ASM_NAMED_SECTION
#define TARGET_ASM_NAMED_SECTION darwin_asm_named_section
#undef TARGET_SECTION_TYPE_FLAGS
#define TARGET_SECTION_TYPE_FLAGS darwin_section_type_flags

#define DECL_IS_COALESCED_OR_WEAK(DECL)			\
	(DECL_COALESCED (DECL) || DECL_WEAK (DECL))

extern int machopic_var_referred_to_p PARAMS ((const char*)); 
#define MACHOPIC_VAR_REFERRED_TO_P(NAME) machopic_var_referred_to_p (NAME)
/* APPLE LOCAL  end coalescing  */

/* APPLE LOCAL insert assembly ".abort" directive on fatal error   */
#define EXIT_FROM_FATAL_DIAGNOSTIC(status) abort_assembly_and_exit (status)
extern void abort_assembly_and_exit (int status) ATTRIBUTE_NORETURN;

/* APPLE LOCAL begin Macintosh alignment 2002-2-13 ff */
#ifdef RS6000_VECTOR_ALIGNMENT
/* When adjusting (lowering) the alignment of fields when in the
   mac68k alignment mode, the 128-bit alignment of vectors *MUST*
   be preserved.  */
#define PEG_ALIGN_FOR_MAC68K(DESIRED)           \
        ((flag_altivec && (DESIRED) == RS6000_VECTOR_ALIGNMENT) \
         ? RS6000_VECTOR_ALIGNMENT              \
         : MIN ((DESIRED), 16))
#else
#define PEG_ALIGN_FOR_MAC68K(DESIRED)   MIN ((DESIRED), 16)
#endif 
/* APPLE LOCAL end Macintosh alignment 2002-2-13 ff */

/* APPLE LOCAL begin double destructor turly 20020214  */
/* Handle __attribute__((apple_kext_compatibility)).  This shrinks the
   vtable for all classes with this attribute (and their descendants)
   back to 2.95 dimensions.  It causes only the deleting destructor to
   be emitted, which means that such objects CANNOT be allocated on the
   stack or as globals.  Luckily, this fits in with the Darwin kext model. */

#define SUBTARGET_ATTRIBUTE_TABLE					      \
  /* { name, min_len, max_len, decl_req, type_req, fn_type_req, handler } */  \
  { "apple_kext_compatibility", 0, 0, 0, 1, 0, darwin_handle_odd_attribute }

/* APPLE KEXT stuff -- only applies with pure static C++ code.  */
/* NB: Can't use flag_apple_kext as it's in the C++ FE, and this macro
   is used in the back end for the above __attribute__ handler.  */
#define POSSIBLY_COMPILING_APPLE_KEXT_P()		\
	(! MACHOPIC_INDIRECT && c_language == clk_cplusplus)

/* Need a mechanism to tell whether a C++ operator delete is empty so
   we overload TREE_SIDE_EFFECTS here (it is unused for FUNCTION_DECLS.)
   Fromage, c'est moi!  */
#define CHECK_TRIVIAL_FUNCTION(DECL)					\
    do {								\
      const char *_name = IDENTIFIER_POINTER (DECL_NAME (DECL));	\
      if (POSSIBLY_COMPILING_APPLE_KEXT_P () && DECL_SAVED_TREE (DECL)	\
	  && strstr (_name, "operator delete")				\
	  && TREE_CODE (DECL_SAVED_TREE (DECL)) == COMPOUND_STMT	\
	  && compound_body_is_empty_p (					\
				COMPOUND_BODY (DECL_SAVED_TREE (DECL))))\
	  TREE_SIDE_EFFECTS (DECL) = 1;					\
    } while (0)

/* gcc3 initialises the vptr field of all objects so that it points at the
   first virtual function slot, NOT the base address of the vtable proper.
   This is different from gcc2.95 which always initialised the vptr to
   point at the base address of the vtable.  The difference here is 8 bytes.
   So, for 2.95 compatibility, we need to:

     (1) subtract 8 from the vptr initialiser, and
     (2) add 2 to every vfunc index.  (2 * 4 == 8.)

   This is getting ever cheesier.  */

#define VPTR_INITIALIZER_ADJUSTMENT	8
#define ADJUST_VTABLE_INDEX(IDX, VTBL)					\
    do {								\
      if (POSSIBLY_COMPILING_APPLE_KEXT_P () && flag_apple_kext)	\
	(IDX) = fold (build (PLUS_EXPR, TREE_TYPE (IDX), IDX, size_int (2))); \
    } while (0)

/* APPLE LOCAL end double destructor turly 20020214  */

/* APPLE LOCAL begin zerofill turly 20020218  */
#define ASM_OUTPUT_ZEROFILL(FILE, NAME, SIZE, ALIGNMENT)        \
do { fputs (".zerofill __DATA, __common, ", (FILE));            \
        assemble_name ((FILE), (NAME));                         \
        fprintf ((FILE), ", %u, %u\n", (SIZE), (ALIGNMENT));    \
        in_section = no_section;                                \
      } while (0)
/* APPLE LOCAL end zerofill turly 20020218  */

#endif /* CONFIG_DARWIN_H  */

