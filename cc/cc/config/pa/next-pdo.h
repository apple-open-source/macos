/* Target definitions for GNU compiler for hppa running HPUX with NeXT pdo
   Copyright (C) 1989, 1990, 1991, 1992, 1993 Free Software Foundation, Inc.

#define USE_GAS

#define OBJC_FORWARDING_REG_OFFSET(ISREG, OFF, REGNO) \
  do { OFF = (FP_REG_CLASS_P( REGNO_REG_CLASS(REGNO)) \
              ? -2 * ((REGNO) - 44) - 4               \
              :  4 * ((REGNO) - 26) - 4);             \
       ISREG = 0; } while (0)

/* pa1-ghpux originally includes pa-ghpux.h and pa.h */
#include "pa/pa1-ghpux.h"

#define NEXT_OBJC_RUNTIME

#define OBJC_HPUX_PADDING

#define NEXT_PDO

/* Report errors to make application. */

#define REPORT_EVENT(TYPE, NAME, FILE, LINE, MSG, ARG1, ARG2, ARG3)	\
  make_support (TYPE, NAME, FILE, LINE, MSG, ARG1, ARG2, ARG3)
#define V_REPORT_EVENT(TYPE, NAME, FILE, LINE, MSG, AP)			\
  v_make_support (TYPE, NAME, FILE, LINE, MSG, AP)

/* Enable code that handles Apple's Pascal strings.  */

#define PASCAL_STRINGS

/* Enable code that handles four-character constants.  */

#define FOUR_CHAR_CONSTANTS

/* Return small structs in registers */
#undef DEFAULT_PCC_STRUCT_RETURN
#define DEFAULT_PCC_STRUCT_RETURN 0

#define readonly_data_section data_section
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

/*
#define REPORT_EVENT(TYPE, NAME, FILE, LINE, MSG, ARG1, ARG2, ARG3)	\
  make_support (TYPE, NAME, FILE, LINE, MSG, ARG1, ARG2, ARG3)

/*
 * override default prefix behavior
 */
/*
#define STANDARD_EXEC_PREFIX "/Developer/Libraries/gcc-lib/"
#define STANDARD_STARTFILE_PREFIX "/Developer/Libraries/"
#define TOOLDIR_BASE_PREFIX "/Developer/"
*/

/*
 * specs
 *
 */

/* The following profiling switch handling doesn't seem to make sense!
   Part of it came from next-specs.h, and part from pa.h - not sure which
   is correct.  The compiler doesn't complain when -p is passed to it,
   though.  Note that profiling cannot be used in conjunction with -fPIC -
   this can cause the compiler to crash!  */

#undef  CC1_SPEC
#define CC1_SPEC "%{!static:-fPIC}"

/* According to the NEWS file, -fhandle-exceptions is known to be buggy in
   conjunction with -O.  One can certainly avoid a lot of problems by disabling
   inlining when -fhandle-exceptions is present on the command line.  */
#define CC1PLUS_SPEC "%{fhandle-exceptions:-fno-inline -fkeep-inline-functions}"

#undef LINK_SPEC
#define LINK_SPEC "-u main -a shared_archive %{!static:+s +b:}"

#undef LIB_SPEC
#define LIB_SPEC "%{p:-L/lib/libp }%{pg:%{!p:-L/lib/libp }}%{!nopdolib:-lpdo }-lpthread -lm -lc"

#undef CPP_PREDEFINES
#define CPP_PREDEFINES "-DNeXT_PDO -Dhppa -Dhp9000s800 -D__hp9000s800 -Dhp9k8 -DPWB -Dhpux -Dunix -D_HPUX_SOURCE -Asystem(unix) -Asystem(hpux) -Acpu(hppa) -Amachine(hppa) -D__BIG_ENDIAN__ -D__ARCHITECTURE__=\"hppa\" -Dcma_read=read -Dcma_write=write"

#undef TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (hppa1.1-nextpdo-hpux)");

#ifndef STARTFILE_SPEC
#define STARTFILE_SPEC  \
  "%{pg:gcrt0.o%s}%{!pg:%{p:mcrt0.o%s}%{!p:crt0.o%s}}"
