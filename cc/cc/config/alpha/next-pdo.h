
/* Definitions of target machine for GNU compiler, for DEC Alpha.
   Copyright (C) 1992, 1993 Free Software Foundation, Inc.
   Contributed by Richard Kenner (kenner@nyu.edu)

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

#include "alpha/alpha-gdb.h"

/* Let's not have gas strip the symbols which start with 'L' or mips-tfile
** will complain!
*/

#undef ASM_SPEC
#define ASM_SPEC "-L -nocpp"

/* This need to be verified on GOLD (the linker bug should be solved so we have
   to be able to get rid of it ! */
#undef LINK_SPEC
#define LINK_SPEC  \
  "-G 8 -O1 %{static|r:-non_shared} %{!static:%{!r:-call_shared}}"

#define USE_GAS

#define NEXT_OBJC_RUNTIME

#define NEXT_PDO

/*
 * overrides for the funky places they put stuff
 */
#undef  GPLUSPLUS_INCLUDE_DIR
#define GPLUSPLUS_INCLUDE_DIR "/usr/NextDeveloper/Headers/g++"
#undef  INCLUDE_DEFAULTS
#define INCLUDE_DEFAULTS                                \
  {                                                     \
    { GPLUSPLUS_INCLUDE_DIR, 1},                        \
    { GCC_INCLUDE_DIR, 0},                              \
    { TOOL_INCLUDE_DIR, 0},				\
    { LOCAL_INCLUDE_DIR, 0},                            \
    { "/usr/NextDeveloper/Headers", 0},                 \
    { "/usr/NextDeveloper/Headers/ansi", 0},            \
    { "/usr/LocalDeveloper/Headers", 0},                \
    { "/usr/LocalDeveloper/Headers/ansi", 0},           \
    { STANDARD_INCLUDE_DIR, 0},                         \
    { 0, 0}                                             \
  }

/* RIPPED OFF FROM nextstep.h */

/* Give methods pretty symbol names on NeXT. */

#undef  OBJC_GEN_METHOD_LABEL
#define OBJC_GEN_METHOD_LABEL(BUF,IS_INST,CLASS_NAME,CAT_NAME,SEL_NAME,NUM) \
  do { if (CAT_NAME)                                                    \
         sprintf (BUF, "%c[%s(%s) %s]", (IS_INST) ? '-' : '+',          \
                  (CLASS_NAME), (CAT_NAME), (SEL_NAME));                \
       else                                                             \
         sprintf (BUF, "%c[%s %s]", (IS_INST) ? '-' : '+',              \
                  (CLASS_NAME), (SEL_NAME));                            \
     } while (0)

/* Wrap new method names in quotes so the assembler doesn't gag.
   Make Objective-C internal symbols local.  */

#undef  ASM_OUTPUT_LABELREF
#define ASM_OUTPUT_LABELREF(FILE,NAME)  \
  do {  \
        if (NAME[1] == '+' || NAME[1] == '-') \
                fprintf (FILE, "\"%s\"", NAME+1); \
        else if (NAME[0] == '+' || NAME[0] == '-') \
                fprintf (FILE, "\"%s\"", NAME); \
        else fprintf ((FILE), "%s", NAME); } while (0)


#undef CPP_PREDEFINES
#define CPP_PREDEFINES \
  "-Dunix -D__osf__ -D__alpha -D__alpha__ -D__alpha -DNeXT_PDO -D_LONGLONG \
   -DSYSTYPE_BSD -D_SYSTYPE_BSD -D__ARCHITECTURE__=\"alpha\" \
   -D__LITTLE_ENDIAN__ -Asystem(unix) \
   -Asystem(xpg4) -Acpu(alpha) -Amachine(alpha)"

#undef LIB_SPEC
#define LIB_SPEC "%{!p:%{!pg:-L/usr/NextDeveloper/lib %{!nopdolib:-lpdo} -lc -lpthreads -lmach -lc_r}}%{p:-L/usr/NextDeveloper/lib %{!nopdolib:-lpdo} -L/usr/lib/cmplrs/cc/ -lprof1 -lc -lpthreads -lmach -lc_r}%{pg:-L/usr/NextDeveloper/lib %{!nopdolib:-lpdo} -L/usr/lib/cmplrs/cc/ -lprof1 -lc -lpthreads -lmach -lc_r}"