#endif

/* The compiler crashes when both profiling and -f{pic,PIC} are enabled.  */

void override_options ();

#undef  OVERRIDE_OPTIONS
#define OVERRIDE_OPTIONS \
  do {									\
    if (profile_flag && flag_pic)					\
      {									\
	warning ("%s and profiling conflict: disabling profiling",	\
		 (flag_pic == 1) ? "-fpic" : "-fPIC");			\
	profile_flag = 0;						\
      }									\
    override_options ();						\
  } while (0)

/* RIPPED OFF FROM nextstep.h */
#undef WORD_SWITCH_TAKES_ARG
#define WORD_SWITCH_TAKES_ARG(STR)				\
  (DEFAULT_WORD_SWITCH_TAKES_ARG (STR)				\
   || !strcmp (STR, "target") 					\
   || !strcmp (STR, "assert")					\
   || !strcmp (STR, "arch")					\
   || !strcmp (STR, "filelist")					\
   || !strcmp (STR, "framework"))
 

#undef	WORD_SWITCH
#define WORD_SWITCH(STR)			\
  (WORD_SWITCH_TAKES_ARG (STR)			\
   || !strcmp (STR, "bsd")			\
   || !strcmp (STR, "object")			\
   || !strcmp (STR, "ObjC")			\
   || !strcmp (STR, "all_load"))

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
	if (FUNCTION_NAME_P (NAME) && (NAME[1] == '+' || NAME[1] == '-')) \
		fprintf (FILE, "\"%s\"", NAME+1); \
	else if (NAME[0] == '+' || NAME[0] == '-') \
		fprintf (FILE, "\"%s\"", NAME[0]); \
	else if (!strncmp (NAME, "_OBJC_", 6)) \
		fprintf (FILE, "L%s", NAME);   \
	else if (!strncmp (NAME, ".objc_class_name_", 17))		\
		fprintf (FILE, "P'%s", NAME);				\
	else fprintf ((FILE), "%s", (NAME) + (FUNCTION_NAME_P (NAME) ? 1 : 0)); } while (0)

/* Modified version of the macro in pa.h.

   We make the first line stab special to avoid adding several
   gross hacks to GAS.  */

#undef ASM_OUTPUT_SOURCE_LINE
#define ASM_OUTPUT_SOURCE_LINE(file, line)		\
  { static int sym_lineno = 1;				\
    static tree last_function_decl = NULL;		\
    if (current_function_decl == last_function_decl) {	\
      char* label = XSTR (XEXP (DECL_RTL (current_function_decl), 0), 0) + 1; \
      if (label[0] == '+' || label[0] == '-')		\
	    fprintf (file, "\t.stabn 68,0,%d,L$M%d-\"%s\"\nL$M%d:\n", \
		    line, sym_lineno,			\
		    label, 				\
		    sym_lineno);			\
      else 						\
	    fprintf (file, "\t.stabn 68,0,%d,L$M%d-%s\nL$M%d:\n", \
		    line, sym_lineno,			\
		    label, 				\
		    sym_lineno);			\
      }							\
    else						\
      fprintf (file, "\t.stabn 68,0,%d,0\n", line);	\
    last_function_decl = current_function_decl;		\
    sym_lineno += 1; }

/* This is how to output the definition of a user-level label named NAME,
   such as the label on a static function or variable NAME.  */

#undef ASM_OUTPUT_LABEL
#define ASM_OUTPUT_LABEL(FILE, NAME)	\
  do { assemble_name (FILE, NAME); 	\
	 fputc (':', FILE);		\
       fputc ('\n', FILE); } while (0)

/* This is how to output an assembler line defining an `int' constant.  
	this is used in the OBJC_INSTANCE_METHODS segment 
	removed code that put P% infront of code being linked to shared libs 
 */

#undef	ASM_OUTPUT_INT
#define ASM_OUTPUT_INT(FILE,VALUE)  \
{ fprintf (FILE, "\t.word ");			\
  if (function_label_operand (VALUE, VOIDmode))	\
    fprintf (FILE, "P'");			\
  output_addr_const (FILE, (VALUE));		\
  fprintf (FILE, "\n");}



#undef ASM_GLOBALIZE_LABEL
#define ASM_GLOBALIZE_LABEL(FILE, NAME)					\
  if (strncmp(NAME,"_OBJC_PROTOCOL_",15))                                   \
  do { fputs ("\t.EXPORT ", FILE); assemble_name (FILE, NAME);		\
       if (FUNCTION_NAME_P (NAME))					\
	 fputs (",CODE\n", FILE);					\
       else								\
	 fputs (",DATA\n", FILE);} while (0)

/* from josh */


#undef	SECTION_FUNCTION
#define SECTION_FUNCTION(FUNCTION, SECTION, SUBSPACE)			\
void									\
FUNCTION ()								\
{									\
  extern void text_section ();					 	\
  extern void objc_section_init ();					\
  extern int flag_no_mach_text_sections;				\
  static int been_here = 0;						\
 									\
  if (in_section != SECTION)						\
    {									\
      objc_section_init ();						\
      fprintf (asm_out_file, "\t.SPACE $PRIVATE$\n");			\
      fprintf (asm_out_file, "\t.SUBSPA %s", SUBSPACE);			\
      if (!been_here) {							\
        fprintf(asm_out_file,",QUAD=1,ALIGN=4,ACCESS=31");		\
	been_here = 1;							\
	}								\
      fprintf(asm_out_file,"\n");					\
      in_section = SECTION;						\
    }									\
}									\

#undef	EXTRA_SECTIONS
#define EXTRA_SECTIONS	in_bss, in_readonly_data,	\
  in_objc_class, in_objc_meta_class, in_objc_category,	\
  in_objc_class_vars, in_objc_instance_vars,		\
  in_objc_cls_meth, in_objc_inst_meth,			\
  in_objc_cat_cls_meth, in_objc_cat_inst_meth,		\
  in_objc_selector_refs,				\
  in_objc_symbols, in_objc_module_info,			\
  in_objc_protocol, in_objc_string_object,		\
  in_objc_class_names, in_objc_meth_var_names,		\
  in_objc_meth_var_types, in_objc_cls_refs

#undef	EXTRA_SECTION_FUNCTIONS
#define EXTRA_SECTION_FUNCTIONS			\
void						\
bss_section ()					\
{						\
  if (in_section != in_bss)			\
    {						\
      fprintf (asm_out_file, "%s\n", 		\
	       BSS_SECTION_ASM_OP);		\
      in_section = in_bss;			\
    }						\
}						\
void						\
readonly_data ()				\
{						\
  if (in_section != in_readonly_data)		\
    {						\
      fprintf (asm_out_file, "%s\n", 		\
	       READONLY_DATA_ASM_OP);		\
      in_section = in_readonly_data;		\
    }						\
}						\
SECTION_FUNCTION (objc_class_section,		\
		  in_objc_class,		\
		  "$$OBJC_CLASS$$")		\
SECTION_FUNCTION (objc_meta_class_section,	\
		  in_objc_meta_class,		\
		  "$$OBJC_METACLASS$$")		\
SECTION_FUNCTION (objc_category_section,	\
		  in_objc_category,		\
		 "$$OBJC_CATEGORY$$")		\
SECTION_FUNCTION (objc_class_vars_section,	\
		  in_objc_class_vars,		\
		  "$$OBJC_CLASS_VARS$$")	\
SECTION_FUNCTION (objc_instance_vars_section,	\
		  in_objc_instance_vars,	\
		  "$$OBJC_INSTANCE_VARS$$")	\
SECTION_FUNCTION (objc_cls_meth_section,	\
		  in_objc_cls_meth,		\
		  "$$OBJC_CLS_METH$$")		\
SECTION_FUNCTION (objc_inst_meth_section,	\
		  in_objc_inst_meth,		\
		  "$$OBJC_INST_METHODS$$")	\
SECTION_FUNCTION (objc_cat_cls_meth_section,	\
		  in_objc_cat_cls_meth,		\
		  "$$OBJC_CAT_CLS_METH$$")	\