/* RIPPED OFF FROM nextstep.h */
#undef WORD_SWITCH_TAKES_ARG
#define WORD_SWITCH_TAKES_ARG(STR)				\
  (DEFAULT_WORD_SWITCH_TAKES_ARG (STR)				\
   || !strcmp (STR, "target") 					\
   || !strcmp (STR, "assert")					\
   || !strcmp (STR, "arch")					\
   || !strcmp (STR, "filelist")					\
   || !strcmp (STR, "framework"))
 

#undef  WORD_SWITCH
#define WORD_SWITCH(STR)                        \
  (WORD_SWITCH_TAKES_ARG (STR)                  \
   || !strcmp (STR, "bsd")                      \
   || !strcmp (STR, "object")                   \
   || !strcmp (STR, "ObjC")                     \
   || !strcmp (STR, "all_load"))

#undef  SECTION_FUNCTION
#define SECTION_FUNCTION(FUNCTION, SECTION, DIRECTIVE, WAS_TEXT, OBJC)  \
void                                                                    \
FUNCTION ()                                                             \
{                                                                       \
  extern void text_section ();                                          \
  extern void objc_section_init ();                                     \
  extern int flag_no_mach_text_sections;                                \
                                                                        \
  if (WAS_TEXT && flag_no_mach_text_sections)                           \
    text_section ();                                                    \
  else if (in_section != SECTION)                                       \
    {                                                                   \
      if (OBJC)                                                         \
        objc_section_init ();                                           \
      fprintf (asm_out_file, "%s\n", DIRECTIVE);                        \
      in_section = SECTION;                                             \
    }                                                                   \
}                             						\


#undef  EXTRA_SECTIONS
#define EXTRA_SECTIONS                                  \
  in_sdata, in_lit4, in_lit8, in_lita, in_rdata, \
  in_objc_class, in_objc_meta_class, in_objc_category,  \
  in_objc_class_vars, in_objc_instance_vars,            \
  in_objc_cls_meth, in_objc_inst_meth,                  \
  in_objc_cat_cls_meth, in_objc_cat_inst_meth,          \
  in_objc_selector_refs,                                \
  in_objc_symbols, in_objc_module_info,                 \
  in_objc_protocol, in_objc_string_object,              \
  in_objc_class_names, in_objc_meth_var_names,          \
  in_objc_meth_var_types, in_objc_cls_refs

#undef  EXTRA_SECTION_FUNCTIONS
#define EXTRA_SECTION_FUNCTIONS                 \
SECTION_FUNCTION (sdata_section,              \
                  in_sdata,                   \
                  ".sdata", 0, 0)             \
SECTION_FUNCTION (lit4_section,             \
                  in_lit4,                  \
                  ".sdata", 0, 0)            \
SECTION_FUNCTION (lit8_section,             \
                  in_lit8,                  \
                  ".sdata", 0, 0)            \
SECTION_FUNCTION (lita_section,             \
                  in_lita,                  \
                  ".sdata", 0, 0)            \
SECTION_FUNCTION (rdata_section,           \
                  in_rdata,                \
                  ".rdata", 0, 0)          \
SECTION_FUNCTION (objc_class_section,           \
                  in_objc_class,                \
                  ".data \t#.objc_class", 0, 1)          \
SECTION_FUNCTION (objc_meta_class_section,      \
                  in_objc_meta_class,           \
                  ".data \t#.objc_meta_class", 0, 1)     \
SECTION_FUNCTION (objc_category_section,        \
                  in_objc_category,             \
                ".data \t#.objc_category", 0, 1)         \
SECTION_FUNCTION (objc_class_vars_section,      \
                  in_objc_class_vars,           \
                  ".data \t#.objc_class_vars", 0, 1)     \
SECTION_FUNCTION (objc_instance_vars_section,   \
                  in_objc_instance_vars,        \
                  ".data \t#.objc_instance_vars", 0, 1)  \
SECTION_FUNCTION (objc_cls_meth_section,        \
                  in_objc_cls_meth,             \
                  ".data \t#.objc_cls_meth", 0, 1)       \
SECTION_FUNCTION (objc_inst_meth_section,       \
                  in_objc_inst_meth,            \
                  ".data \t#.objc_inst_meth", 0, 1)      \
SECTION_FUNCTION (objc_cat_cls_meth_section,    \
                  in_objc_cat_cls_meth,         \
                  ".data \t#.objc_cat_cls_meth", 0, 1)   \
SECTION_FUNCTION (objc_cat_inst_meth_section,   \
                  in_objc_cat_inst_meth,        \
                  ".data \t#.objc_cat_inst_meth", 0, 1)  \
SECTION_FUNCTION (objc_selector_refs_section,   \
                  in_objc_selector_refs,        \
                  ".data \t#.objc_message_refs", 0, 1)   \
SECTION_FUNCTION (objc_symbols_section,         \
                  in_objc_symbols,              \
                  ".data \t#.objc_symbols", 0, 1)        \
SECTION_FUNCTION (objc_module_info_section,     \
                  in_objc_module_info,          \
                  ".text \t#.objc_module_info", 0, 1)    \
SECTION_FUNCTION (objc_protocol_section,        \
                  in_objc_protocol,             \
                  ".data \t#.objc_protocol", 0, 1)       \
SECTION_FUNCTION (objc_string_object_section,   \
                  in_objc_string_object,        \
                  ".data \t#.objc_string_object", 0, 1)  \
SECTION_FUNCTION (objc_class_names_section,     \
                in_objc_class_names,            \
                ".data \t#.objc_class_names", 0, 1)      \
SECTION_FUNCTION (objc_meth_var_names_section,  \
                in_objc_meth_var_names,         \
                ".data \t#.objc_meth_var_names", 0, 1)   \
SECTION_FUNCTION (objc_meth_var_types_section,  \
                in_objc_meth_var_types,         \
                ".data \t#.objc_meth_var_types", 0, 1)   \
SECTION_FUNCTION (objc_cls_refs_section,        \
                in_objc_cls_refs,               \
                ".data \t#.objc_cls_refs", 0, 1)         \
                                                \
void                                            \
objc_section_init ()                            \
{                                               \
  static int been_here = 0;                     \
                                                \
  if (been_here == 0)                           \
    {                                           \
      been_here = 1;                            \
      objc_class_section ();                    \
      objc_meta_class_section ();               \
      objc_cat_cls_meth_section ();             \
      objc_cls_meth_section ();                 \
      objc_inst_meth_section ();                \
      objc_selector_refs_section ();            \
      objc_symbols_section ();                  \
      objc_category_section ();                 \
      objc_protocol_section ();                 \
      objc_class_vars_section ();               \
      objc_instance_vars_section ();            \
      objc_module_info_section ();              \
      objc_string_object_section ();            \
      objc_class_names_section ();              \
      objc_meth_var_names_section ();           \
      objc_meth_var_types_section ();           \
      objc_cls_refs_section ();                 \
    }                                           \
}

#undef  READONLY_DATA_SECTION
#define READONLY_DATA_SECTION rdata_section