SECTION_FUNCTION (objc_cat_inst_meth_section,	\
		  in_objc_cat_inst_meth,	\
		  "$$OBJC_CAT_INST_METH$$")	\
SECTION_FUNCTION (objc_selector_refs_section,	\
		  in_objc_selector_refs,	\
		  "$$OBJC_MESSAGE_REFS$$")	\
SECTION_FUNCTION (objc_symbols_section,		\
		  in_objc_symbols,		\
		  "$$OBJC_SYMBOLS$$")		\
SECTION_FUNCTION (objc_module_info_section,	\
		  in_objc_module_info,		\
		  "$$OBJC_MODULES$$")		\
SECTION_FUNCTION (objc_protocol_section,	\
		  in_objc_protocol,		\
		  "$$OBJC_PROTOCOL$$")		\
SECTION_FUNCTION (objc_string_object_section,	\
		  in_objc_string_object,	\
		  "$$OBJC_STRING_OBJECT$$")	\
SECTION_FUNCTION (objc_class_names_section,	\
		in_objc_class_names,		\
		"$$OBJC_CLASS_NAMES$$")		\
SECTION_FUNCTION (objc_meth_var_names_section,	\
		in_objc_meth_var_names,		\
		"$$OBJC_METH_VAR_NAMES$$")	\
SECTION_FUNCTION (objc_meth_var_types_section,	\
		in_objc_meth_var_types,		\
		"$$OBJC_METH_VAR_TYPES$$")	\
SECTION_FUNCTION (objc_cls_refs_section,	\
		in_objc_cls_refs,		\
		"$$OBJC_CLS_REFS$$")		\
						\
void						\
objc_section_init ()				\
{						\
  static int been_here = 0;			\
						\
  if (been_here == 0)				\
    {						\
      been_here = 1;				\
      objc_class_section ();			\
      objc_meta_class_section ();		\
      objc_cat_cls_meth_section ();		\
      objc_cat_inst_meth_section ();		\
      objc_cls_meth_section ();			\
      objc_inst_meth_section ();		\
      objc_selector_refs_section ();		\
      objc_symbols_section ();			\
      objc_category_section ();			\
      objc_protocol_section ();			\
      objc_class_vars_section ();		\
      objc_instance_vars_section ();		\
      objc_module_info_section ();		\
      objc_string_object_section ();		\
      objc_class_names_section ();		\
      objc_meth_var_names_section ();		\
      objc_meth_var_types_section ();		\
      objc_cls_refs_section ();			\
    }						\
}

/* different from NS version in that no literalX_section */ 
#undef	SELECT_SECTION
#define SELECT_SECTION(exp,reloc)				\
  do								\
    {								\
      if (TREE_CODE (exp) == STRING_CST)			\
	{							\
	  if (flag_writable_strings)				\
	    data_section ();					\
	  else 	\
	    readonly_data_section ();				\
	}							\
      else if (TREE_CODE (exp) == INTEGER_CST			\
	       || TREE_CODE (exp) == REAL_CST)			\
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
	  if (!strcmp (IDENTIFIER_POINTER (TYPE_NAME (TREE_TYPE (exp))), \
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
      else if ((TREE_READONLY (exp) || TREE_CONSTANT (exp))	\
	       && !TREE_SIDE_EFFECTS (exp))				\
	data_section (); /* BUG in ObjC++ Compiler */		\
      else								\
        data_section ();						\
    }									\
  while (0)

/* this was added to support unresolved references in the gas assembler */
#undef DECLARE_UNRESOLVED_REFERENCE

#undef ASM_OUTPUT_INTERNAL_LABEL
#define ASM_OUTPUT_INTERNAL_LABEL(FILE,PREFIX,NUM)      \
{fprintf (FILE, "%c$%s%04d", (PREFIX)[0], (PREFIX) + 1, NUM); \
   if (TARGET_GAS)                                      \
     fputs (":\n", FILE);                               \
   else                                                 \
     fputs ("\n", FILE);}
#undef readonly_data_section