#undef  SELECT_SECTION
#define SELECT_SECTION(exp,reloc)                               \
  do                                                            \
    {                                                           \
      if (TREE_CODE (exp) == STRING_CST)                        \
        {                                                       \
          if (flag_writable_strings)                            \
            data_section ();                                    \
          else         \
            readonly_data_section ();                           \
        }                                                       \
      else if (TREE_CODE (exp) == INTEGER_CST                   \
               || TREE_CODE (exp) == REAL_CST)                  \
        {                                                       \
          tree size = TYPE_SIZE (TREE_TYPE (exp));              \
                                                                \
          if (TREE_CODE (size) == INTEGER_CST &&                \
              TREE_INT_CST_LOW (size) == 4 &&                   \
              TREE_INT_CST_HIGH (size) == 0)                    \
            lit4_section ();                                \
          else if (TREE_CODE (size) == INTEGER_CST &&           \
              TREE_INT_CST_LOW (size) == 8 &&                   \
              TREE_INT_CST_HIGH (size) == 0)                    \
            lit8_section ();                                \
          else                                                  \
            readonly_data_section ();                           \
        }                                                       \
      else if (TREE_CODE (exp) == CONSTRUCTOR                           \
               && TREE_TYPE (exp)                                       \
               && TREE_CODE (TREE_TYPE (exp)) == RECORD_TYPE            \
               && TYPE_NAME (TREE_TYPE (exp)))                          \
        {                                                               \
          tree name = TYPE_NAME (TREE_TYPE (exp));                      \
          if (TREE_CODE (name) == TYPE_DECL)                            \
            name = DECL_NAME (name);                                    \
          if (!strcmp (IDENTIFIER_POINTER (name), "NXConstantString"))  \
            objc_string_object_section ();                              \
          else if ((TREE_READONLY (exp) || TREE_CONSTANT (exp))         \
                && !TREE_SIDE_EFFECTS (exp))                            \
            readonly_data_section ();                                   \
          else                                                          \
            data_section ();                                            \
      }                                                                 \
      else if (TREE_CODE (exp) == VAR_DECL &&                           \
               DECL_NAME (exp) &&                                       \
               TREE_CODE (DECL_NAME (exp)) == IDENTIFIER_NODE &&        \
               IDENTIFIER_POINTER (DECL_NAME (exp)) &&                  \
               !strncmp (IDENTIFIER_POINTER (DECL_NAME (exp)), "_OBJC_", 6)) \
        {                                                               \
          const char *name = IDENTIFIER_POINTER (DECL_NAME (exp));      \
                                                                        \
          if (!strncmp (name, "_OBJC_CLASS_METHODS_", 20))              \
            objc_cls_meth_section ();                                   \
          else if (!strncmp (name, "_OBJC_INSTANCE_METHODS_", 23))      \
            objc_inst_meth_section ();                                  \
          else if (!strncmp (name, "_OBJC_CATEGORY_CLASS_METHODS_", 20)) \
            objc_cat_cls_meth_section ();                               \
          else if (!strncmp (name, "_OBJC_CATEGORY_INSTANCE_METHODS_", 23)) \
            objc_cat_inst_meth_section ();                              \
          else if (!strncmp (name, "_OBJC_CLASS_VARIABLES_", 22))       \
            objc_class_vars_section ();                                 \
          else if (!strncmp (name, "_OBJC_INSTANCE_VARIABLES_", 25))    \
            objc_instance_vars_section ();                              \
          else if (!strncmp (name, "_OBJC_CLASS_PROTOCOLS_", 22))       \
            objc_cat_cls_meth_section ();                               \
          else if (!strncmp (name, "_OBJC_CLASS_NAME_", 17))            \
            objc_class_names_section ();                                \
          else if (!strncmp (name, "_OBJC_METH_VAR_NAME_", 20))         \
            objc_meth_var_names_section ();                             \
          else if (!strncmp (name, "_OBJC_METH_VAR_TYPE_", 20))         \
            objc_meth_var_types_section ();                             \
          else if (!strncmp (name, "_OBJC_CLASS_REFERENCES", 22))       \
            objc_cls_refs_section ();                                   \
          else if (!strncmp (name, "_OBJC_CLASS_", 12))                 \
            objc_class_section ();                                      \
          else if (!strncmp (name, "_OBJC_METACLASS_", 16))             \
            objc_meta_class_section ();                                 \
          else if (!strncmp (name, "_OBJC_CATEGORY_", 15))              \
            objc_category_section ();                                   \
          else if (!strncmp (name, "_OBJC_SELECTOR_REFERENCES", 25))    \
            objc_selector_refs_section ();                              \
          else if (!strncmp (name, "_OBJC_SYMBOLS", 13))                \
            objc_symbols_section ();                                    \
          else if (!strncmp (name, "_OBJC_MODULES", 13))                \
            objc_module_info_section ();                                \
          else if (!strncmp (name, "_OBJC_PROTOCOL_INSTANCE_METHODS_", 32)) \
            objc_cat_inst_meth_section ();                              \
          else if (!strncmp (name, "_OBJC_PROTOCOL_CLASS_METHODS_", 29)) \
            objc_cat_cls_meth_section ();                               \
          else if (!strncmp (name, "_OBJC_PROTOCOL_REFS_", 20))         \
            objc_cat_cls_meth_section ();                               \
          else if (!strncmp (name, "_OBJC_PROTOCOL_", 15))              \
            objc_protocol_section ();                                   \
          else if ((TREE_READONLY (exp) || TREE_CONSTANT (exp))         \
                && !TREE_SIDE_EFFECTS (exp))                            \
            readonly_data_section ();                                   \
          else                                                          \
            data_section ();                                            \
        }                                                               \
      else if ((TREE_READONLY (exp) || TREE_CONSTANT (exp))     \
               && !TREE_SIDE_EFFECTS (exp))                             \
        readonly_data_section ();                                       \
      else                                                              \
        data_section ();                                                \
    }                                                                   \
  while (0)

#undef  SELECT_RTX_SECTION
#define SELECT_RTX_SECTION(mode, rtx)                                   \
  do                                                                    \
    {                                                                   \
      if (GET_MODE_SIZE(mode) == 8)                                     \
        lit8_section();                                             \
      else if (GET_MODE_SIZE(mode) == 4)                                \
        lit4_section();                                             \
      else                                                              \
        rdata_section ();                                               \
    }                                                                   \
  while (0)


