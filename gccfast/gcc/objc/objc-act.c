/* Implement classes and message passing for Objective C.
   Copyright (C) 1992, 1993, 1994, 1995, 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.
   Contributed by Steve Naroff.

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

/* Purpose: This module implements the Objective-C 4.0 language.

   compatibility issues (with the Stepstone translator):

   - does not recognize the following 3.3 constructs.
     @requires, @classes, @messages, = (...)
   - methods with variable arguments must conform to ANSI standard.
   - tagged structure definitions that appear in BOTH the interface
     and implementation are not allowed.
   - public/private: all instance variables are public within the
     context of the implementation...I consider this to be a bug in
     the translator.
   - statically allocated objects are not supported. the user will
     receive an error if this service is requested.

   code generation `options':

   */

#include "config.h"
#include "system.h"
#include "tree.h"
#include "rtl.h"
#include "expr.h"
/* APPLE LOCAL begin Objective-C++ */
#ifdef OBJCPLUS
#include "cp-tree.h"
#include "lex.h"
#else
#include "c-tree.h"
#endif
/* APPLE LOCAL end Objective-C++ */
#include "ggc.h"
#include "c-common.h"
#include "flags.h"
#include "objc-act.h"
#include "input.h"
#include "except.h"
#include "function.h"
#include "output.h"
#include "toplev.h"
#include "varray.h"
#include "debug.h"
#include "target.h"
#include "diagnostic.h"
/* APPLE LOCAL callgraph inlining */
#include "cgraph.h"

/* APPLE LOCAL Panther ObjC enhancements */
#define OBJC_VOID_AT_END	build_tree_list (NULL_TREE, void_type_node)

/* APPLE LOCAL indexing dpatel */
#include "genindex.h"

/* APPLE LOCAL begin Objective-C++ */
/* When building Objective-C++, we are not linking against the C front-end
   and so need to replicate the C tree-construction functions in some way.  */
#ifdef OBJCPLUS
#define OBJCP_REMAP_FUNCTIONS
#include "objcp-decl.h"
#endif  /* OBJCPLUS */
/* APPLE LOCAL end Objective-C++ */

/* APPLE LOCAL new tree dump */
#ifdef ENABLE_DMP_TREE
#include "dmp-tree.h"
extern int c_dump_tree_p	PARAMS ((FILE *, const char *, tree, int));
extern int objc_dump_tree_p	PARAMS ((FILE *, const char *, tree, int));
extern lang_dump_tree_p_t	objc_prev_lang_dump_tree_p;
#endif

/* This is the default way of generating a method name.  */
/* I am not sure it is really correct.
   Perhaps there's a danger that it will make name conflicts
   if method names contain underscores. -- rms.  */
#ifndef OBJC_GEN_METHOD_LABEL
#define OBJC_GEN_METHOD_LABEL(BUF, IS_INST, CLASS_NAME, CAT_NAME, SEL_NAME, NUM) \
  do {					    \
    char *temp;				    \
    sprintf ((BUF), "_%s_%s_%s_%s",	    \
	     ((IS_INST) ? "i" : "c"),	    \
	     (CLASS_NAME),		    \
	     ((CAT_NAME)? (CAT_NAME) : ""), \
	     (SEL_NAME));		    \
    for (temp = (BUF); *temp; temp++)	    \
      if (*temp == ':') *temp = '_';	    \
  } while (0)
#endif

/* These need specifying.  */
#ifndef OBJC_FORWARDING_STACK_OFFSET
#define OBJC_FORWARDING_STACK_OFFSET 0
#endif

#ifndef OBJC_FORWARDING_MIN_OFFSET
#define OBJC_FORWARDING_MIN_OFFSET 0
#endif


/* Set up for use of obstacks.  */

#include "obstack.h"

/* This obstack is used to accumulate the encoding of a data type.  */
static struct obstack util_obstack;
/* This points to the beginning of obstack contents,
   so we can free the whole contents.  */
char *util_firstobj;

/* APPLE LOCAL method encoding */
/* The encode_method_def routine is no more (encode_method_prototype
   is used instead).  */

/* The version identifies which language generation and runtime
   the module (file) was compiled for, and is recorded in the
   module descriptor.  */

#define OBJC_VERSION	(flag_next_runtime ? 5 : 8)
#define PROTOCOL_VERSION 2

/* (Decide if these can ever be validly changed.) */
#define OBJC_ENCODE_INLINE_DEFS 	0
#define OBJC_ENCODE_DONT_INLINE_DEFS	1

/*** Private Interface (procedures) ***/

/* Used by compile_file.  */

static void init_objc				PARAMS ((void));
static void finish_objc				PARAMS ((void));

/* Code generation.  */

static void synth_module_prologue		PARAMS ((void));
static tree build_constructor			PARAMS ((tree, tree));
static rtx build_module_descriptor		PARAMS ((void));
static tree init_module_descriptor		PARAMS ((tree));
/* APPLE LOCAL begin msg send super */
static tree build_objc_method_call		PARAMS ((int, tree, tree,
						       tree, tree));
/* APPLE LOCAL end msg send super */
static void generate_strings			PARAMS ((void));
static tree get_proto_encoding 			PARAMS ((tree));
static void build_selector_translation_table	PARAMS ((void));

static tree objc_add_static_instance		PARAMS ((tree, tree));

/* APPLE LOCAL begin Panther ObjC enhancements */
static void build_objc_exception_stuff		PARAMS ((void));
static tree objc_declare_variable		PARAMS ((enum rid, tree, 
						       tree, tree));
static tree objc_enter_block			PARAMS ((void));
static tree objc_exit_block			PARAMS ((void));
static void objc_build_try_enter_fragment	PARAMS ((void));
static void objc_build_try_exit_fragment	PARAMS ((void));
static void objc_build_extract_fragment		PARAMS ((void));
static tree objc_build_extract_expr		PARAMS ((void));
/* APPLE LOCAL end Panther ObjC enhancements */

static tree build_ivar_template			PARAMS ((void));
static tree build_method_template		PARAMS ((void));
static tree build_private_template		PARAMS ((tree));
static void build_class_template		PARAMS ((void));
static void build_selector_template		PARAMS ((void));
static void build_category_template		PARAMS ((void));
/* APPLE LOCAL begin msg send super */
static tree lookup_method_in_hash_lists		PARAMS ((tree));
static void build_super_template		PARAMS ((void));
/* APPLE LOCAL end msg send super */
static tree build_category_initializer		PARAMS ((tree, tree, tree,
						       tree, tree, tree));
static tree build_protocol_initializer		PARAMS ((tree, tree, tree,
						       tree, tree));
/* APPLE LOCAL SPEC */
static tree type_protocol_list			PARAMS ((tree));
static void synth_forward_declarations		PARAMS ((void));
/* APPLE LOCAL Objective-C++ */
static int ivar_list_length			PARAMS ((tree));
/* APPLE LOCAL bitfields */
static tree get_class_ivars			PARAMS ((tree, int));
static void generate_ivar_lists			PARAMS ((void));
static void generate_dispatch_tables		PARAMS ((void));
static void generate_shared_structures		PARAMS ((void));
static tree generate_protocol_list		PARAMS ((tree));
static void generate_forward_declaration_to_string_table PARAMS ((void));
static void build_protocol_reference		PARAMS ((tree));

static tree build_keyword_selector		PARAMS ((tree));
static tree synth_id_with_class_suffix		PARAMS ((const char *, tree));

static void generate_static_references		PARAMS ((void));
static int check_methods_accessible		PARAMS ((tree, tree,
						       int));
static void encode_aggregate_within		PARAMS ((tree, int, int,
					               int, int));
static const char *objc_demangle		PARAMS ((const char *));
static void objc_expand_function_end            PARAMS ((void));

/* Hash tables to manage the global pool of method prototypes.  */

hash *nst_method_hash_list = 0;
hash *cls_method_hash_list = 0;

static size_t hash_func				PARAMS ((tree));
static void hash_init				PARAMS ((void));
static void hash_enter				PARAMS ((hash *, tree));
static hash hash_lookup				PARAMS ((hash *, tree));
static void hash_add_attr			PARAMS ((hash, tree));
static tree lookup_method			PARAMS ((tree, tree));
static tree lookup_instance_method_static	PARAMS ((tree, tree));
static tree lookup_class_method_static		PARAMS ((tree, tree));
/* APPLE LOCAL objc speedup dpatel */
static tree add_class				PARAMS ((tree, tree));
static void add_category			PARAMS ((tree, tree));

enum string_section
{
  class_names,		/* class, category, protocol, module names */
  meth_var_names,	/* method and variable names */
  meth_var_types	/* method and variable type descriptors */
};

static tree add_objc_string			PARAMS ((tree,
						       enum string_section));
static tree get_objc_string_decl		PARAMS ((tree,
						       enum string_section));
static tree build_objc_string_decl		PARAMS ((enum string_section));
static tree build_selector_reference_decl	PARAMS ((void));

/* Protocol additions.  */

static tree add_protocol			PARAMS ((tree));
static tree lookup_protocol			PARAMS ((tree));
static void check_protocol_recursively		PARAMS ((tree, tree));
static tree lookup_and_install_protocols	PARAMS ((tree));

/* Type encoding.  */

static void encode_type_qualifiers		PARAMS ((tree));
static void encode_pointer			PARAMS ((tree, int, int));
static void encode_array			PARAMS ((tree, int, int));
static void encode_aggregate			PARAMS ((tree, int, int));
static void encode_bitfield			PARAMS ((int));
static void encode_type				PARAMS ((tree, int, int));
static void encode_field_decl			PARAMS ((tree, int, int));

static void really_start_method			PARAMS ((tree, tree));
static int comp_method_with_proto		PARAMS ((tree, tree));
static int comp_proto_with_proto		PARAMS ((tree, tree));
static tree get_arg_type_list			PARAMS ((tree, int, int));
static tree expr_last				PARAMS ((tree));
/* APPLE LOCAL Objective-C++ */
static void synth_self_and_ucmd_args		PARAMS ((void));

/* Utilities for debugging and error diagnostics.  */

static void warn_with_method			PARAMS ((const char *, int, tree));
static void error_with_ivar			PARAMS ((const char *, tree, tree));
static char *gen_method_decl			PARAMS ((tree, char *));
static char *gen_declaration			PARAMS ((tree, char *));
static void gen_declaration_1			PARAMS ((tree, char *));
static char *gen_declarator			PARAMS ((tree, char *,
						       const char *));
static int is_complex_decl			PARAMS ((tree));
static void adorn_decl				PARAMS ((tree, char *));
static void dump_interface			PARAMS ((FILE *, tree));

/* Everything else.  */

static tree define_decl				PARAMS ((tree, tree));
static tree lookup_method_in_protocol_list	PARAMS ((tree, tree, int));
static tree lookup_protocol_in_reflist		PARAMS ((tree, tree));
static tree create_builtin_decl			PARAMS ((enum tree_code,
						       tree, const char *));
static void setup_string_decl			PARAMS ((void));
/* APPLE LOCAL Objective-C++ */
static int check_string_class_template		PARAMS ((void));
static tree my_build_string			PARAMS ((int, const char *));
static void build_objc_symtab_template		PARAMS ((void));
static tree init_def_list			PARAMS ((tree));
static tree init_objc_symtab			PARAMS ((tree));
/* APPLE LOCAL Objective-C++ */
static tree build_metadata_decl			PARAMS ((const char *, tree));
static void forward_declare_categories		PARAMS ((void));
static void generate_objc_symtab_decl		PARAMS ((void));
static tree build_selector			PARAMS ((tree));
static tree build_typed_selector_reference     	PARAMS ((tree, tree));
static tree build_selector_reference		PARAMS ((tree));
static tree build_class_reference_decl		PARAMS ((void));
static void add_class_reference			PARAMS ((tree));
static tree build_protocol_template		PARAMS ((void));
static tree build_descriptor_table_initializer	PARAMS ((tree, tree));
static tree build_method_prototype_list_template PARAMS ((tree, int));
static tree build_method_prototype_template	PARAMS ((void));
/* APPLE LOCAL begin Panther ObjC enhancements */
/* 'objc_forwarding_offset' has been removed */
static tree objc_method_parm_type		PARAMS ((tree));
static int objc_encoded_type_size		PARAMS ((tree));
static tree encode_method_prototype		PARAMS ((tree));
/* APPLE LOCAL end Panther ObjC enhancements */
static tree generate_descriptor_table		PARAMS ((tree, const char *,
						       int, tree, tree));
static void generate_method_descriptors		PARAMS ((tree));
/* APPLE LOCAL Panther ObjC enhancements */
/* 'build_tmp_function_decl' and 'hack_method_prototype' have been removed */
static void generate_protocol_references	PARAMS ((tree));
static void generate_protocols			PARAMS ((void));
static void check_ivars				PARAMS ((tree, tree));
static tree build_ivar_list_template		PARAMS ((tree, int));
static tree build_method_list_template		PARAMS ((tree, int));
static tree build_ivar_list_initializer		PARAMS ((tree, tree));
static tree generate_ivars_list			PARAMS ((tree, const char *,
						       int, tree));
static tree build_dispatch_table_initializer	PARAMS ((tree, tree));
static tree generate_dispatch_table		PARAMS ((tree, const char *,
						       int, tree));
static tree build_shared_structure_initializer	PARAMS ((tree, tree, tree, tree,
						       tree, int, tree, tree,
						       tree));
static void generate_category			PARAMS ((tree));
static int is_objc_type_qualifier		PARAMS ((tree));
static tree adjust_type_for_id_default		PARAMS ((tree));
static tree check_duplicates			PARAMS ((hash));
/* APPLE LOCAL Panther ObjC enhancements */
static tree receiver_is_class_object		PARAMS ((tree, int, int));
static int check_methods			PARAMS ((tree, tree, int));
static int conforms_to_protocol			PARAMS ((tree, tree));
static void check_protocol			PARAMS ((tree, const char *,
						       const char *));
static void check_protocols			PARAMS ((tree, const char *,
						       const char *));
/* APPLE LOCAL method encoding */
/* The encode_method_def routine is no more (encode_method_prototype
   is used instead).  */
static void gen_declspecs			PARAMS ((tree, char *, int));
static void generate_classref_translation_entry	PARAMS ((tree));
static void handle_class_ref			PARAMS ((tree));
static void generate_struct_by_value_array	PARAMS ((void))
     ATTRIBUTE_NORETURN;
static void encode_complete_bitfield		PARAMS ((int, tree, int));
/* APPLE LOCAL callgraph inlining */
static void mark_referenced_methods		PARAMS ((void));

/*** Private Interface (data) ***/

/* Reserved tag definitions.  */

#define TYPE_ID			"id"
#define TAG_OBJECT		"objc_object"
#define TAG_CLASS		"objc_class"
#define TAG_SUPER		"objc_super"
#define TAG_SELECTOR		"objc_selector"

#define UTAG_CLASS		"_objc_class"
#define UTAG_IVAR		"_objc_ivar"
#define UTAG_IVAR_LIST		"_objc_ivar_list"
#define UTAG_METHOD		"_objc_method"
#define UTAG_METHOD_LIST	"_objc_method_list"
#define UTAG_CATEGORY		"_objc_category"
#define UTAG_MODULE		"_objc_module"
#define UTAG_SYMTAB		"_objc_symtab"
#define UTAG_SUPER		"_objc_super"
#define UTAG_SELECTOR		"_objc_selector"

#define UTAG_PROTOCOL		"_objc_protocol"
#define UTAG_METHOD_PROTOTYPE	"_objc_method_prototype"
#define UTAG_METHOD_PROTOTYPE_LIST "_objc__method_prototype_list"

/* Note that the string object global name is only needed for the
   NeXT runtime.  */
/* APPLE LOCAL constant strings */   
#define STRING_OBJECT_GLOBAL_FORMAT "_%sClassReference"

#define PROTOCOL_OBJECT_CLASS_NAME "Protocol"

static const char *TAG_GETCLASS;
static const char *TAG_GETMETACLASS;
static const char *TAG_MSGSEND;
static const char *TAG_MSGSENDSUPER;
/* APPLE LOCAL begin objc stret methods */
/* The Objective-C messenger may have two extra entry points, for use
   when returning a structure. */
static const char *TAG_MSGSEND_STRET;
static const char *TAG_MSGSENDSUPER_STRET;
/* APPLE LOCAL end objc stret methods */
static const char *TAG_EXECCLASS;
static const char *default_constant_string_class_name;

/* APPLE LOCAL begin Panther ObjC enhancements */
#define CLS_FACTORY			0x0001L
#define CLS_META			0x0002L

#define OBJC_MODIFIER_STATIC		0x00000001
#define OBJC_MODIFIER_FINAL		0x00000002
#define OBJC_MODIFIER_PUBLIC		0x00000004
#define OBJC_MODIFIER_PRIVATE		0x00000008
#define OBJC_MODIFIER_PROTECTED		0x00000010
#define OBJC_MODIFIER_NATIVE		0x00000020
#define OBJC_MODIFIER_SYNCHRONIZED	0x00000040
#define OBJC_MODIFIER_ABSTRACT		0x00000080
#define OBJC_MODIFIER_VOLATILE		0x00000100
#define OBJC_MODIFIER_TRANSIENT		0x00000200
#define OBJC_MODIFIER_NONE_SPECIFIED	0x80000000

#define TAG_MSGSEND_NONNIL		"objc_msgSendNonNil"
#define TAG_MSGSEND_NONNIL_STRET	"objc_msgSendNonNil_stret"
#define TAG_EXCEPTIONEXTRACT		"objc_exception_extract"
#define TAG_EXCEPTIONTRYENTER		"objc_exception_try_enter"
#define TAG_EXCEPTIONTRYEXIT		"objc_exception_try_exit"
#define TAG_EXCEPTIONMATCH		"objc_exception_match"
#define TAG_EXCEPTIONTHROW		"objc_exception_throw"
#define TAG_SYNCENTER			"objc_sync_enter"
#define TAG_SYNCEXIT			"objc_sync_exit"
#define TAG_SETJMP			"_setjmp"
#define TAG_RETURN_STRUCT		"objc_return_struct"

#define UTAG_EXCDATA		"_objc_exception_data"
#define UTAG_EXCDATA_VAR	"_stackExceptionData"
#define UTAG_CAUGHTEXC_VAR	"_caughtException"
#define UTAG_RETHROWEXC_VAR	"_rethrowException"
#define UTAG_EVALONCE_VAR	"_eval_once"

struct val_stack {
  long val;
  struct val_stack *next;
};
static struct val_stack *catch_count_stack, *exc_binding_stack;
static int if_nesting_count, blk_nesting_count; /* useful for debugging */

static void val_stack_push		PARAMS ((struct val_stack **, long));
static void val_stack_pop		PARAMS ((struct val_stack **));

/* APPLE LOCAL end Panther ObjC enhancements */

/* The OCTI_... enumeration itself is in objc/objc-act.h.  */
tree objc_global_trees[OCTI_MAX];

/* APPLE LOCAL fix and continue */
static void generate_objc_image_info		PARAMS ((void));

static void handle_impent			PARAMS ((struct imp_entry *));

struct imp_entry *imp_list = 0;
int imp_count = 0;	/* `@implementation' */
int cat_count = 0;	/* `@category' */

static int  method_slot = 0;	/* Used by start_method_def, */

#define BUFSIZE		1024

static char *errbuf;	/* Buffer for error diagnostics */

/* Data imported from tree.c.  */

extern enum debug_info_type write_symbols;

/* Data imported from toplev.c.  */

extern const char *dump_base_name;

static int flag_typed_selectors;

FILE *gen_declaration_file;

/* Tells "encode_pointer/encode_aggregate" whether we are generating
   type descriptors for instance variables (as opposed to methods).
   Type descriptors for instance variables contain more information
   than methods (for static typing and embedded structures).  */

static int generating_instance_variables = 0;

/* Some platforms pass small structures through registers versus
   through an invisible pointer.  Determine at what size structure is
   the transition point between the two possibilities.  */

static void
generate_struct_by_value_array ()
{
  tree type;
  tree field_decl, field_decl_chain;
  int i, j;
  int aggregate_in_mem[32];
  int found = 0;

  /* Presumably no platform passes 32 byte structures in a register.  */
  for (i = 1; i < 32; i++)
    {
      char buffer[5];

      /* Create an unnamed struct that has `i' character components */
      type = start_struct (RECORD_TYPE, NULL_TREE);

      strcpy (buffer, "c1");
      field_decl = create_builtin_decl (FIELD_DECL,
					char_type_node,
					buffer);
      field_decl_chain = field_decl;

      for (j = 1; j < i; j++)
	{
	  sprintf (buffer, "c%d", j + 1);
	  field_decl = create_builtin_decl (FIELD_DECL,
					    char_type_node,
					    buffer);
	  chainon (field_decl_chain, field_decl);
	}
      finish_struct (type, field_decl_chain, NULL_TREE);
 
      aggregate_in_mem[i] = aggregate_value_p (type);
      if (!aggregate_in_mem[i])
	found = 1;
    }
 
  /* We found some structures that are returned in registers instead of memory
     so output the necessary data.  */
  if (found)
    {
      for (i = 31; i >= 0;  i--)
	if (!aggregate_in_mem[i])
	  break;
      printf ("#define OBJC_MAX_STRUCT_BY_VALUE %d\n\n", i);
 
      /* The first member of the structure is always 0 because we don't handle
	 structures with 0 members */
      printf ("static int struct_forward_array[] = {\n  0");
 
      for (j = 1; j <= i; j++)
	printf (", %d", aggregate_in_mem[j]);
      printf ("\n};\n");
    }
 
  exit (0);
}

const char *
objc_init (filename)
     const char *filename;
{
  /* APPLE LOCAL Objective-C++ */
#ifdef OBJCPLUS
  filename = cxx_init (filename);
#else
  filename = c_objc_common_init (filename);
#endif
     
  if (filename == NULL)
    return filename;

  /* Force the line number back to 0; check_newline will have
     raised it to 1, which will make the builtin functions appear
     not to be built in.  */
  lineno = 0;

/* APPLE LOCAL new tree dump */
#ifdef ENABLE_DMP_TREE
  if (!objc_prev_lang_dump_tree_p)
    objc_prev_lang_dump_tree_p = set_dump_tree_p (objc_dump_tree_p); 
  /* At this point, objc_prev_lang_dump_tree_p should point at the C tree
     dump routine (which, in the case of Objective-C++, points at the C++
     tree dump routine in turn).  */
  if (objc_prev_lang_dump_tree_p != &c_dump_tree_p)
    abort ();
       
  SET_MAX_DMP_TREE_CODE (LAST_OBJC_TREE_CODE);
#endif

  /* If gen_declaration desired, open the output file.  */
  if (flag_gen_declaration)
    {
      register char * const dumpname = concat (dump_base_name, ".decl", NULL);
      gen_declaration_file = fopen (dumpname, "w");
      if (gen_declaration_file == 0)
	fatal_io_error ("can't open %s", dumpname);
      free (dumpname);
    }

  if (flag_next_runtime)
    {
      TAG_GETCLASS = "objc_getClass";
      TAG_GETMETACLASS = "objc_getMetaClass";
      TAG_MSGSEND = "objc_msgSend";
      TAG_MSGSENDSUPER = "objc_msgSendSuper";
      /* APPLE LOCAL begin objc stret methods */
      TAG_MSGSEND_STRET = "objc_msgSend_stret";
      TAG_MSGSENDSUPER_STRET = "objc_msgSendSuper_stret";
      /* APPLE LOCAL end objc stret methods */
      TAG_EXECCLASS = "__objc_execClass";
      default_constant_string_class_name = "NSConstantString";
    }
  else
    {
      TAG_GETCLASS = "objc_get_class";
      TAG_GETMETACLASS = "objc_get_meta_class";
      TAG_MSGSEND = "objc_msg_lookup";
      TAG_MSGSENDSUPER = "objc_msg_lookup_super";
      /* APPLE LOCAL begin objc stret methods */
      /* GNU runtime does not require special functions to support
	 structure-returning methods.  */
      /* APPLE LOCAL end objc stret methods */
      TAG_EXECCLASS = "__objc_exec_class";
      default_constant_string_class_name = "NXConstantString";
      flag_typed_selectors = 1;
    }

  objc_ellipsis_node = make_node (ERROR_MARK);

  init_objc ();

  if (print_struct_values)
    generate_struct_by_value_array ();

  return filename;
}

/* APPLE LOCAL Objective-C++  */
void
objc_finish_file ()
{
  /* APPLE LOCAL callgraph inlining */
  mark_referenced_methods ();
  /* APPLE LOCAL begin Objective-C++  */
#ifdef OBJCPLUS
  if (! cxx_finish_file_start ())
    return;
  cxx_finish_file_rest ();
#else
  c_objc_common_finish_file ();
#endif
  /* APPLE LOCAL end Objective-C++  */

  /* Finalize Objective-C runtime data.  No need to generate tables
     and code if only checking syntax.  */
  if (!flag_syntax_only)
    finish_objc ();

  if (gen_declaration_file)
    fclose (gen_declaration_file);
}

static tree
define_decl (declarator, declspecs)
     tree declarator;
     tree declspecs;
{
  tree decl;

  /* APPLE LOCAL indexing dpatel */
  flag_suppress_builtin_indexing = 1;

  decl = start_decl (declarator, declspecs, 0, NULL_TREE);
  finish_decl (decl, NULL_TREE, NULL_TREE);

  /* APPLE LOCAL indexing dpatel */
  flag_suppress_builtin_indexing = 0;

  return decl;
}

/* APPLE LOCAL begin SPEC */
/* Protocol qualifications (as in 'id <MyProtocol>' or 'YourObject <YourProtocol> *')
   are stored in the same place where a TYPE_CONTEXT (e.g., TRANSLATION_UNIT_DECL)
   can sometimes be stored.  We need to distinguish between these.  */

static tree
type_protocol_list (type)
     tree type;
{
  tree list = TYPE_PROTOCOL_LIST (type);

  return (list && TREE_CODE (list) == TREE_LIST ? list : NULL_TREE);
}
/* APPLE LOCAL end SPEC */

/* Return 1 if LHS and RHS are compatible types for assignment or
   various other operations.  Return 0 if they are incompatible, and
   return -1 if we choose to not decide.  When the operation is
   REFLEXIVE, check for compatibility in either direction.

   For statically typed objects, an assignment of the form `a' = `b'
   is permitted if:

   `a' is of type "id",
   `a' and `b' are the same class type, or
   `a' and `b' are of class types A and B such that B is a descendant of A.  */

static tree
lookup_method_in_protocol_list (rproto_list, sel_name, class_meth)
   tree rproto_list;
   tree sel_name;
   int class_meth;
{
   tree rproto, p;
   tree fnd = 0;

   for (rproto = rproto_list; rproto; rproto = TREE_CHAIN (rproto))
     {
        p = TREE_VALUE (rproto);

	if (TREE_CODE (p) == PROTOCOL_INTERFACE_TYPE)
	  {
	    if ((fnd = lookup_method (class_meth
				      ? PROTOCOL_CLS_METHODS (p)
				      : PROTOCOL_NST_METHODS (p), sel_name)))
	      ;
	    else if (PROTOCOL_LIST (p))
	      fnd = lookup_method_in_protocol_list (PROTOCOL_LIST (p),
						    sel_name, class_meth);
	  }
	else
          {
	    ; /* An identifier...if we could not find a protocol.  */
          }

	if (fnd)
	  return fnd;
     }

   return 0;
}

static tree
lookup_protocol_in_reflist (rproto_list, lproto)
     tree rproto_list;
     tree lproto;
{
  tree rproto, p;

  /* Make sure the protocol is supported by the object on the rhs.  */
  if (TREE_CODE (lproto) == PROTOCOL_INTERFACE_TYPE)
    {
      tree fnd = 0;
      for (rproto = rproto_list; rproto; rproto = TREE_CHAIN (rproto))
	{
	  p = TREE_VALUE (rproto);

	  if (TREE_CODE (p) == PROTOCOL_INTERFACE_TYPE)
	    {
	      if (lproto == p)
		fnd = lproto;

	      else if (PROTOCOL_LIST (p))
		fnd = lookup_protocol_in_reflist (PROTOCOL_LIST (p), lproto);
	    }

	  if (fnd)
	    return fnd;
	}
    }
  else
    {
      ; /* An identifier...if we could not find a protocol.  */
    }

  return 0;
}

/* Return 1 if LHS and RHS are compatible types for assignment or
   various other operations.  Return 0 if they are incompatible, and
   return -1 if we choose to not decide (because the types are really
   just C types, not ObjC specific ones).  When the operation is
   REFLEXIVE (typically comparisons), check for compatibility in
   either direction; when it's not (typically assignments), don't.

   This function is called in two cases: when both lhs and rhs are
   pointers to records (in which case we check protocols too), and
   when both lhs and rhs are records (in which case we check class
   inheritance only).

   Warnings about classes/protocols not implementing a protocol are
   emitted here (multiple of those warnings might be emitted for a
   single line!); generic warnings about incompatible assignments and
   lacks of casts in comparisons are/must be emitted by the caller if
   we return 0.
*/

int
objc_comptypes (lhs, rhs, reflexive)
     tree lhs;
     tree rhs;
     int reflexive;
{
  /* New clause for protocols.  */

  /* Here we manage the case of a POINTER_TYPE = POINTER_TYPE.  We only
     manage the ObjC ones, and leave the rest to the C code.  */
  if (TREE_CODE (lhs) == POINTER_TYPE
      && TREE_CODE (TREE_TYPE (lhs)) == RECORD_TYPE
      && TREE_CODE (rhs) == POINTER_TYPE
      && TREE_CODE (TREE_TYPE (rhs)) == RECORD_TYPE)
    {
      int lhs_is_proto = IS_PROTOCOL_QUALIFIED_ID (lhs);
      int rhs_is_proto = IS_PROTOCOL_QUALIFIED_ID (rhs);

      if (lhs_is_proto)
        {
	  /* APPLE LOCAL SPEC */
	  tree lproto, lproto_list = type_protocol_list (lhs);
	  tree rproto, rproto_list;
	  tree p;

	  /* <Protocol> = <Protocol>  */
	  if (rhs_is_proto)
	    {
	      /* APPLE LOCAL SPEC */
	      rproto_list = type_protocol_list (rhs);
	      
	      if (!reflexive)
		{
		  /* An assignment between objects of type 'id
		     <Protocol>'; make sure the protocol on the lhs is
		     supported by the object on the rhs.  */
		  for (lproto = lproto_list; lproto; 
		       lproto = TREE_CHAIN (lproto))
		    {
		      p = TREE_VALUE (lproto);
		      rproto = lookup_protocol_in_reflist (rproto_list, p);

		      if (!rproto)
			warning 
			  ("object does not conform to the `%s' protocol",
			   IDENTIFIER_POINTER (PROTOCOL_NAME (p)));
		    }
		  return 1;
		}
	      else
		{
		  /* Obscure case - a comparison between two objects
		     of type 'id <Protocol>'.  Check that either the
		     protocol on the lhs is supported by the object on
		     the rhs, or viceversa.  */
		  
		  /* Check if the protocol on the lhs is supported by the
		     object on the rhs.  */
		  for (lproto = lproto_list; lproto; 
		       lproto = TREE_CHAIN (lproto))
		    {
		      p = TREE_VALUE (lproto);
		      rproto = lookup_protocol_in_reflist (rproto_list, p);
		      
		      if (!rproto)
			{
			  /* Check failed - check if the protocol on the rhs
			     is supported by the object on the lhs.  */
			  for (rproto = rproto_list; rproto; 
			       rproto = TREE_CHAIN (rproto))
			    {
			      p = TREE_VALUE (rproto);
			      lproto = lookup_protocol_in_reflist (lproto_list,
								   p);

			      if (!lproto)
				{
				  /* This check failed too: incompatible  */
				  return 0;
				}
			    }
			  return 1;
			}
		    }
		  return 1;
		}
	    }
	  /* <Protocol> = <class> *  */
	  else if (TYPED_OBJECT (TREE_TYPE (rhs)))
	    {
	      /* APPLE LOCAL type aliasing */
	      tree rname = OBJC_TYPE_NAME (TREE_TYPE (rhs));
	      tree rinter;

	      /* Make sure the protocol is supported by the object on
		 the rhs.  */
	      for (lproto = lproto_list; lproto; lproto = TREE_CHAIN (lproto))
		{
		  p = TREE_VALUE (lproto);
		  rproto = 0;
		  rinter = lookup_interface (rname);

		  while (rinter && !rproto)
		    {
		      tree cat;

		      rproto_list = CLASS_PROTOCOL_LIST (rinter);
		      rproto = lookup_protocol_in_reflist (rproto_list, p);
		      /* If the underlying ObjC class does not have
			 the protocol we're looking for, check for "one-off"
			 protocols (e.g., `NSObject<MyProt> *foo;') attached
			 to the rhs.  */
		      if (!rproto)
			{
			  /* APPLE LOCAL SPEC */
			  rproto_list = type_protocol_list (TREE_TYPE (rhs));
			  rproto = lookup_protocol_in_reflist (rproto_list, p);
			}

		      /* Check for protocols adopted by categories.  */
		      cat = CLASS_CATEGORY_LIST (rinter);
		      while (cat && !rproto)
			{
			  rproto_list = CLASS_PROTOCOL_LIST (cat);
			  rproto = lookup_protocol_in_reflist (rproto_list, p);
			  cat = CLASS_CATEGORY_LIST (cat);
			}

		      rinter = lookup_interface (CLASS_SUPER_NAME (rinter));
		    }

		  if (!rproto)
		    warning ("class `%s' does not implement the `%s' protocol",
			     /* APPLE LOCAL type aliasing */
			     IDENTIFIER_POINTER (OBJC_TYPE_NAME (TREE_TYPE (rhs))),
			     IDENTIFIER_POINTER (PROTOCOL_NAME (p)));
		}
	      return 1;
	    }
	  /* <Protocol> = id */
	  /* APPLE LOCAL type aliasing */
	  else if (OBJC_TYPE_NAME (TREE_TYPE (rhs)) == objc_object_id)
	    {
	      return 1;
	    }
	  /* <Protocol> = Class */
	  /* APPLE LOCAL type aliasing */
	  else if (OBJC_TYPE_NAME (TREE_TYPE (rhs)) == objc_class_id)
	    {
	      return 0;
	    }
	  /* <Protocol> = ?? : let comptypes decide.  */
          return -1;
        }
      else if (rhs_is_proto)
	{
	  /* <class> * = <Protocol> */
	  if (TYPED_OBJECT (TREE_TYPE (lhs)))
	    {
	      if (reflexive)
		{
		  /* APPLE LOCAL type aliasing */
		  tree rname = OBJC_TYPE_NAME (TREE_TYPE (lhs));
		  tree rinter;
		  /* APPLE LOCAL SPEC */
		  tree rproto, rproto_list = type_protocol_list (rhs);
		  
		  /* Make sure the protocol is supported by the object on
		     the lhs.  */
		  for (rproto = rproto_list; rproto; 
		       rproto = TREE_CHAIN (rproto))
		    {
		      tree p = TREE_VALUE (rproto);
		      tree lproto = 0;
		      rinter = lookup_interface (rname);

		      while (rinter && !lproto)
			{
			  tree cat;

			  tree lproto_list = CLASS_PROTOCOL_LIST (rinter);
			  lproto = lookup_protocol_in_reflist (lproto_list, p);
			  /* If the underlying ObjC class does not
			     have the protocol we're looking for,
			     check for "one-off" protocols (e.g.,
			     `NSObject<MyProt> *foo;') attached to the
			     lhs.  */
			  if (!lproto)
			    {
			      /* APPLE LOCAL SPEC */
			      lproto_list = type_protocol_list 
				(TREE_TYPE (lhs));
			      lproto = lookup_protocol_in_reflist 
				(lproto_list, p);
			    }

			  /* Check for protocols adopted by categories.  */
			  cat = CLASS_CATEGORY_LIST (rinter);
			  while (cat && !lproto)
			    {
			      lproto_list = CLASS_PROTOCOL_LIST (cat);
			      lproto = lookup_protocol_in_reflist (lproto_list,
								   p);
			      cat = CLASS_CATEGORY_LIST (cat);
			    }
			  
			  rinter = lookup_interface (CLASS_SUPER_NAME 
						     (rinter));
			}
		      
		      if (!lproto)
			warning ("class `%s' does not implement the `%s' protocol",
				 /* APPLE LOCAL type aliasing */
				 IDENTIFIER_POINTER (OBJC_TYPE_NAME 
						     (TREE_TYPE (lhs))),
				 IDENTIFIER_POINTER (PROTOCOL_NAME (p)));
		    }
		  return 1;
		}
	      else
		return 0;
	    }
	  /* id = <Protocol> */
	  /* APPLE LOCAL type aliasing */
	  else if (OBJC_TYPE_NAME (TREE_TYPE (lhs)) == objc_object_id)
	    {
	      return 1;
	    }
	  /* Class = <Protocol> */
	  /* APPLE LOCAL type aliasing */
	  else if (OBJC_TYPE_NAME (TREE_TYPE (lhs)) == objc_class_id)
	    {
	      return 0;
	    }
	  /* ??? = <Protocol> : let comptypes decide */
	  else
	    {
	      return -1;
	    }
	}
      else
	{
	  /* Attention: we shouldn't defer to comptypes here.  One bad
	     side effect would be that we might loose the REFLEXIVE
	     information.
	  */
	  lhs = TREE_TYPE (lhs);
	  rhs = TREE_TYPE (rhs);
	}
    }

  if (TREE_CODE (lhs) != RECORD_TYPE || TREE_CODE (rhs) != RECORD_TYPE)
    {
      /* Nothing to do with ObjC - let immediately comptypes take
	 responsibility for checking.  */
      return -1;
    }

  /* `id' = `<class> *' `<class> *' = `id': always allow it.
     Please note that 
     'Object *o = [[Object alloc] init]; falls
     in the case <class> * = `id'.
  */
  /* APPLE LOCAL begin type aliasing */
  if ((OBJC_TYPE_NAME (lhs) == objc_object_id && TYPED_OBJECT (rhs))
      || (OBJC_TYPE_NAME (rhs) == objc_object_id && TYPED_OBJECT (lhs)))
  /* APPLE LOCAL end type aliasing */    
    return 1;

  /* `id' = `Class', `Class' = `id' */

  /* APPLE LOCAL begin type aliasing */
  else if ((OBJC_TYPE_NAME (lhs) == objc_object_id
	    && OBJC_TYPE_NAME (rhs) == objc_class_id)
	   || (OBJC_TYPE_NAME (lhs) == objc_class_id
	       && OBJC_TYPE_NAME (rhs) == objc_object_id))
  /* APPLE LOCAL end type aliasing */
    return 1;

  /* `<class> *' = `<class> *' */

  else if (TYPED_OBJECT (lhs) && TYPED_OBJECT (rhs))
    {
      /* APPLE LOCAL begin type aliasing */
      tree lname = OBJC_TYPE_NAME (lhs);
      tree rname = OBJC_TYPE_NAME (rhs);
      /* APPLE LOCAL end type aliasing */
      tree inter;

      if (lname == rname)
	return 1;

      /* If the left hand side is a super class of the right hand side,
	 allow it.  */
      for (inter = lookup_interface (rname); inter;
	   inter = lookup_interface (CLASS_SUPER_NAME (inter)))
	if (lname == CLASS_SUPER_NAME (inter))
	  return 1;

      /* Allow the reverse when reflexive.  */
      if (reflexive)
	for (inter = lookup_interface (lname); inter;
	     inter = lookup_interface (CLASS_SUPER_NAME (inter)))
	  if (rname == CLASS_SUPER_NAME (inter))
	    return 1;

      return 0;
    }
  else
    /* Not an ObjC type - let comptypes do the check.  */
    return -1;
}

/* APPLE LOCAL objc typedef */
/* Called from finish_decl.  */

void
objc_check_decl (decl)
     tree decl;
{
  /* APPLE LOCAL begin objc typedef */
  tree type = TREE_TYPE (decl);

  if (TREE_CODE (type) != RECORD_TYPE)
    return;
  if (TYPE_NAME (type) && (type = is_class_name (TYPE_NAME (type))))
    error ("statically allocated instance of Objective-C class `%s'",
	     IDENTIFIER_POINTER (type));
  /* APPLE LOCAL end objc typedef */
}

/* Implement static typing.  At this point, we know we have an interface.  */

tree
get_static_reference (interface, protocols)
     tree interface;
     tree protocols;
{
  tree type = xref_tag (RECORD_TYPE, interface);

  if (protocols)
    {
      tree t, m = TYPE_MAIN_VARIANT (type);

      t = copy_node (type);

      /* Add this type to the chain of variants of TYPE.  */
      TYPE_NEXT_VARIANT (t) = TYPE_NEXT_VARIANT (m);
      TYPE_NEXT_VARIANT (m) = t;

      /* Look up protocols and install in lang specific list.  Note
	 that the protocol list can have a different lifetime than T!  */
      TYPE_PROTOCOL_LIST (t) = lookup_and_install_protocols (protocols);

      /* This forces a new pointer type to be created later
	 (in build_pointer_type)...so that the new template
	 we just created will actually be used...what a hack!  */
      if (TYPE_POINTER_TO (t))
	TYPE_POINTER_TO (t) = NULL_TREE;

      type = t;
    }

  return type;
}

tree
get_object_reference (protocols)
     tree protocols;
{
  tree type_decl = lookup_name (objc_id_id);
  tree type;

  if (type_decl && TREE_CODE (type_decl) == TYPE_DECL)
    {
      type = TREE_TYPE (type_decl);
      if (TYPE_MAIN_VARIANT (type) != id_type)
	warning ("unexpected type for `id' (%s)",
		 gen_declaration (type, errbuf));
    }
  else
    {
      error ("undefined type `id', please import <objc/objc.h>");
      return error_mark_node;
    }

  /* This clause creates a new pointer type that is qualified with
     the protocol specification...this info is used later to do more
     elaborate type checking.  */

  if (protocols)
    {
      tree t, m = TYPE_MAIN_VARIANT (type);

      t = copy_node (type);

      /* Add this type to the chain of variants of TYPE.  */
      TYPE_NEXT_VARIANT (t) = TYPE_NEXT_VARIANT (m);
      TYPE_NEXT_VARIANT (m) = t;

      /* Look up protocols...and install in lang specific list */
      TYPE_PROTOCOL_LIST (t) = lookup_and_install_protocols (protocols);

      /* This forces a new pointer type to be created later
	 (in build_pointer_type)...so that the new template
	 we just created will actually be used...what a hack!  */
      if (TYPE_POINTER_TO (t))
	TYPE_POINTER_TO (t) = NULL_TREE;

      type = t;
    }
  return type;
}

/* Check for circular dependencies in protocols.  The arguments are
   PROTO, the protocol to check, and LIST, a list of protocol it
   conforms to.  */

static void 
check_protocol_recursively (proto, list)
     tree proto;
     tree list;
{
  tree p;

  for (p = list; p; p = TREE_CHAIN (p))
    {
      tree pp = TREE_VALUE (p);

      if (TREE_CODE (pp) == IDENTIFIER_NODE)
	pp = lookup_protocol (pp);

      if (pp == proto)
	fatal_error ("protocol `%s' has circular dependency",
		     IDENTIFIER_POINTER (PROTOCOL_NAME (pp)));      
      if (pp)
	check_protocol_recursively (proto, PROTOCOL_LIST (pp));
    }
}

static tree
lookup_and_install_protocols (protocols)
     tree protocols;
{
  tree proto;
  tree prev = NULL;
  tree return_value = protocols;

  for (proto = protocols; proto; proto = TREE_CHAIN (proto))
    {
      tree ident = TREE_VALUE (proto);
      tree p = lookup_protocol (ident);

      if (!p)
	{
	  /* APPLE LOCAL begin Panther ObjC enhancements */
	  error ("cannot find protocol declaration for `%s'",
		 IDENTIFIER_POINTER (ident));
	  return error_mark_node;
	  /* APPLE LOCAL end Panther ObjC enhancements */
	}
      else
	{
	  /* Replace identifier with actual protocol node.  */
	  TREE_VALUE (proto) = p;
	  prev = proto;
	}
    }

  return return_value;
}

/* Create and push a decl for a built-in external variable or field NAME.
   CODE says which.
   TYPE is its data type.  */

static tree
create_builtin_decl (code, type, name)
     enum tree_code code;
     tree type;
     const char *name;
{
  tree decl = build_decl (code, get_identifier (name), type);

  if (code == VAR_DECL)
    {
      TREE_STATIC (decl) = 1;
      make_decl_rtl (decl, 0);
      pushdecl (decl);
      /* APPLE LOCAL begin Objective-C++ */
      DECL_ARTIFICIAL (decl) = 1;
    }
/* APPLE LOCAL end Objective-C++ */

  return decl;
}

/* Find the decl for the constant string class.  */

static void
setup_string_decl ()
{
  if (!string_class_decl)
    {
     /* APPLE LOCAL begin constant strings */
      if (!constant_string_global_id)
	{
	  char *name;
	  size_t length;
	  /* %s in format will provide room for terminating null */
	  length = strlen (STRING_OBJECT_GLOBAL_FORMAT) 
		   + strlen (constant_string_class_name);
	  name = xmalloc (length);
	  sprintf (name, STRING_OBJECT_GLOBAL_FORMAT, 
		   constant_string_class_name);
	  constant_string_global_id = get_identifier (name);
	}
      /* APPLE LOCAL end constant strings */
      string_class_decl = lookup_name (constant_string_global_id);
    }
}

/* Purpose: "play" parser, creating/installing representations
   of the declarations that are required by Objective-C.

   Model:

 	type_spec--------->sc_spec
 	(tree_list)        (tree_list)
 	    |                  |
 	    |                  |
 	identifier_node    identifier_node  */

static void
synth_module_prologue ()
{
  tree temp_type;
  /* APPLE LOCAL msg send super */

  /* APPLE LOCAL Objective-C++  */
#ifdef OBJCPLUS
  push_lang_context (lang_name_c); /* extern "C" */
#endif

  /* Defined in `objc.h' */
  objc_object_id = get_identifier (TAG_OBJECT);

  objc_object_reference = xref_tag (RECORD_TYPE, objc_object_id);

  id_type = build_pointer_type (objc_object_reference);

  objc_id_id = get_identifier (TYPE_ID);
  objc_class_id = get_identifier (TAG_CLASS);

  objc_class_type = build_pointer_type (xref_tag (RECORD_TYPE, objc_class_id));
  /* APPLE LOCAL Objective-C++ */
  temp_type = get_identifier (PROTOCOL_OBJECT_CLASS_NAME);
  objc_declare_class (tree_cons (NULL_TREE, temp_type, NULL_TREE));
  protocol_type = build_pointer_type (xref_tag (RECORD_TYPE,
                                temp_type));

  /* Declare type of selector-objects that represent an operation name.  */

  /* `struct objc_selector *' */
  selector_type
    = build_pointer_type (xref_tag (RECORD_TYPE,
				    get_identifier (TAG_SELECTOR)));

  /* Forward declare type, or else the prototype for msgSendSuper will
     complain.  */

  /* APPLE LOCAL begin msg send super */
  /* `struct objc_super *' */
  super_type = build_pointer_type (xref_tag (RECORD_TYPE,
					  get_identifier (TAG_SUPER)));
  /* APPLE LOCAL end msg send super */


  /* id objc_msgSend (id, SEL, ...); */

  temp_type
    = build_function_type (id_type,
			   tree_cons (NULL_TREE, id_type,
				      tree_cons (NULL_TREE, selector_type,
						 NULL_TREE)));

  if (! flag_next_runtime)
    {
      umsg_decl = build_decl (FUNCTION_DECL,
			      get_identifier (TAG_MSGSEND), temp_type);
      DECL_EXTERNAL (umsg_decl) = 1;
      TREE_PUBLIC (umsg_decl) = 1;
      DECL_INLINE (umsg_decl) = 1;
      DECL_ARTIFICIAL (umsg_decl) = 1;

      make_decl_rtl (umsg_decl, NULL);
      pushdecl (umsg_decl);
    }
  else
    /* APPLE LOCAL begin Panther ObjC enhancements */
    {
      umsg_decl = builtin_function (TAG_MSGSEND, temp_type, 0, NOT_BUILT_IN, NULL, NULL_TREE);
      /* id objc_msgSendNonNil (id, SEL, ...); */
      umsg_nonnil_decl = builtin_function (TAG_MSGSEND_NONNIL, temp_type, 0, NOT_BUILT_IN, NULL, NULL_TREE);
    }
    /* APPLE LOCAL end Panther ObjC enhancements */

  /* id objc_msgSendSuper (struct objc_super *, SEL, ...); */

  temp_type
    = build_function_type (id_type,
			   tree_cons (NULL_TREE, super_type,
				      tree_cons (NULL_TREE, selector_type,
						 NULL_TREE)));

  umsg_super_decl = builtin_function (TAG_MSGSENDSUPER,
				      temp_type, 0, NOT_BUILT_IN,
				      NULL, NULL_TREE);

  /* APPLE LOCAL begin objc stret methods */

  /* The NeXT runtime defines the following additional entry points,
     used for dispatching calls to methods returning structs:
     
     #if defined(__cplusplus)
       id objc_msgSend_stret(id self, SEL op, ...);
       id objc_msgSendSuper_stret(struct objc_super *super, SEL op, ...);
     #else
       void objc_msgSend_stret(void * stretAddr, id self, SEL op, ...);
       void objc_msgSendSuper_stret(void * stretAddr, struct objc_super *super, 
				    SEL op, ...);
     #endif

     struct objc_return_struct objc_msgSendNonNil_stret(id self, SEL op, ...);
       
     These prototypes appear in <objc/objc-runtime.h>; however, they
     CANNOT BE USED DIRECTLY.  In order to call one of the ..._stret
     functions, the function must first be cast to a signature that
     corresponds to the actual ObjC method being invoked.  This is 
     what is done by the build_objc_method_call() routine below. */

  if (flag_next_runtime)
    {
      /* APPLE LOCAL begin Panther ObjC enhancements */
      tree objc_return_struct_type 
	= xref_tag (RECORD_TYPE, 
		    get_identifier (TAG_RETURN_STRUCT));
      /* APPLE LOCAL end Panther ObjC enhancements */

      tree stret_temp_type
	= build_function_type (id_type,
			       tree_cons (NULL_TREE, id_type,
					  tree_cons (NULL_TREE, selector_type,
						     NULL_TREE)));

      umsg_stret_decl =
	builtin_function (TAG_MSGSEND_STRET, stret_temp_type, 0, NOT_BUILT_IN, NULL, NULL_TREE);
      /* APPLE LOCAL begin Panther ObjC enhancements */
      stret_temp_type
	= build_function_type (objc_return_struct_type,
			       tree_cons (NULL_TREE, id_type,
					  tree_cons (NULL_TREE, selector_type,
						     NULL_TREE)));

      umsg_nonnil_stret_decl =
	builtin_function (TAG_MSGSEND_NONNIL_STRET, stret_temp_type, 0, NOT_BUILT_IN, NULL, NULL_TREE);
      /* APPLE LOCAL end Panther ObjC enhancements */

      stret_temp_type
	= build_function_type (id_type,
			       tree_cons (NULL_TREE, super_type,
					  tree_cons (NULL_TREE, selector_type,
						     NULL_TREE)));

      umsg_super_stret_decl = builtin_function (TAG_MSGSENDSUPER_STRET,
						stret_temp_type, 0, NOT_BUILT_IN, 0,
						NULL_TREE);
    }
  /* APPLE LOCAL end objc stret methods */

  /* id objc_getClass (const char *); */

  temp_type = build_function_type (id_type,
			tree_cons (NULL_TREE,
				   const_string_type_node,
				   /* APPLE LOCAL Panther ObjC enhancements */
				   OBJC_VOID_AT_END));

  objc_get_class_decl
    = builtin_function (TAG_GETCLASS, temp_type, 0, NOT_BUILT_IN,
			NULL, NULL_TREE);

  /* id objc_getMetaClass (const char *); */

  objc_get_meta_class_decl
    = builtin_function (TAG_GETMETACLASS, temp_type, 0, NOT_BUILT_IN, NULL, NULL_TREE);

  /* APPLE LOCAL msg send super */
  build_super_template ();
  /* APPLE LOCAL Panther ObjC enhancements */
  build_objc_exception_stuff ();
    

  /* static SEL _OBJC_SELECTOR_TABLE[]; */

  if (! flag_next_runtime)
    {
      if (flag_typed_selectors)
	{
	  /* Suppress outputting debug symbols, because
	     dbxout_init hasn'r been called yet.  */
	  enum debug_info_type save_write_symbols = write_symbols;
	  const struct gcc_debug_hooks *const save_hooks = debug_hooks;
	  write_symbols = NO_DEBUG;
	  debug_hooks = &do_nothing_debug_hooks;

	  build_selector_template ();
	  temp_type = build_array_type (objc_selector_template, NULL_TREE);

	  write_symbols = save_write_symbols;
	  debug_hooks = save_hooks;
	}
      else
	temp_type = build_array_type (selector_type, NULL_TREE);

      layout_type (temp_type);
      UOBJC_SELECTOR_TABLE_decl
	= create_builtin_decl (VAR_DECL, temp_type,
			       "_OBJC_SELECTOR_TABLE");

      /* Avoid warning when not sending messages.  */
      TREE_USED (UOBJC_SELECTOR_TABLE_decl) = 1;
    }

  generate_forward_declaration_to_string_table ();

  /* Forward declare constant_string_id and constant_string_type.  */
  if (!constant_string_class_name)
    constant_string_class_name = default_constant_string_class_name;

  constant_string_id = get_identifier (constant_string_class_name);
  /* APPLE LOCAL begin Objective-C++ */
  objc_declare_class (tree_cons (NULL_TREE, constant_string_id, NULL_TREE));

  /* Pre-build the following entities - for speed/convenience.  */
  self_id = get_identifier ("self");
  ucmd_id = get_identifier ("_cmd");
#ifndef OBJCPLUS
  /* The C++ front-end does not appear to grok __attribute__((__unused__)).  */
  unused_list = build_tree_list (get_identifier ("__unused__"), NULL_TREE);
#endif	  

#ifdef OBJCPLUS
  pop_lang_context ();
#endif
  /* APPLE LOCAL end Objective-C++ */
}

/* APPLE LOCAL begin constant strings */
/* Ensure that the ivar list for NSConstantString/NXConstantString
   (or whatever was specified via `-fconstant-string-class')
   contains fields at least as large as the following three, so that
   the runtime can stomp on them with confidence:

   struct STRING_OBJECT_CLASS_NAME 
   {
     Object isa;
     char *cString;
     unsigned int length;
   }; */

static int
check_string_class_template ()
{
  tree field_decl = TYPE_FIELDS (constant_string_type);

#define AT_LEAST_AS_LARGE_AS(F, T) \
  (F && TREE_CODE (F) == FIELD_DECL \
     && (TREE_INT_CST_LOW (DECL_SIZE (F)) \
	 >= TREE_INT_CST_LOW (TYPE_SIZE (T))))
  
  if (!AT_LEAST_AS_LARGE_AS (field_decl, ptr_type_node))
    return 0;
    
  field_decl = TREE_CHAIN (field_decl);
  if (!AT_LEAST_AS_LARGE_AS (field_decl, ptr_type_node))
    return 0;
  
  field_decl = TREE_CHAIN (field_decl);
  return AT_LEAST_AS_LARGE_AS (field_decl, unsigned_type_node);
  
#undef AT_LEAST_AS_LARGE_AS
}

/* Avoid calling `check_string_class_template ()' more than once.  */
static GTY(()) int string_layout_checked;  
/* APPLE LOCAL end constant strings */

/* Custom build_string which sets TREE_TYPE!  */

static tree
my_build_string (len, str)
     int len;
     const char *str;
{
  return fix_string_type (build_string (len, str));
}

/* Given a chain of STRING_CST's, build a static instance of
   NXConstantString which points at the concatenation of those strings.
   We place the string object in the __string_objects section of the
   __OBJC segment.  The Objective-C runtime will initialize the isa
   pointers of the string objects to point at the NXConstantString
   class object.  */

tree
build_objc_string_object (strings)
     tree strings;
{
  tree string, initlist, constructor, constant_string_class;
  int length;

  /* APPLE LOCAL constant cfstrings */
  /* Move string combining up.  */
  if (TREE_CHAIN (strings))
    {
      varray_type vstrings;
      VARRAY_TREE_INIT (vstrings, 32, "strings");

      for (; strings ; strings = TREE_CHAIN (strings))
	VARRAY_PUSH_TREE (vstrings, strings);

      string = combine_strings (vstrings);
    }
  else
    string = strings;

  string = fix_string_type (string);

  /* APPLE LOCAL begin constant strings */
  constant_string_class = lookup_interface (constant_string_id);
  if (!constant_string_class
      || !(constant_string_type
	     = CLASS_STATIC_TEMPLATE (constant_string_class)))
    {
      error ("cannot find interface declaration for `%s'",
	     IDENTIFIER_POINTER (constant_string_id));
      return error_mark_node;
    }

  /* The '-fconstant-cfstrings' switch trumps any '-fconstant-string-class'
     setting.  We must, however, cast the CFStringRef to NSConstantString *.  */
  if (flag_constant_cfstrings)
    return build_c_cast (build_pointer_type (constant_string_type), 
			 build_cfstring_ascii (string));
  
  /* Call to 'combine_strings' has been moved above.  */
  TREE_SET_CODE (string, STRING_CST);
  length = TREE_STRING_LENGTH (string) - 1;

  if (!string_layout_checked)
    {    
      /* The NSConstantString/NXConstantString ivar layout is now
	 known.  */
      if (!check_string_class_template ())
	{
	  error ("interface `%s' does not have valid constant string layout",
		 IDENTIFIER_POINTER (constant_string_id));
	  return error_mark_node;
	}
      add_class_reference (constant_string_id);
    }
  /* APPLE LOCAL end constant strings */

  /* & ((NXConstantString) { NULL, string, length })  */

  if (flag_next_runtime)
    {
      /* For the NeXT runtime, we can generate a literal reference
	 to the string class, don't need to run a constructor.  */
      setup_string_decl ();
      if (string_class_decl == NULL_TREE)
	{
	  error ("cannot find reference tag for class `%s'",
		 IDENTIFIER_POINTER (constant_string_id));
	  return error_mark_node;
	}
      initlist = build_tree_list
	(NULL_TREE,
	 copy_node (build_unary_op (ADDR_EXPR, string_class_decl, 0)));
    }
  else
    {
      initlist = build_tree_list (NULL_TREE, build_int_2 (0, 0));
    }

  initlist
    = tree_cons (NULL_TREE, copy_node (build_unary_op (ADDR_EXPR, string, 1)),
		 initlist);
  initlist = tree_cons (NULL_TREE, build_int_2 (length, 0), initlist);
  constructor = build_constructor (constant_string_type, nreverse (initlist));

  if (!flag_next_runtime)
    {
      constructor
	= objc_add_static_instance (constructor, constant_string_type);
    }

  return (build_unary_op (ADDR_EXPR, constructor, 1));
}

/* Declare a static instance of CLASS_DECL initialized by CONSTRUCTOR.  */

static GTY(()) int num_static_inst;
static tree
objc_add_static_instance (constructor, class_decl)
     tree constructor, class_decl;
{
  tree *chain, decl;
  char buf[256];

  /* Find the list of static instances for the CLASS_DECL.  Create one if
     not found.  */
  for (chain = &objc_static_instances;
       *chain && TREE_VALUE (*chain) != class_decl;
       chain = &TREE_CHAIN (*chain));
  if (!*chain)
    {
      *chain = tree_cons (NULL_TREE, class_decl, NULL_TREE);
      /* APPLE LOCAL type aliasing */
      add_objc_string (OBJC_TYPE_NAME (class_decl), class_names);
    }

  sprintf (buf, "_OBJC_INSTANCE_%d", num_static_inst++);
  decl = build_decl (VAR_DECL, get_identifier (buf), class_decl);
  DECL_COMMON (decl) = 1;
  TREE_STATIC (decl) = 1;
  DECL_ARTIFICIAL (decl) = 1;
  DECL_INITIAL (decl) = constructor;

  /* We may be writing something else just now.
     Postpone till end of input.  */
  DECL_DEFER_OUTPUT (decl) = 1;
  pushdecl_top_level (decl);
  rest_of_decl_compilation (decl, 0, 1, 0);

  /* Add the DECL to the head of this CLASS' list.  */
  TREE_PURPOSE (*chain) = tree_cons (NULL_TREE, decl, TREE_PURPOSE (*chain));

  return decl;
}

/* Build a static constant CONSTRUCTOR
   with type TYPE and elements ELTS.  */

static tree
build_constructor (type, elts)
     tree type, elts;
{
  tree constructor, f, e;

  /* ??? Most of the places that we build constructors, we don't fill in
     the type of integers properly.  Convert them all en masse.  */
  if (TREE_CODE (type) == ARRAY_TYPE)
    {
      f = TREE_TYPE (type);
      if (TREE_CODE (f) == POINTER_TYPE || TREE_CODE (f) == INTEGER_TYPE)
	for (e = elts; e ; e = TREE_CHAIN (e))
	  TREE_VALUE (e) = convert (f, TREE_VALUE (e));
    }
  else
    {
      f = TYPE_FIELDS (type);
      for (e = elts; e && f; e = TREE_CHAIN (e), f = TREE_CHAIN (f))
	if (TREE_CODE (TREE_TYPE (f)) == POINTER_TYPE
	    || TREE_CODE (TREE_TYPE (f)) == INTEGER_TYPE)
	  TREE_VALUE (e) = convert (TREE_TYPE (f), TREE_VALUE (e));
    }

  constructor = build (CONSTRUCTOR, type, NULL_TREE, elts);
  TREE_CONSTANT (constructor) = 1;
  TREE_STATIC (constructor) = 1;
  TREE_READONLY (constructor) = 1;

  /* APPLE LOCAL begin Objective-C++ */
#ifdef OBJCPLUS  
  /* zlaski 2001-Apr-02: mark this as a call to a constructor, as required by
     build_unary_op (wasn't true in 2.7.2.1 days) */
  TREE_HAS_CONSTRUCTOR (constructor) = 1;   
#endif
  /* APPLE LOCAL end Objective-C++ */
  return constructor;
}

/* Take care of defining and initializing _OBJC_SYMBOLS.  */

/* Predefine the following data type:

   struct _objc_symtab
   {
     long sel_ref_cnt;
     SEL *refs;
     short cls_def_cnt;
     short cat_def_cnt;
     void *defs[cls_def_cnt + cat_def_cnt];
   }; */

static void
build_objc_symtab_template ()
{
  /* APPLE LOCAL Panther ObjC enhancements */
  tree field_decl, field_decl_chain;

  objc_symtab_template
    = start_struct (RECORD_TYPE, get_identifier (UTAG_SYMTAB));

  /* long sel_ref_cnt; */

  field_decl = create_builtin_decl (FIELD_DECL,
				    long_integer_type_node,
				    "sel_ref_cnt");
  field_decl_chain = field_decl;

  /* SEL *refs; */

  field_decl = create_builtin_decl (FIELD_DECL,
				    build_pointer_type (selector_type),
				    "refs");
  chainon (field_decl_chain, field_decl);

  /* short cls_def_cnt; */

  field_decl = create_builtin_decl (FIELD_DECL,
				    short_integer_type_node,
				    "cls_def_cnt");
  chainon (field_decl_chain, field_decl);

  /* short cat_def_cnt; */

  field_decl = create_builtin_decl (FIELD_DECL,
				    short_integer_type_node,
				    "cat_def_cnt");
  chainon (field_decl_chain, field_decl);

  /* APPLE LOCAL begin Panther ObjC enhancements */
  if (imp_count || cat_count || !flag_next_runtime)
    {
      /* void *defs[imp_count + cat_count (+ 1)]; */
      /* NB: The index is one less than the size of the array.  */
      int index = imp_count + cat_count
		+ (flag_next_runtime? -1: 0);
      field_decl = create_builtin_decl
		   (FIELD_DECL,
		    build_array_type
		    (ptr_type_node,
		     build_index_type (build_int_2 (index, 0))),
		    "defs");
      chainon (field_decl_chain, field_decl);
    }
  /* APPLE LOCAL end Panther ObjC enhancements */

  finish_struct (objc_symtab_template, field_decl_chain, NULL_TREE);
}

/* Create the initial value for the `defs' field of _objc_symtab.
   This is a CONSTRUCTOR.  */

static tree
init_def_list (type)
     tree type;
{
  tree expr, initlist = NULL_TREE;
  struct imp_entry *impent;

  if (imp_count)
    for (impent = imp_list; impent; impent = impent->next)
      {
	if (TREE_CODE (impent->imp_context) == CLASS_IMPLEMENTATION_TYPE)
	  {
	    expr = build_unary_op (ADDR_EXPR, impent->class_decl, 0);
	    initlist = tree_cons (NULL_TREE, expr, initlist);
	  }
      }

  if (cat_count)
    for (impent = imp_list; impent; impent = impent->next)
      {
	if (TREE_CODE (impent->imp_context) == CATEGORY_IMPLEMENTATION_TYPE)
	  {
	    expr = build_unary_op (ADDR_EXPR, impent->class_decl, 0);
	    initlist = tree_cons (NULL_TREE, expr, initlist);
	  }
      }

  if (!flag_next_runtime)
    {
      /* statics = { ..., _OBJC_STATIC_INSTANCES, ... }  */
      tree expr;

      if (static_instances_decl)
	expr = build_unary_op (ADDR_EXPR, static_instances_decl, 0);
      else
	expr = build_int_2 (0, 0);

      initlist = tree_cons (NULL_TREE, expr, initlist);
    }

  return build_constructor (type, nreverse (initlist));
}

/* Construct the initial value for all of _objc_symtab.  */

static tree
init_objc_symtab (type)
     tree type;
{
  tree initlist;

  /* sel_ref_cnt = { ..., 5, ... } */

  initlist = build_tree_list (NULL_TREE, build_int_2 (0, 0));

  /* refs = { ..., _OBJC_SELECTOR_TABLE, ... } */

  if (flag_next_runtime || ! sel_ref_chain)
    initlist = tree_cons (NULL_TREE, build_int_2 (0, 0), initlist);
  else
    initlist = tree_cons (NULL_TREE,
			  build_unary_op (ADDR_EXPR,
					  UOBJC_SELECTOR_TABLE_decl, 1),
			  initlist);

  /* cls_def_cnt = { ..., 5, ... } */

  initlist = tree_cons (NULL_TREE, build_int_2 (imp_count, 0), initlist);

  /* cat_def_cnt = { ..., 5, ... } */

  initlist = tree_cons (NULL_TREE, build_int_2 (cat_count, 0), initlist);

  /* cls_def = { ..., { &Foo, &Bar, ...}, ... } */

  /* APPLE LOCAL Panther ObjC enhancements */
  if (imp_count || cat_count || !flag_next_runtime)
    {

      tree field = TYPE_FIELDS (type);
      field = TREE_CHAIN (TREE_CHAIN (TREE_CHAIN (TREE_CHAIN (field))));

      initlist = tree_cons (NULL_TREE, init_def_list (TREE_TYPE (field)),
			    initlist);
    }

  return build_constructor (type, nreverse (initlist));
}

/* APPLE LOCAL begin Objective-C++ */
/* Generate forward declarations for metadata such as 
  'OBJC_CLASS_...'.  */
static tree
build_metadata_decl (name, type)
     const char *name; 
     tree type;
{
  tree decl, decl_specs;
  /* extern struct TYPE NAME_<name>; */
  decl_specs = build_tree_list (NULL_TREE, ridpointers[(int) RID_EXTERN]);
  decl_specs = tree_cons (NULL_TREE, type, decl_specs);
  decl = define_decl (synth_id_with_class_suffix 
		      (name, 
		       objc_implementation_context), 
		      decl_specs);
  TREE_USED (decl) = 1;
  DECL_ARTIFICIAL (decl) = 1;
  TREE_PUBLIC (decl) = 0;
  return decl;
}  
/* APPLE LOCAL end Objective-C++ */
       
/* Push forward-declarations of all the categories so that
   init_def_list can use them in a CONSTRUCTOR.  */

static void
forward_declare_categories ()
{
  struct imp_entry *impent;
  tree sav = objc_implementation_context;

  for (impent = imp_list; impent; impent = impent->next)
    {
      if (TREE_CODE (impent->imp_context) == CATEGORY_IMPLEMENTATION_TYPE)
	{
	  /* Set an invisible arg to synth_id_with_class_suffix.  */
	  objc_implementation_context = impent->imp_context;
	
	  /* APPLE LOCAL begin Objective-C++ */
	  /* extern struct objc_category _OBJC_CATEGORY_<name>; */
	  impent->class_decl = build_metadata_decl ("_OBJC_CATEGORY",
						    objc_category_template);			    
	  /* APPLE LOCAL end Objective-C++ */
	}
    }
  objc_implementation_context = sav;
}

/* Create the declaration of _OBJC_SYMBOLS, with type `struct _objc_symtab'
   and initialized appropriately.  */

static void
generate_objc_symtab_decl ()
{
  tree sc_spec;

  if (!objc_category_template)
    build_category_template ();

  /* forward declare categories */
  if (cat_count)
    forward_declare_categories ();

  if (!objc_symtab_template)
    build_objc_symtab_template ();

  sc_spec = build_tree_list (NULL_TREE, ridpointers[(int) RID_STATIC]);

  /* APPLE LOCAL indexing */
  flag_suppress_builtin_indexing = 1;
  
  UOBJC_SYMBOLS_decl = start_decl (get_identifier ("_OBJC_SYMBOLS"),
				   tree_cons (NULL_TREE,
					      objc_symtab_template, sc_spec),
				   1,
				   NULL_TREE);

  TREE_USED (UOBJC_SYMBOLS_decl) = 1;
  DECL_IGNORED_P (UOBJC_SYMBOLS_decl) = 1;
  DECL_ARTIFICIAL (UOBJC_SYMBOLS_decl) = 1;
  finish_decl (UOBJC_SYMBOLS_decl,
	       init_objc_symtab (TREE_TYPE (UOBJC_SYMBOLS_decl)),
	       NULL_TREE);

  /* APPLE LOCAL indexing */
  flag_suppress_builtin_indexing = 0;
}

static tree
init_module_descriptor (type)
     tree type;
{
  tree initlist, expr;

  /* version = { 1, ... } */

  expr = build_int_2 (OBJC_VERSION, 0);
  initlist = build_tree_list (NULL_TREE, expr);

  /* size = { ..., sizeof (struct objc_module), ... } */

  expr = size_in_bytes (objc_module_template);
  initlist = tree_cons (NULL_TREE, expr, initlist);

  /* name = { ..., "foo.m", ... } */

  expr = add_objc_string (get_identifier (input_filename), class_names);
  initlist = tree_cons (NULL_TREE, expr, initlist);

  /* symtab = { ..., _OBJC_SYMBOLS, ... } */

  if (UOBJC_SYMBOLS_decl)
    expr = build_unary_op (ADDR_EXPR, UOBJC_SYMBOLS_decl, 0);
  else
    expr = build_int_2 (0, 0);
  initlist = tree_cons (NULL_TREE, expr, initlist);

  return build_constructor (type, nreverse (initlist));
}

/* Write out the data structures to describe Objective C classes defined.
   If appropriate, compile and output a setup function to initialize them.
   Return a symbol_ref to the function to call to initialize the Objective C
   data structures for this file (and perhaps for other files also).

   struct objc_module { ... } _OBJC_MODULE = { ... };   */

static rtx
build_module_descriptor ()
{
  tree decl_specs, field_decl, field_decl_chain;

  /* APPLE LOCAL Objective-C++  */
#ifdef OBJCPLUS
  push_lang_context (lang_name_c); /* extern "C" */
#endif

  objc_module_template
    = start_struct (RECORD_TYPE, get_identifier (UTAG_MODULE));

  /* Long version; */

  decl_specs = build_tree_list (NULL_TREE, ridpointers[(int) RID_LONG]);
  field_decl = get_identifier ("version");
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  field_decl_chain = field_decl;

  /* long  size; */

  decl_specs = build_tree_list (NULL_TREE, ridpointers[(int) RID_LONG]);
  field_decl = get_identifier ("size");
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  /* char  *name; */

  decl_specs = build_tree_list (NULL_TREE, ridpointers[(int) RID_CHAR]);
  field_decl = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("name"));
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  /* struct objc_symtab *symtab; */

  decl_specs = get_identifier (UTAG_SYMTAB);
  decl_specs = build_tree_list (NULL_TREE, xref_tag (RECORD_TYPE, decl_specs));
  field_decl = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("symtab"));
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  finish_struct (objc_module_template, field_decl_chain, NULL_TREE);

  /* Create an instance of "objc_module".  */

  decl_specs = tree_cons (NULL_TREE, objc_module_template,
			  build_tree_list (NULL_TREE,
					   ridpointers[(int) RID_STATIC]));

  /* APPLE LOCAL indexing */
  flag_suppress_builtin_indexing = 1;
  
  UOBJC_MODULES_decl = start_decl (get_identifier ("_OBJC_MODULES"),
				   decl_specs, 1, NULL_TREE);

  DECL_ARTIFICIAL (UOBJC_MODULES_decl) = 1;
  DECL_IGNORED_P (UOBJC_MODULES_decl) = 1;
  DECL_CONTEXT (UOBJC_MODULES_decl) = NULL_TREE;

  finish_decl (UOBJC_MODULES_decl,
	       init_module_descriptor (TREE_TYPE (UOBJC_MODULES_decl)),
	       NULL_TREE);

  /* Mark the decl to avoid "defined but not used" warning.  */
  DECL_IN_SYSTEM_HEADER (UOBJC_MODULES_decl) = 1;

  /* APPLE LOCAL indexing */
  flag_suppress_builtin_indexing = 0;
  
  /* Generate a constructor call for the module descriptor.
     This code was generated by reading the grammar rules
     of c-parse.in;  Therefore, it may not be the most efficient
     way of generating the requisite code.  */

  if (flag_next_runtime)
    return NULL_RTX;

  {
    tree parms, execclass_decl, decelerator, void_list_node_1;
    tree init_function_name, init_function_decl;

    /* Declare void __objc_execClass (void *); */

    void_list_node_1 = build_tree_list (NULL_TREE, void_type_node);
    /* APPLE LOCAL Panther ObjC enhancements */    
    execclass_decl = build_decl (FUNCTION_DECL,
				 get_identifier (TAG_EXECCLASS),
				 build_function_type (void_type_node,
					tree_cons (NULL_TREE, ptr_type_node,
						   /* APPLE LOCAL Panther ObjC enhancements */
						   OBJC_VOID_AT_END)));
						   
    DECL_EXTERNAL (execclass_decl) = 1;
    DECL_ARTIFICIAL (execclass_decl) = 1;
    TREE_PUBLIC (execclass_decl) = 1;
    pushdecl (execclass_decl);
    rest_of_decl_compilation (execclass_decl, 0, 0, 0);
    assemble_external (execclass_decl);

    /* void _GLOBAL_$I$<gnyf> () {objc_execClass (&L_OBJC_MODULES);}  */

    init_function_name = get_file_function_name ('I');
    parms = tree_cons (NULL_TREE, NULL_TREE, void_list_node_1);
    start_function (void_list_node_1,
		    build_nt (CALL_EXPR, init_function_name,
			      parms, NULL_TREE),
		    NULL_TREE);
    store_parm_decls ();

    init_function_decl = current_function_decl;
    TREE_PUBLIC (init_function_decl) = ! targetm.have_ctors_dtors;
    TREE_USED (init_function_decl) = 1;
    /* Don't let this one be deferred.  */
    DECL_INLINE (init_function_decl) = 0;
    DECL_UNINLINABLE (init_function_decl) = 1;
    current_function_cannot_inline
      = "static constructors and destructors cannot be inlined";

    parms
      = build_tree_list (NULL_TREE,
			 build_unary_op (ADDR_EXPR, UOBJC_MODULES_decl, 0));
    decelerator = build_function_call (execclass_decl, parms);

    c_expand_expr_stmt (decelerator);

    finish_function (0, 0);
    /* APPLE LOCAL begin Objective-C++  */
#ifdef OBJCPLUS
    pop_lang_context ();
#endif
    /* APPLE LOCAL end Objective-C++  */
    
    return XEXP (DECL_RTL (init_function_decl), 0);
  }
}

/* extern const char _OBJC_STRINGS[]; */

static void
generate_forward_declaration_to_string_table ()
{
  tree sc_spec, decl_specs, expr_decl;

  sc_spec = tree_cons (NULL_TREE, ridpointers[(int) RID_EXTERN], NULL_TREE);
  decl_specs = tree_cons (NULL_TREE, ridpointers[(int) RID_CHAR], sc_spec);

  expr_decl
    = build_nt (ARRAY_REF, get_identifier ("_OBJC_STRINGS"), NULL_TREE);

  UOBJC_STRINGS_decl = define_decl (expr_decl, decl_specs);
}

/* Return the DECL of the string IDENT in the SECTION.  */

static tree
get_objc_string_decl (ident, section)
     tree ident;
     enum string_section section;
{
  tree chain;

  if (section == class_names)
    chain = class_names_chain;
  else if (section == meth_var_names)
    chain = meth_var_names_chain;
  else if (section == meth_var_types)
    chain = meth_var_types_chain;
  else
    abort ();

  for (; chain != 0; chain = TREE_CHAIN (chain))
    if (TREE_VALUE (chain) == ident)
      return (TREE_PURPOSE (chain));

  abort ();
  return NULL_TREE;
}

/* Output references to all statically allocated objects.  Return the DECL
   for the array built.  */

static void
generate_static_references ()
{
  tree decls = NULL_TREE, ident, decl_spec, expr_decl, expr = NULL_TREE;
  tree class_name, class, decl, initlist;
  tree cl_chain, in_chain, type;
  int num_inst, num_class;
  char buf[256];

  if (flag_next_runtime)
    abort ();

  for (cl_chain = objc_static_instances, num_class = 0;
       cl_chain; cl_chain = TREE_CHAIN (cl_chain), num_class++)
    {
      for (num_inst = 0, in_chain = TREE_PURPOSE (cl_chain);
	   in_chain; num_inst++, in_chain = TREE_CHAIN (in_chain));

      sprintf (buf, "_OBJC_STATIC_INSTANCES_%d", num_class);
      ident = get_identifier (buf);

      expr_decl = build_nt (ARRAY_REF, ident, NULL_TREE);
      decl_spec = tree_cons (NULL_TREE, build_pointer_type (void_type_node),
			     build_tree_list (NULL_TREE,
					      ridpointers[(int) RID_STATIC]));
      decl = start_decl (expr_decl, decl_spec, 1, NULL_TREE);
      DECL_CONTEXT (decl) = 0;
      DECL_ARTIFICIAL (decl) = 1;

      /* Output {class_name, ...}.  */
      class = TREE_VALUE (cl_chain);
      /* APPLE LOCAL type aliasing */
      class_name = get_objc_string_decl (OBJC_TYPE_NAME (class), class_names);
      initlist = build_tree_list (NULL_TREE,
				  build_unary_op (ADDR_EXPR, class_name, 1));

      /* Output {..., instance, ...}.  */
      for (in_chain = TREE_PURPOSE (cl_chain);
	   in_chain; in_chain = TREE_CHAIN (in_chain))
	{
	  expr = build_unary_op (ADDR_EXPR, TREE_VALUE (in_chain), 1);
	  initlist = tree_cons (NULL_TREE, expr, initlist);
	}

      /* Output {..., NULL}.  */
      initlist = tree_cons (NULL_TREE, build_int_2 (0, 0), initlist);

      expr = build_constructor (TREE_TYPE (decl), nreverse (initlist));
      finish_decl (decl, expr, NULL_TREE);
      TREE_USED (decl) = 1;

      type = build_array_type (build_pointer_type (void_type_node), 0);
      decl = build_decl (VAR_DECL, ident, type);
      TREE_USED (decl) = 1;
      TREE_STATIC (decl) = 1;
      decls
	= tree_cons (NULL_TREE, build_unary_op (ADDR_EXPR, decl, 1), decls);
    }

  decls = tree_cons (NULL_TREE, build_int_2 (0, 0), decls);
  ident = get_identifier ("_OBJC_STATIC_INSTANCES");
  expr_decl = build_nt (ARRAY_REF, ident, NULL_TREE);
  decl_spec = tree_cons (NULL_TREE, build_pointer_type (void_type_node),
			 build_tree_list (NULL_TREE,
					  ridpointers[(int) RID_STATIC]));
  static_instances_decl
    = start_decl (expr_decl, decl_spec, 1, NULL_TREE);
  TREE_USED (static_instances_decl) = 1;
  DECL_CONTEXT (static_instances_decl) = 0;
  DECL_ARTIFICIAL (static_instances_decl) = 1;
  expr = build_constructor (TREE_TYPE (static_instances_decl),
			    nreverse (decls));
  finish_decl (static_instances_decl, expr, NULL_TREE);
}

/* Output all strings.  */

static void
generate_strings ()
{
  tree sc_spec, decl_specs, expr_decl;
  tree chain, string_expr;
  tree string, decl;

  for (chain = class_names_chain; chain; chain = TREE_CHAIN (chain))
    {
      string = TREE_VALUE (chain);
      decl = TREE_PURPOSE (chain);
      sc_spec
	= tree_cons (NULL_TREE, ridpointers[(int) RID_STATIC], NULL_TREE);
      decl_specs = tree_cons (NULL_TREE, ridpointers[(int) RID_CHAR], sc_spec);
      expr_decl = build_nt (ARRAY_REF, DECL_NAME (decl), NULL_TREE);
      decl = start_decl (expr_decl, decl_specs, 1, NULL_TREE);
      DECL_CONTEXT (decl) = NULL_TREE;
      string_expr = my_build_string (IDENTIFIER_LENGTH (string) + 1,
				     IDENTIFIER_POINTER (string));
      finish_decl (decl, string_expr, NULL_TREE);
    }

  for (chain = meth_var_names_chain; chain; chain = TREE_CHAIN (chain))
    {
      string = TREE_VALUE (chain);
      decl = TREE_PURPOSE (chain);
      sc_spec
	= tree_cons (NULL_TREE, ridpointers[(int) RID_STATIC], NULL_TREE);
      decl_specs = tree_cons (NULL_TREE, ridpointers[(int) RID_CHAR], sc_spec);
      expr_decl = build_nt (ARRAY_REF, DECL_NAME (decl), NULL_TREE);
      decl = start_decl (expr_decl, decl_specs, 1, NULL_TREE);
      DECL_CONTEXT (decl) = NULL_TREE;
      string_expr = my_build_string (IDENTIFIER_LENGTH (string) + 1,
				     IDENTIFIER_POINTER (string));
      finish_decl (decl, string_expr, NULL_TREE);
    }

  for (chain = meth_var_types_chain; chain; chain = TREE_CHAIN (chain))
    {
      string = TREE_VALUE (chain);
      decl = TREE_PURPOSE (chain);
      sc_spec
	= tree_cons (NULL_TREE, ridpointers[(int) RID_STATIC], NULL_TREE);
      decl_specs = tree_cons (NULL_TREE, ridpointers[(int) RID_CHAR], sc_spec);
      expr_decl = build_nt (ARRAY_REF, DECL_NAME (decl), NULL_TREE);
      decl = start_decl (expr_decl, decl_specs, 1, NULL_TREE);
      DECL_CONTEXT (decl) = NULL_TREE;
      string_expr = my_build_string (IDENTIFIER_LENGTH (string) + 1,
				IDENTIFIER_POINTER (string));
      finish_decl (decl, string_expr, NULL_TREE);
    }
}

static GTY(()) int selector_reference_idx;
static tree
build_selector_reference_decl ()
{
  tree decl, ident;
  char buf[256];
  sprintf (buf, "_OBJC_SELECTOR_REFERENCES_%d", selector_reference_idx++);

  ident = get_identifier (buf);

  decl = build_decl (VAR_DECL, ident, selector_type);
  DECL_EXTERNAL (decl) = 1;
  /* APPLE LOCAL Objective-C++ */
  /* TREE_PUBLIC (decl) = 1; */
  TREE_USED (decl) = 1;
  /* APPLE LOCAL Objective-C++ */
  /* TREE_READONLY (decl) = 1; */
  DECL_ARTIFICIAL (decl) = 1;
  DECL_CONTEXT (decl) = 0;

  make_decl_rtl (decl, 0);
  pushdecl_top_level (decl);

  return decl;
}

/* Just a handy wrapper for add_objc_string.  */

static tree
build_selector (ident)
     tree ident;
{
  tree expr = add_objc_string (ident, meth_var_names);
  if (flag_typed_selectors)
    return expr;
  else
    return build_c_cast (selector_type, expr); /* cast! */
}

static void
build_selector_translation_table ()
{
  tree sc_spec, decl_specs;
  tree chain, initlist = NULL_TREE;
  int offset = 0;
  tree decl = NULL_TREE, var_decl, name;

  for (chain = sel_ref_chain; chain; chain = TREE_CHAIN (chain))
    {
      tree expr;

      if (warn_selector && objc_implementation_context)
      {
        tree method_chain;
        bool found = false;
        for (method_chain = meth_var_names_chain;
             method_chain;
             method_chain = TREE_CHAIN (method_chain))
          {
            if (TREE_VALUE (method_chain) == TREE_VALUE (chain))
              {
                found = true;
                break;
              }
          }
        if (!found)
          {
            /* Adjust line number for warning message.  */
            int save_lineno = lineno;
            if (flag_next_runtime && TREE_PURPOSE (chain))
              lineno = DECL_SOURCE_LINE (TREE_PURPOSE (chain));
            warning ("creating selector for non existant method %s",
                     IDENTIFIER_POINTER (TREE_VALUE (chain)));
            lineno = save_lineno;
          }
      }

      expr = build_selector (TREE_VALUE (chain));

      if (flag_next_runtime)
	{
	  name = DECL_NAME (TREE_PURPOSE (chain));

	  sc_spec = build_tree_list (NULL_TREE, ridpointers[(int) RID_STATIC]);

	  /* static SEL _OBJC_SELECTOR_REFERENCES_n = ...; */
	  decl_specs = tree_cons (NULL_TREE, selector_type, sc_spec);

	  var_decl = name;

	  /* The `decl' that is returned from start_decl is the one that we
	     forward declared in `build_selector_reference'  */
	  decl = start_decl (var_decl, decl_specs, 1, NULL_TREE );
	}

      /* add one for the '\0' character */
      offset += IDENTIFIER_LENGTH (TREE_VALUE (chain)) + 1;

      if (flag_next_runtime)
	finish_decl (decl, expr, NULL_TREE);
      else 
	{
	  if (flag_typed_selectors)
	    {
	      tree eltlist = NULL_TREE;
	      tree encoding = get_proto_encoding (TREE_PURPOSE (chain));
	      eltlist = tree_cons (NULL_TREE, expr, NULL_TREE);
	      eltlist = tree_cons (NULL_TREE, encoding, eltlist);
	      expr = build_constructor (objc_selector_template,
					nreverse (eltlist));
	    }
	  initlist = tree_cons (NULL_TREE, expr, initlist);
	  
	}
    }

  if (! flag_next_runtime)
    {
      /* Cause the variable and its initial value to be actually output.  */
      DECL_EXTERNAL (UOBJC_SELECTOR_TABLE_decl) = 0;
      TREE_STATIC (UOBJC_SELECTOR_TABLE_decl) = 1;
      /* NULL terminate the list and fix the decl for output.  */
      initlist = tree_cons (NULL_TREE, build_int_2 (0, 0), initlist);
      DECL_INITIAL (UOBJC_SELECTOR_TABLE_decl) = objc_ellipsis_node;
      initlist = build_constructor (TREE_TYPE (UOBJC_SELECTOR_TABLE_decl),
				    nreverse (initlist));
      finish_decl (UOBJC_SELECTOR_TABLE_decl, initlist, NULL_TREE);
      current_function_decl = NULL_TREE;
    }
}

static tree
get_proto_encoding (proto)
     tree proto;
{
  tree encoding;
  if (proto)
    {
      /* APPLE LOCAL Panther ObjC enhancements */
      if (! METHOD_ENCODING (proto))
	{
	  /* APPLE LOCAL Panther ObjC enhancements */
	  encoding = encode_method_prototype (proto);
	  METHOD_ENCODING (proto) = encoding;
	}
      else
	encoding = METHOD_ENCODING (proto);

      return add_objc_string (encoding, meth_var_types);
    }
  else
    return build_int_2 (0, 0);
}

/* sel_ref_chain is a list whose "value" fields will be instances of
   identifier_node that represent the selector.  */

static tree
build_typed_selector_reference (ident, prototype)
     tree ident, prototype;
{
  tree *chain = &sel_ref_chain;
  tree expr;
  int index = 0;

  while (*chain)
    {
      if (TREE_PURPOSE (*chain) == prototype && TREE_VALUE (*chain) == ident)
	goto return_at_index;

      index++;
      chain = &TREE_CHAIN (*chain);
    }

  *chain = tree_cons (prototype, ident, NULL_TREE);

 return_at_index:
  expr = build_unary_op (ADDR_EXPR,
			 build_array_ref (UOBJC_SELECTOR_TABLE_decl,
					  build_int_2 (index, 0)),
			 1);
  return build_c_cast (selector_type, expr);
}

static tree
build_selector_reference (ident)
     tree ident;
{
  tree *chain = &sel_ref_chain;
  tree expr;
  int index = 0;

  while (*chain)
    {
      if (TREE_VALUE (*chain) == ident)
	return (flag_next_runtime
		? TREE_PURPOSE (*chain)
		: build_array_ref (UOBJC_SELECTOR_TABLE_decl,
				   build_int_2 (index, 0)));

      index++;
      chain = &TREE_CHAIN (*chain);
    }

  expr = build_selector_reference_decl ();

  *chain = tree_cons (expr, ident, NULL_TREE);

  return (flag_next_runtime
	  ? expr
	  : build_array_ref (UOBJC_SELECTOR_TABLE_decl,
			     build_int_2 (index, 0)));
}

static GTY(()) int class_reference_idx;
static tree
build_class_reference_decl ()
{
  tree decl, ident;
  char buf[256];
  sprintf (buf, "_OBJC_CLASS_REFERENCES_%d", class_reference_idx++);

  ident = get_identifier (buf);

  decl = build_decl (VAR_DECL, ident, objc_class_type);
  DECL_EXTERNAL (decl) = 1;
  /* APPLE LOCAL Objective-C++ */
  /* TREE_PUBLIC (decl) = 1; */
  TREE_USED (decl) = 1;
  /* APPLE LOCAL Objective-C++ */
  /* TREE_READONLY (decl) = 1; */
  DECL_CONTEXT (decl) = 0;
  DECL_ARTIFICIAL (decl) = 1;

  make_decl_rtl (decl, 0);
  pushdecl_top_level (decl);

  return decl;
}

/* Create a class reference, but don't create a variable to reference
   it.  */

static void
add_class_reference (ident)
     tree ident;
{
  tree chain;

  if ((chain = cls_ref_chain))
    {
      tree tail;
      do
        {
	  if (ident == TREE_VALUE (chain))
	    return;

	  tail = chain;
	  chain = TREE_CHAIN (chain);
        }
      while (chain);

      /* Append to the end of the list */
      TREE_CHAIN (tail) = tree_cons (NULL_TREE, ident, NULL_TREE);
    }
  else
    cls_ref_chain = tree_cons (NULL_TREE, ident, NULL_TREE);
}

/* Get a class reference, creating it if necessary.  Also create the
   reference variable.  */

tree
get_class_reference (ident)
     tree ident;
{
  /* APPLE LOCAL type aliasing */
  tree orig_ident;
  
  /* APPLE LOCAL begin Objective-C++ */
#ifdef OBJCPLUS
  if (processing_template_decl)
    /* Must wait until template instantiation time.  */
    return build_min_nt (CLASS_REFERENCE_EXPR, ident);
  if (TREE_CODE (ident) == TYPE_DECL)
    ident = DECL_NAME (ident);
#endif
  /* APPLE LOCAL end Objective-C++ */

  /* APPLE LOCAL type aliasing */
  orig_ident = ident;       

  /* APPLE LOCAL begin type aliasing */
  if (!(ident = is_class_name (ident)))
    {
      error ("`%s' is not an Objective-C class name or alias",
	     IDENTIFIER_POINTER (orig_ident));
      return error_mark_node;
    }
  /* APPLE LOCAL end type aliasing */
    
  /* APPLE LOCAL Panther ObjC enhancements */
  if (flag_next_runtime && !flag_zero_link)
    {
      tree *chain;
      tree decl;

      for (chain = &cls_ref_chain; *chain; chain = &TREE_CHAIN (*chain))
	if (TREE_VALUE (*chain) == ident)
	  {
	    if (! TREE_PURPOSE (*chain))
	      TREE_PURPOSE (*chain) = build_class_reference_decl ();

	    return TREE_PURPOSE (*chain);
	  }

      decl = build_class_reference_decl ();
      *chain = tree_cons (decl, ident, NULL_TREE);
      return decl;
    }
  else
    {
      tree params;

      add_class_reference (ident);

      params = build_tree_list (NULL_TREE,
				my_build_string (IDENTIFIER_LENGTH (ident) + 1,
						 IDENTIFIER_POINTER (ident)));

      assemble_external (objc_get_class_decl);
      return build_function_call (objc_get_class_decl, params);
    }
}

/* For each string section we have a chain which maps identifier nodes
   to decls for the strings.  */

static tree
add_objc_string (ident, section)
     tree ident;
     enum string_section section;
{
  tree *chain, decl;

  if (section == class_names)
    chain = &class_names_chain;
  else if (section == meth_var_names)
    chain = &meth_var_names_chain;
  else if (section == meth_var_types)
    chain = &meth_var_types_chain;
  else
    abort ();

  while (*chain)
    {
      if (TREE_VALUE (*chain) == ident)
	return build_unary_op (ADDR_EXPR, TREE_PURPOSE (*chain), 1);

      chain = &TREE_CHAIN (*chain);
    }

  decl = build_objc_string_decl (section);

  *chain = tree_cons (decl, ident, NULL_TREE);

  return build_unary_op (ADDR_EXPR, decl, 1);
}

static GTY(()) int class_names_idx;
static GTY(()) int meth_var_names_idx;
static GTY(()) int meth_var_types_idx;

static tree
build_objc_string_decl (section)
     enum string_section section;
{
  tree decl, ident;
  char buf[256];
  /* APPLE LOCAL indexing dpatel */
  flag_suppress_builtin_indexing = 1;

  if (section == class_names)
    sprintf (buf, "_OBJC_CLASS_NAME_%d", class_names_idx++);
  else if (section == meth_var_names)
    sprintf (buf, "_OBJC_METH_VAR_NAME_%d", meth_var_names_idx++);
  else if (section == meth_var_types)
    sprintf (buf, "_OBJC_METH_VAR_TYPE_%d", meth_var_types_idx++);

  ident = get_identifier (buf);

  decl = build_decl (VAR_DECL, ident, build_array_type (char_type_node, 0));
  DECL_EXTERNAL (decl) = 1;
  /* APPLE LOCAL Objective-C++ */
  /* TREE_PUBLIC (decl) = 1; */
  TREE_USED (decl) = 1;
  /* APPLE LOCAL Objective-C++ */
  /* don't set TREE_READONLY */
  TREE_CONSTANT (decl) = 1;
  DECL_CONTEXT (decl) = 0;
  DECL_ARTIFICIAL (decl) = 1;
 
  make_decl_rtl (decl, 0);
  pushdecl_top_level (decl);

  return decl;
}


void
objc_declare_alias (alias_ident, class_ident)
     tree alias_ident;
     tree class_ident;
{
  /* APPLE LOCAL type aliasing */
  tree underlying_class;

  /* APPLE LOCAL begin Objective-C++ */
#ifdef OBJCPLUS
  if (current_namespace != global_namespace) {
    error ("Objective-C declarations may only appear in global scope");
  }
#endif /* OBJCPLUS */
  /* APPLE LOCAL end Objective-C++ */
  
  /* APPLE LOCAL type aliasing */
  if (!(underlying_class = is_class_name (class_ident)))
    warning ("cannot find class `%s'", IDENTIFIER_POINTER (class_ident));
  else if (is_class_name (alias_ident))
    warning ("class `%s' already exists", IDENTIFIER_POINTER (alias_ident));
  else
    /* APPLE LOCAL type aliasing */
    alias_chain = tree_cons (underlying_class, alias_ident, alias_chain);
}

void
objc_declare_class (ident_list)
     tree ident_list;
{
  tree list;
  /* APPLE LOCAL Objective-C++ */
#ifdef OBJCPLUS
  if (current_namespace != global_namespace) {
    error ("Objective-C declarations may only appear in global scope");
  }
#endif /* OBJCPLUS */

  for (list = ident_list; list; list = TREE_CHAIN (list))
    {
      tree ident = TREE_VALUE (list);
      /* APPLE LOCAL begin Objective-C++ */

      if (! is_class_name (ident))
	{
	  tree record = lookup_name (ident);
	  
	  if (record && ! TREE_STATIC_TEMPLATE (record))
	    {
	      error ("`%s' redeclared as different kind of symbol",
		     IDENTIFIER_POINTER (ident));
	      error_with_decl (record, "previous declaration of `%s'");
	    }

	  record = xref_tag (RECORD_TYPE, ident);
      /* APPLE LOCAL end Objective-C++ */
	  TREE_STATIC_TEMPLATE (record) = 1;
	  class_chain = tree_cons (NULL_TREE, ident, class_chain);
	}
    }
}

tree
is_class_name (ident)
     tree ident;
{
  tree chain;

  /* APPLE LOCAL begin type aliasing */
  if (ident && TREE_CODE (ident) == IDENTIFIER_NODE
      && IDENTIFIER_GLOBAL_VALUE (ident))
    ident = IDENTIFIER_GLOBAL_VALUE (ident);
  while (ident && TREE_CODE (ident) == TYPE_DECL && DECL_RESULT (ident))    
    ident = TYPE_NAME (DECL_RESULT (ident));
  /* APPLE LOCAL end type aliasing */
      
  /* APPLE LOCAL begin Objective-C++ */
#ifdef OBJCPLUS
  if (ident && TREE_CODE (ident) == RECORD_TYPE)
    ident = TYPE_NAME (ident);
  if (ident && TREE_CODE (ident) == TYPE_DECL)
    ident = DECL_NAME (ident);
#endif
  /* APPLE LOCAL end Objective-C++ */
  /* APPLE LOCAL begin type aliasing */
  if (!ident || TREE_CODE (ident) != IDENTIFIER_NODE)
    return NULL_TREE; 
  /* APPLE LOCAL end type aliasing */
  
  if (lookup_interface (ident))
    return ident;

  for (chain = class_chain; chain; chain = TREE_CHAIN (chain))
    {
      if (ident == TREE_VALUE (chain))
	return ident;
    }

  for (chain = alias_chain; chain; chain = TREE_CHAIN (chain))
    {
      if (ident == TREE_VALUE (chain))
	return TREE_PURPOSE (chain);
    }

  /* APPLE LOCAL type aliasing */
  return NULL_TREE;
}

/* APPLE LOCAL begin Objective-C++ */
tree
objc_is_id (ident)
     tree ident;
{
  /* NB: This function may be called before the ObjC front-end
     has been initialized, in which case ID_TYPE will be NULL. */
  return (id_type && ident && TYPE_P (ident)
	  && (IS_ID (ident)
	      || TREE_TYPE (ident) == TREE_TYPE (objc_class_type)) 
	  ? ident 
	  : NULL_TREE);
}

tree
lookup_interface (ident)
     tree ident;
{
  /* APPLE LOCAL begin Objective-C++ */
#ifdef OBJCPLUS
  if (ident && TREE_CODE (ident) == TYPE_DECL)
    ident = DECL_NAME (ident);
#endif
  /* APPLE LOCAL end Objective-C++ */
  /* APPLE LOCAL begin objc speedup dpatel */
  return (ident && TREE_CODE (ident) == IDENTIFIER_NODE
	  ? IDENTIFIER_INTERFACE_VALUE (ident)
	  : NULL_TREE);
  /* APPLE LOCAL end objc speedup dpatel */
}

/* APPLE LOCAL begin Objective-C++ */
/* Implement @defs (<classname>) within struct bodies.  */
tree
get_class_ivars_from_name (class_name)
     tree class_name;
{
  tree interface = lookup_interface (class_name);
  tree field, fields = NULL_TREE;

  if (interface)
    {
      tree raw_ivar = get_class_ivars (interface, 1);

      /* Regenerate the FIELD_DECLs for the enclosing struct.  */
      for (; raw_ivar; raw_ivar = TREE_CHAIN (raw_ivar))
	{
	  field = grokfield (input_filename, lineno, 
			     TREE_PURPOSE (TREE_VALUE (raw_ivar)),
			     TREE_PURPOSE (raw_ivar),
			     TREE_VALUE (TREE_VALUE (raw_ivar)));
#ifdef OBJCPLUS
	  finish_member_declaration (field);
#else
	  fields = chainon (fields, field);
#endif
	}
    }      
  else
    error ("cannot find interface declaration for `%s'",
	   IDENTIFIER_POINTER (class_name));

  return fields;
}
/* APPLE LOCAL end Objective-C++ */

/* Used by: build_private_template, continue_class,
   and for @defs constructs.  */

/* APPLE LOCAL begin bitfields */
static tree
get_class_ivars (interface, raw)
     tree interface;
     int raw;
/* APPLE LOCAL end bitfields */
{
  tree my_name, super_name, ivar_chain;

  my_name = CLASS_NAME (interface);
  super_name = CLASS_SUPER_NAME (interface);
  /* APPLE LOCAL begin bitfields */
  if (raw)
    ivar_chain = CLASS_RAW_IVARS (interface);
  else
    {
      ivar_chain = CLASS_IVARS (interface);
      /* Save off a pristine copy of the leaf ivars (i.e, those not
	 inherited from a super class).  */
      if (!CLASS_OWN_IVARS (interface))
	 CLASS_OWN_IVARS (interface) = copy_list (ivar_chain);
    }
  /* APPLE LOCAL end bitfields */

  while (super_name)
    {
      tree op1;
      tree super_interface = lookup_interface (super_name);

      if (!super_interface)
        {
	  /* fatal did not work with 2 args...should fix */
	  error ("cannot find interface declaration for `%s', superclass of `%s'",
		 IDENTIFIER_POINTER (super_name),
		 IDENTIFIER_POINTER (my_name));
	  exit (FATAL_EXIT_CODE);
        }

      if (super_interface == interface)
	fatal_error ("circular inheritance in interface declaration for `%s'",
		     IDENTIFIER_POINTER (super_name));

      interface = super_interface;
      my_name = CLASS_NAME (interface);
      super_name = CLASS_SUPER_NAME (interface);

      /* APPLE LOCAL bitfields */
      op1 = (raw ? CLASS_RAW_IVARS (interface) : CLASS_OWN_IVARS (interface));
      if (op1)
        {
	  tree head = copy_list (op1);

	  /* Prepend super class ivars...make a copy of the list, we
	     do not want to alter the original.  */
	  chainon (head, ivar_chain);
	  ivar_chain = head;
        }
    }
  return ivar_chain;
}

/* APPLE LOCAL begin Panther ObjC enhancements */
static tree
objc_enter_block ()
{
  tree block;
  
#ifdef OBJCPLUS
  block = begin_compound_stmt (0);
#else
  block = c_begin_compound_stmt ();
  pushlevel (0);
  clear_last_expr ();
  add_scope_stmt (/*begin_p=*/1, /*partial_p=*/0);
#endif  
  
  objc_exception_block_stack 
    = tree_cons (NULL_TREE, block, objc_exception_block_stack);

  blk_nesting_count++;   
  return block;  
}

static tree
objc_exit_block ()
{
  tree block = TREE_VALUE (objc_exception_block_stack);
#ifndef OBJCPLUS
  tree scope_stmt, inner;
#endif
  
  objc_clear_super_receiver ();
#ifdef OBJCPLUS
  finish_compound_stmt (0, block);
#else  
  scope_stmt = add_scope_stmt (/*begin_p=*/0, /*partial_p=*/0);
  inner = poplevel (kept_level_p (), 1, 0); 
  
  SCOPE_STMT_BLOCK (TREE_PURPOSE (scope_stmt)) 
	= SCOPE_STMT_BLOCK (TREE_VALUE (scope_stmt))
	= inner;
  RECHAIN_STMTS (block, COMPOUND_BODY (block));
#endif  
  last_expr_type = NULL_TREE;
  objc_exception_block_stack = TREE_CHAIN (objc_exception_block_stack);	
  
  blk_nesting_count--;
  return block;
}
  
static tree
objc_declare_variable (scspec, name, type, init)
     enum rid scspec;
     tree name, type, init;
{
  tree decl;
  
  flag_suppress_builtin_indexing = 1;
  
  type = tree_cons (NULL_TREE, type, 
		    tree_cons (NULL_TREE, ridpointers[(int) scspec], NULL_TREE));
  TREE_STATIC (type) = 1;
  decl = start_decl (name, type, (init != NULL_TREE), NULL_TREE);
  finish_decl (decl, init, NULL_TREE);
  /* This prevents `unused variable' warnings when compiling with -Wall.  */
  TREE_USED (decl) = 1;
  DECL_ARTIFICIAL (decl) = 1;
  flag_suppress_builtin_indexing = 0;
  return decl;
}

tree 
objc_build_throw_stmt (throw_expr)
     tree throw_expr;
{
  tree func_params;
  
  if (!flag_objc_exceptions)
    fatal_error ("Use `-fobjc-exceptions' to enable Objective-C exception syntax");
    
  if (!throw_expr && objc_caught_exception)
    throw_expr = TREE_VALUE (objc_caught_exception);
    
  if (!throw_expr)
    {
      error ("`@throw;' (rethrow) used outside of a `@catch' block");
      return error_mark_node;
    }
        
  func_params = tree_cons (NULL_TREE, throw_expr, NULL_TREE);

  assemble_external (objc_exception_throw_decl);
  return c_expand_expr_stmt (build_function_call
	(objc_exception_throw_decl, func_params)); 
}

static void
val_stack_push (nc, val)
     struct val_stack **nc;
     long val;
{
  struct val_stack *new_elem = xmalloc (sizeof (struct val_stack));
  new_elem->val = val;
  new_elem->next = *nc;
  *nc = new_elem;
}

static void
val_stack_pop (nc)
     struct val_stack **nc;
{     
  struct val_stack *old_elem = *nc;
  *nc = old_elem->next;
  free (old_elem);  
}

static void
objc_build_try_enter_fragment ()
{
	/*	objc_exception_try_enter(&_stackExceptionData);
		if (!_setjmp(&_stackExceptionData.buf)) { */

  tree func_params, if_stmt, cond;
  
  func_params = tree_cons (NULL_TREE, 
	build_unary_op (ADDR_EXPR, TREE_VALUE (objc_stack_exception_data), 0),  
	NULL_TREE);
  assemble_external (objc_exception_try_enter_decl);
  c_expand_expr_stmt (build_function_call 
	(objc_exception_try_enter_decl, func_params));
  
  if_stmt = c_begin_if_stmt ();
  if_nesting_count++;
  /* If <setjmp.h> has been included, the _setjmp prototype has acquired a
     real, breathing type for its parameter.  Cast our argument to that type.  */
  func_params = tree_cons (NULL_TREE, 
	build_c_cast (TYPE_ARG_TYPES (TREE_TYPE (objc_setjmp_decl))
		      ? TREE_VALUE (TYPE_ARG_TYPES (TREE_TYPE (objc_setjmp_decl)))
		      : ptr_type_node, 
		build_unary_op (ADDR_EXPR, 
		  build_component_ref (TREE_VALUE (objc_stack_exception_data), 
		get_identifier ("buf")), 0)),  
	NULL_TREE); 
  assemble_external (objc_setjmp_decl);
  cond = build_unary_op (TRUTH_NOT_EXPR, 
	build_function_call (objc_setjmp_decl, func_params), 0);
  c_expand_start_cond (c_common_truthvalue_conversion (cond), 
				       0, if_stmt);
  objc_enter_block ();
}

static tree
objc_build_extract_expr ()
{
	/*	... = objc_exception_extract(&_stackExceptionData);  */

  tree func_params = tree_cons (NULL_TREE, 
	build_unary_op (ADDR_EXPR, TREE_VALUE (objc_stack_exception_data), 0),  
	NULL_TREE);

  assemble_external (objc_exception_extract_decl);
  return build_function_call (objc_exception_extract_decl, func_params);
}

static void
objc_build_try_exit_fragment ()
{
	/* 	objc_exception_try_exit(&_stackExceptionData); */

    tree func_params = tree_cons (NULL_TREE, 
	build_unary_op (ADDR_EXPR, TREE_VALUE (objc_stack_exception_data), 0),  
	NULL_TREE);

    assemble_external (objc_exception_try_exit_decl);
    c_expand_expr_stmt (build_function_call 
	(objc_exception_try_exit_decl, func_params));
}

static void
objc_build_extract_fragment ()
{
	/*
             }
             else { 
                _rethrowException = objc_exception_extract(&_stackExceptionData);
             } */

    objc_exit_block ();
    c_finish_then ();
    
    c_expand_start_else ();
    objc_enter_block ();
    c_expand_expr_stmt
	(build_modify_expr 
	    (TREE_VALUE (objc_rethrow_exception),
	     NOP_EXPR,
	     objc_build_extract_expr ()));
    objc_exit_block ();
    c_finish_else ();
    c_expand_end_cond ();
    if_nesting_count--;
}

tree 
objc_build_try_prologue ()
{

    /*	{ // new scope
	   struct _objc_exception_data _stackExceptionData;
	   volatile id _rethrowException = nil;
	   { // begin TRY-CATCH scope
		objc_exception_try_enter(&_stackExceptionData);
		if (!_setjmp(&_stackExceptionData.buf)) { */

  tree try_catch_block;

  if (!flag_objc_exceptions)
    fatal_error ("Use `-fobjc-exceptions' to enable Objective-C exception syntax");
    
  objc_mark_locals_volatile ((void *)(exc_binding_stack 
				      ? exc_binding_stack->val 
				      : 0));
  objc_enter_block ();
  objc_stack_exception_data = tree_cons (NULL_TREE,
	objc_declare_variable (RID_AUTO, get_identifier (UTAG_EXCDATA_VAR), 
		xref_tag (RECORD_TYPE, get_identifier (UTAG_EXCDATA)),
		NULL_TREE),
	objc_stack_exception_data);
  objc_rethrow_exception = tree_cons (NULL_TREE,
	objc_declare_variable (RID_VOLATILE, get_identifier (UTAG_RETHROWEXC_VAR), 
		id_type,
		build_int_2 (0, 0)),
	objc_rethrow_exception);

  try_catch_block = objc_enter_block ();
  val_stack_push (&exc_binding_stack, 
		  (long)get_current_binding_level ());
  objc_build_try_enter_fragment (); 
  
  return try_catch_block;
}

void 
objc_build_try_epilogue (also_catch_prologue)
     int also_catch_prologue;
{

  if (also_catch_prologue) {

    /* 
        }
       	else {
		register id _caughtException = objc_exception_extract( &_stackExceptionData);
		objc_exception_try_enter(&_stackExceptionData);
                if(!_setjmp(&_stackExceptionData.buf)) {
		   if (0) {
        */

    tree if_stmt; 
    
    objc_exit_block ();
    c_finish_then ();
    		
    c_expand_start_else ();
    objc_enter_block ();
    objc_caught_exception = tree_cons (NULL_TREE, 
	objc_declare_variable (RID_REGISTER, get_identifier (UTAG_CAUGHTEXC_VAR), 
		id_type, objc_build_extract_expr ()),
	objc_caught_exception);
    objc_build_try_enter_fragment ();	
    val_stack_push (&catch_count_stack, 1);
    if_stmt = c_begin_if_stmt ();
    if_nesting_count++;
    c_expand_start_cond (c_common_truthvalue_conversion (boolean_false_node), 
				       0, if_stmt);
    objc_enter_block ();
    
    /* Start a new chain of @catch statements for this @try.  */
    objc_catch_type = tree_cons (objc_catch_type, NULL_TREE, NULL_TREE);
  }
  else {  /* !also_catch_prologue */
  
    /*	
	  }
          else {
		_rethrowException = objc_exception_extract( &_stackExceptionData);
	  }
       }
    */
    objc_build_extract_fragment ();
    objc_exit_block ();
  }
}

void 
objc_build_catch_stmt (catch_expr)
     tree catch_expr;
{
      /* 	} else if (objc_exception_match(objc_get_class("SomeClass"), _caughtException)) {
			register SomeClass *e = _caughtException;
      */
      
  tree if_stmt, cond, func_params, prev_catch, var_name, var_type;
  int catch_id;

#ifndef OBJCPLUS
  /* Yet another C/C++ impedance mismatch.  */
  catch_expr = TREE_PURPOSE (catch_expr);
#endif

  var_name = TREE_VALUE (catch_expr);
  var_type = TREE_VALUE (TREE_PURPOSE (catch_expr));
  if (TREE_CODE (var_name) == INDIRECT_REF)
    var_name = TREE_OPERAND (var_name, 0);
  if (TREE_CODE (var_type) == TYPE_DECL || TREE_CODE (var_type) == POINTER_TYPE)
    var_type = TREE_TYPE (var_type);
  catch_id = (var_type == TREE_TYPE (id_type));
        
  if (!flag_objc_exceptions)
    fatal_error ("Use `-fobjc-exceptions' to enable Objective-C exception syntax");
  
  if (!(catch_id || TYPED_OBJECT (var_type)))
    fatal_error ("`@catch' parameter is not a known Objective-C class type");
    
  /* Examine previous @catch clauses for the current @try block for
     superclasses of the 'var_type' class.  */
  for (prev_catch = objc_catch_type; TREE_VALUE (prev_catch); 
       prev_catch = TREE_CHAIN (prev_catch))   
    {
      if (TREE_VALUE (prev_catch) == TREE_TYPE (id_type))
	{
	  warning ("Exception already handled by preceding `@catch(id)'");
	  break;
	}
      else if (!catch_id
	       && objc_comptypes (TREE_VALUE (prev_catch), var_type, 0) == 1)
	warning ("Exception of type `%s *' already handled by `@catch (%s *)'",
		 IDENTIFIER_POINTER (OBJC_TYPE_NAME (var_type)),
		 IDENTIFIER_POINTER (OBJC_TYPE_NAME (TREE_VALUE (prev_catch))));
    }
    
  objc_catch_type = tree_cons (NULL_TREE, var_type, objc_catch_type);
  
  objc_exit_block ();
  c_finish_then ();
  
  c_expand_start_else ();
  catch_count_stack->val++;
  if_stmt = c_begin_if_stmt ();
  if_nesting_count++;

  if (catch_id) 
    cond = integer_one_node;
  else
    {  
      cond = get_class_reference (OBJC_TYPE_NAME (var_type));
	
      func_params = tree_cons (NULL_TREE, cond, 
	tree_cons (NULL_TREE,
		TREE_VALUE (objc_caught_exception), 
		NULL_TREE)); 
      assemble_external (objc_exception_match_decl);
      cond = build_function_call (objc_exception_match_decl, func_params);
    }
    
  c_expand_start_cond (c_common_truthvalue_conversion (cond), 
				       0, if_stmt);
  objc_enter_block ();
  objc_declare_variable (RID_REGISTER, var_name, 
		build_pointer_type (var_type), 
		TREE_VALUE (objc_caught_exception));
}

void 
objc_build_catch_epilogue ()
{
  /* 
			   }
                           else {
                                   _rethrowException = _caughtException;
				   objc_exception_try_exit(&_stackExceptionData);
                           }
                   }
                   else { 
                           _rethrowException = objc_exception_extract(&_stackExceptionData);
                   }
              } 
        } // end TRY-CATCH scope */

  objc_exit_block ();
  c_finish_then ();
  
  c_expand_start_else ();
  objc_enter_block ();
  c_expand_expr_stmt
	(build_modify_expr 
	    (TREE_VALUE (objc_rethrow_exception),
	     NOP_EXPR,
	     TREE_VALUE (objc_caught_exception)));
  objc_build_try_exit_fragment ();	     
  objc_exit_block ();
  while (catch_count_stack->val--) {
    c_finish_else ();		/* close off all the nested ifs ! */
    c_expand_end_cond ();
    if_nesting_count--;
  }
  val_stack_pop (&catch_count_stack);
  objc_caught_exception = TREE_CHAIN (objc_caught_exception);
    
  objc_build_extract_fragment ();
  
  objc_exit_block ();
  c_finish_else ();
  c_expand_end_cond ();
  if_nesting_count--;
  objc_exit_block ();
  
  /* Return to enclosing chain of @catch statements (if any).  */
  while (TREE_VALUE (objc_catch_type))
    objc_catch_type = TREE_CHAIN (objc_catch_type);
  objc_catch_type = TREE_PURPOSE (objc_catch_type);
}

tree 
objc_build_finally_prologue ()
{
	/* { // begin FINALLY scope 
                  if(!_rethrowException) { 
                        objc_exception_try_exit(&_stackExceptionData);
                  } */
	
  tree blk = objc_enter_block ();
  
  tree if_stmt = c_begin_if_stmt ();
  if_nesting_count++;
  
  c_expand_start_cond (c_common_truthvalue_conversion 
		       (build_unary_op 
		        (TRUTH_NOT_EXPR, TREE_VALUE (objc_rethrow_exception), 0)), 
			0, if_stmt);
  objc_enter_block ();
  objc_build_try_exit_fragment ();
  objc_exit_block ();
  c_finish_then ();
  c_expand_end_cond ();
  if_nesting_count--;
  
  return blk;
}

tree 
objc_build_finally_epilogue ()
{

       /*         if(_rethrowException) { 
                        objc_exception_throw(_rethrowException);
                  }
             } // end FINALLY scope
	} */

  tree if_stmt = c_begin_if_stmt ();
  if_nesting_count++;
  
  c_expand_start_cond (c_common_truthvalue_conversion (TREE_VALUE (objc_rethrow_exception)), 
				       0, if_stmt);
  objc_enter_block ();
  objc_build_throw_stmt (TREE_VALUE (objc_rethrow_exception));
  objc_exit_block ();
  c_finish_then ();
  c_expand_end_cond ();
  if_nesting_count--;

  objc_exit_block ();
  objc_rethrow_exception = TREE_CHAIN (objc_rethrow_exception);
  objc_stack_exception_data = TREE_CHAIN (objc_stack_exception_data);
  
  val_stack_pop (&exc_binding_stack);
  return objc_exit_block ();
}

tree 
objc_build_try_catch_finally_stmt (has_catch, has_finally)
     int has_catch, has_finally;
{
  /* NB: The operative assumption here is that TRY_FINALLY_EXPR will
     deal with all exits from 'try_catch_blk' and route them through
     'finally_blk'.  */
  tree outer_blk = objc_build_finally_epilogue ();
  tree prec_stmt = TREE_CHAIN (TREE_CHAIN (COMPOUND_BODY (outer_blk)));  
  tree try_catch_blk = TREE_CHAIN (prec_stmt), try_catch_expr;
  tree finally_blk = TREE_CHAIN (try_catch_blk), finally_expr;
  tree succ_stmt = TREE_CHAIN (finally_blk);
  tree try_finally_stmt, try_finally_expr;
  
  if (!flag_objc_exceptions)
    fatal_error ("Use `-fobjc-exceptions' to enable Objective-C exception syntax");
    
  /* It is an error to have a @try block without a @catch and/or @finally
     (even though sensible code can be generated nonetheless).  */

  if (!has_catch && !has_finally)
    error ("`@try' without `@catch' or `@finally'");
       
  /* We shall now do something truly disgusting.  We shall remove the 
     'try_catch_blk' and 'finally_blk' from the 'outer_blk' statement
     chain, and replace them with a TRY_FINALLY_EXPR statement!  If this
     doesn't work, we will have to learn (from Per/gcj) how to construct
     the 'outer_blk' lazily.  */

  TREE_CHAIN (try_catch_blk) = TREE_CHAIN (finally_blk) = NULL_TREE;
  try_catch_expr = build1 (STMT_EXPR, void_type_node, try_catch_blk); 
  TREE_SIDE_EFFECTS (try_catch_expr) = 1;
  finally_expr = build1 (STMT_EXPR, void_type_node, finally_blk); 
  TREE_SIDE_EFFECTS (finally_expr) = 1;
  try_finally_expr = build (TRY_FINALLY_EXPR, void_type_node, try_catch_expr, 
			    finally_expr);
  TREE_SIDE_EFFECTS (try_finally_expr) = 1;
  try_finally_stmt = build_stmt (EXPR_STMT, try_finally_expr);
  TREE_CHAIN (prec_stmt) = try_finally_stmt;
  TREE_CHAIN (try_finally_stmt) = succ_stmt;
  	
  return outer_blk;  /* the whole enchilada */
}

void 
objc_build_synchronized_prologue (sync_expr)
     tree sync_expr;
{
  /* 	{
		id _eval_once = <sync_expr>;
		@try {
			objc_sync_enter( _eval_once );
  */
  
  tree func_params;
  
  if (!flag_objc_exceptions)
    fatal_error ("Use `-fobjc-exceptions' to enable Objective-C exception syntax");
    
  objc_enter_block ();
  objc_eval_once = tree_cons (NULL_TREE,
	objc_declare_variable (RID_AUTO, get_identifier (UTAG_EVALONCE_VAR), 
		id_type,
		sync_expr),
	objc_eval_once);
  objc_build_try_prologue ();
  objc_enter_block ();	
  func_params = tree_cons (NULL_TREE, TREE_VALUE (objc_eval_once),  
			   NULL_TREE);

  assemble_external (objc_sync_enter_decl);
  c_expand_expr_stmt (build_function_call 
	(objc_sync_enter_decl, func_params));
}
     
tree 
objc_build_synchronized_epilogue ()
{
  /* 		}
		@finally {
			objc_sync_exit( _eval_once );
		}
	}
  */
  
  tree func_params;

  objc_exit_block ();	
  objc_build_try_epilogue (0);
  objc_build_finally_prologue ();
  func_params = tree_cons (NULL_TREE, TREE_VALUE (objc_eval_once),  
			   NULL_TREE);

  assemble_external (objc_sync_exit_decl);
  c_expand_expr_stmt (build_function_call 
	(objc_sync_exit_decl, func_params));
  objc_build_try_catch_finally_stmt (0, 1);
  
  return objc_exit_block ();
}
     
/* Predefine the following data type:

   struct _objc_exception_data
   {
     int buf[_JBLEN];
     void *pointers[4];
   }; */

/* The following yuckiness should prevent users from having to #include 
   <setjmp.h> in their code... */
   
#ifdef TARGET_POWERPC
/* snarfed from /usr/include/ppc/setjmp.h */
#define _JBLEN (26 + 36 + 129 + 1)
#else
/* snarfed from /usr/include/i386/{setjmp,signal}.h */
#define _JBLEN 18
#endif
  
static void
build_objc_exception_stuff ()
{
  tree field_decl, field_decl_chain, index, temp_type;

  /* Suppress outputting debug symbols, because
     dbxout_init hasn't been called yet.  */
  enum debug_info_type save_write_symbols = write_symbols;
  const struct gcc_debug_hooks *save_hooks = debug_hooks;
  
  write_symbols = NO_DEBUG;
  debug_hooks = &do_nothing_debug_hooks;
  objc_exception_data_template
    = start_struct (RECORD_TYPE, get_identifier (UTAG_EXCDATA));

  /* int buf[_JBLEN]; */

  index = build_index_type (build_int_2 (_JBLEN - 1, 0));
  field_decl = create_builtin_decl (FIELD_DECL,
			build_array_type (integer_type_node, index),
			"buf");
  field_decl_chain = field_decl;

  /* void *pointers[4]; */

  index = build_index_type (build_int_2 (4 - 1, 0));
  field_decl = create_builtin_decl (FIELD_DECL,
			build_array_type (ptr_type_node, index),
			"pointers");
  chainon (field_decl_chain, field_decl);

  finish_struct (objc_exception_data_template, field_decl_chain, NULL_TREE);

  /* int _setjmp(...); */
  /* If the user includes <setjmp.h>, this shall be superceded by
     'int _setjmp(jmp_buf);' */
  temp_type = build_function_type (integer_type_node, NULL_TREE);
  objc_setjmp_decl
	= builtin_function (TAG_SETJMP, temp_type, 0, NOT_BUILT_IN, NULL, NULL_TREE);

  /* id objc_exception_extract(struct _objc_exception_data *); */
  temp_type = build_function_type (id_type,
		tree_cons (NULL_TREE, 
		    build_pointer_type (objc_exception_data_template), 
			OBJC_VOID_AT_END));
  objc_exception_extract_decl
	= builtin_function (TAG_EXCEPTIONEXTRACT, temp_type, 0, NOT_BUILT_IN, NULL, NULL_TREE);
  /* void objc_exception_try_enter(struct _objc_exception_data *); */
  /* void objc_exception_try_exit(struct _objc_exception_data *); */
  temp_type = build_function_type (void_type_node,
		tree_cons (NULL_TREE, 
		    build_pointer_type (objc_exception_data_template), 
			OBJC_VOID_AT_END));
  objc_exception_try_enter_decl
	= builtin_function (TAG_EXCEPTIONTRYENTER, temp_type, 0, NOT_BUILT_IN, NULL, NULL_TREE);
  objc_exception_try_exit_decl
	= builtin_function (TAG_EXCEPTIONTRYEXIT, temp_type, 0, NOT_BUILT_IN, NULL, NULL_TREE);
  /* void objc_exception_throw(id) __attribute__((noreturn)); */
  /* void objc_sync_enter(id); */
  /* void objc_sync_exit(id); */
  temp_type = build_function_type (void_type_node,
		tree_cons (NULL_TREE, id_type,
			OBJC_VOID_AT_END));
  objc_exception_throw_decl
	= builtin_function (TAG_EXCEPTIONTHROW, temp_type, 0, NOT_BUILT_IN, NULL, NULL_TREE);
  TYPE_VOLATILE (objc_exception_throw_decl) = 1;
  DECL_ATTRIBUTES (objc_exception_throw_decl) 
	= tree_cons (get_identifier ("noreturn"), NULL_TREE, NULL_TREE);
  objc_sync_enter_decl
	= builtin_function (TAG_SYNCENTER, temp_type, 0, NOT_BUILT_IN, NULL, NULL_TREE);
  objc_sync_exit_decl
	= builtin_function (TAG_SYNCEXIT, temp_type, 0, NOT_BUILT_IN, NULL, NULL_TREE);
  /* int objc_exception_match(id, id); */
  temp_type = build_function_type (integer_type_node,
		tree_cons (NULL_TREE, id_type, 
		    tree_cons (NULL_TREE, id_type,
			OBJC_VOID_AT_END)));
  objc_exception_match_decl
	= builtin_function (TAG_EXCEPTIONMATCH, temp_type, 0, NOT_BUILT_IN, NULL, NULL_TREE);
	
  write_symbols = save_write_symbols;
  debug_hooks = save_hooks;
}
/* APPLE LOCAL end Panther ObjC enhancements */
            
/* struct <classname> {
     struct objc_class *isa;
     ...
   };  */

static tree
build_private_template (class)
     tree class;
{
  tree ivar_context;

  if (CLASS_STATIC_TEMPLATE (class))
    {
      uprivate_record = CLASS_STATIC_TEMPLATE (class);
      ivar_context = TYPE_FIELDS (CLASS_STATIC_TEMPLATE (class));
    }
  else
    {
      uprivate_record = start_struct (RECORD_TYPE, CLASS_NAME (class));
      /* APPLE LOCAL bitfields */
      ivar_context = get_class_ivars (class, 0);

      finish_struct (uprivate_record, ivar_context, NULL_TREE);

      CLASS_STATIC_TEMPLATE (class) = uprivate_record;

      /* mark this record as class template - for class type checking */
      TREE_STATIC_TEMPLATE (uprivate_record) = 1;
    }

  instance_type
    = groktypename (build_tree_list (build_tree_list (NULL_TREE,
						      uprivate_record),
				     build1 (INDIRECT_REF, NULL_TREE,
					     NULL_TREE)));

  return ivar_context;
}

/* Begin code generation for protocols...  */

/* struct objc_protocol {
     char *protocol_name;
     struct objc_protocol **protocol_list;
     struct objc_method_desc *instance_methods;
     struct objc_method_desc *class_methods;
   };  */

static tree
build_protocol_template ()
{
  tree decl_specs, field_decl, field_decl_chain;
  tree template;

  template = start_struct (RECORD_TYPE, get_identifier (UTAG_PROTOCOL));

  /* struct objc_class *isa; */

  decl_specs = build_tree_list (NULL_TREE, xref_tag (RECORD_TYPE,
					get_identifier (UTAG_CLASS)));
  field_decl = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("isa"));
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  field_decl_chain = field_decl;

  /* char *protocol_name; */

  decl_specs = build_tree_list (NULL_TREE, ridpointers[(int) RID_CHAR]);
  field_decl
    = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("protocol_name"));
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  /* struct objc_protocol **protocol_list; */

  decl_specs = build_tree_list (NULL_TREE, template);
  field_decl
    = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("protocol_list"));
  field_decl = build1 (INDIRECT_REF, NULL_TREE, field_decl);
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  /* struct objc_method_list *instance_methods; */

  decl_specs
    = build_tree_list (NULL_TREE,
		       xref_tag (RECORD_TYPE,
				 get_identifier (UTAG_METHOD_PROTOTYPE_LIST)));
  field_decl
    = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("instance_methods"));
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  /* struct objc_method_list *class_methods; */

  decl_specs
    = build_tree_list (NULL_TREE,
		       xref_tag (RECORD_TYPE,
				 get_identifier (UTAG_METHOD_PROTOTYPE_LIST)));
  field_decl
    = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("class_methods"));
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  return finish_struct (template, field_decl_chain, NULL_TREE);
}

static tree
build_descriptor_table_initializer (type, entries)
     tree type;
     tree entries;
{
  tree initlist = NULL_TREE;

  do
    {
      tree eltlist = NULL_TREE;

      eltlist
	= tree_cons (NULL_TREE,
		     build_selector (METHOD_SEL_NAME (entries)), NULL_TREE);
      eltlist
	= tree_cons (NULL_TREE,
		     add_objc_string (METHOD_ENCODING (entries),
				      meth_var_types),
		     eltlist);

      initlist
	= tree_cons (NULL_TREE,
		     build_constructor (type, nreverse (eltlist)), initlist);

      entries = TREE_CHAIN (entries);
    }
  while (entries);

  return build_constructor (build_array_type (type, 0), nreverse (initlist));
}

/* struct objc_method_prototype_list {
     int count;
     struct objc_method_prototype {
 	SEL name;
 	char *types;
     } list[1];
   };  */

static tree
build_method_prototype_list_template (list_type, size)
     tree list_type;
     int size;
{
  tree objc_ivar_list_record;
  tree decl_specs, field_decl, field_decl_chain;

  /* Generate an unnamed struct definition.  */

  objc_ivar_list_record = start_struct (RECORD_TYPE, NULL_TREE);

  /* int method_count; */

  decl_specs = build_tree_list (NULL_TREE, ridpointers[(int) RID_INT]);
  field_decl = get_identifier ("method_count");

  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  field_decl_chain = field_decl;

  /* struct objc_method method_list[]; */

  decl_specs = build_tree_list (NULL_TREE, list_type);
  field_decl = build_nt (ARRAY_REF, get_identifier ("method_list"),
			 build_int_2 (size, 0));

  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  finish_struct (objc_ivar_list_record, field_decl_chain, NULL_TREE);

  return objc_ivar_list_record;
}

static tree
build_method_prototype_template ()
{
  tree proto_record;
  tree decl_specs, field_decl, field_decl_chain;

  proto_record
    = start_struct (RECORD_TYPE, get_identifier (UTAG_METHOD_PROTOTYPE));

  /* struct objc_selector *_cmd; */
  decl_specs = tree_cons (NULL_TREE, xref_tag (RECORD_TYPE,
		          get_identifier (TAG_SELECTOR)), NULL_TREE);
  field_decl = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("_cmd"));

  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  field_decl_chain = field_decl;

  decl_specs = tree_cons (NULL_TREE, ridpointers[(int) RID_CHAR], NULL_TREE);
  field_decl
    = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("method_types"));
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  finish_struct (proto_record, field_decl_chain, NULL_TREE);

  return proto_record;
}

/* APPLE LOCAL begin Panther ObjC enhancements */
/* 'forwarding_offset' is no more. */

static tree 
objc_method_parm_type (type)
      tree type;
{
  type = groktypename (TREE_TYPE (type));
  if (TREE_CODE (type) == TYPE_DECL)
    type = TREE_TYPE (type);
  return TYPE_MAIN_VARIANT (type);
}

static int
objc_encoded_type_size (type)
      tree type;
{
  int sz = int_size_in_bytes (type);

  /* Make all integer and enum types at least as large
     as an int.  */
  if (sz > 0 && (TREE_CODE (type) == INTEGER_TYPE
	  	 || TREE_CODE (type) == ENUMERAL_TYPE))
    sz = MAX (sz, int_size_in_bytes (integer_type_node));
  /* Treat arrays as pointers, since that's how they're
     passed in.  */
  else if (TREE_CODE (type) == ARRAY_TYPE)
    sz = int_size_in_bytes (ptr_type_node);     
  return sz;
}
      
static tree
encode_method_prototype (method_decl)
      tree method_decl;
/* APPLE LOCAL end Panther ObjC enhancements */
{
  tree parms;
  /* APPLE LOCAL Panther ObjC enhancements */
  int parm_offset, i;
  /* APPLE LOCAL Panther ObjC enhancements */
  char buf[40];
  tree result;

  /* ONEWAY and BYCOPY, for remote object are the only method qualifiers.  */
  encode_type_qualifiers (TREE_PURPOSE (TREE_TYPE (method_decl)));

  /* APPLE LOCAL begin Panther ObjC enhancements */
  /* Encode return type.  */
  encode_type (objc_method_parm_type (method_decl),
	       obstack_object_size (&util_obstack),
	       OBJC_ENCODE_INLINE_DEFS);

  /* Stack size.  */
  /* The first two arguments (self and _cmd) are pointers; account for
     their size.  */
  i = int_size_in_bytes (ptr_type_node);
  parm_offset = 2 * i;
  for (parms = METHOD_SEL_ARGS (method_decl); parms;
       parms = TREE_CHAIN (parms))
    {
      tree type = objc_method_parm_type (parms);
      int sz = objc_encoded_type_size (type);

      /* If a type size is not known, bail out.  */
      if (sz < 0)
	{
	  error_with_decl (type, "type `%s' does not have a known size");
	  /* Pretend that the encoding succeeded; the compilation will
	     fail nevertheless.  */
	  goto finish_encoding;
	}
      parm_offset += sz;
    }

  sprintf (buf, "%d@0:%d", parm_offset, i);
  obstack_grow (&util_obstack, buf, strlen (buf));

  /* Argument types.  */
  parm_offset = 2 * i;
  for (parms = METHOD_SEL_ARGS (method_decl); parms;
       parms = TREE_CHAIN (parms))
    {
      tree type = objc_method_parm_type (parms);

      /* Process argument qualifiers for user supplied arguments.  */
      encode_type_qualifiers (TREE_PURPOSE (TREE_TYPE (parms)));
 
      /* Type.  */
      encode_type (type, obstack_object_size (&util_obstack),
		   OBJC_ENCODE_INLINE_DEFS);

      /* Compute offset.  */
      sprintf (buf, "%d", parm_offset);
      parm_offset += objc_encoded_type_size (type);
      /* APPLE LOCAL end Panther ObjC enhancements */

      obstack_grow (&util_obstack, buf, strlen (buf));
    }

  finish_encoding:
  obstack_1grow (&util_obstack, '\0');
  result = get_identifier (obstack_finish (&util_obstack));
  obstack_free (&util_obstack, util_firstobj);
  return result;
}

static tree
generate_descriptor_table (type, name, size, list, proto)
     tree type;
     const char *name;
     int size;
     tree list;
     tree proto;
{
  tree sc_spec, decl_specs, decl, initlist;

  sc_spec = tree_cons (NULL_TREE, ridpointers[(int) RID_STATIC], NULL_TREE);
  decl_specs = tree_cons (NULL_TREE, type, sc_spec);

  decl = start_decl (synth_id_with_class_suffix (name, proto),
		     decl_specs, 1, NULL_TREE);
  DECL_CONTEXT (decl) = NULL_TREE;

  initlist = build_tree_list (NULL_TREE, build_int_2 (size, 0));
  initlist = tree_cons (NULL_TREE, list, initlist);

  finish_decl (decl, build_constructor (type, nreverse (initlist)),
	       NULL_TREE);

  return decl;
}

/* APPLE LOCAL begin callgraph inlining */
static void
mark_referenced_methods ()
{
  struct imp_entry *impent;
  tree chain;

  for (impent = imp_list; impent; impent = impent->next)
    {
      chain = CLASS_CLS_METHODS (impent->imp_context);
      while (chain)
	{
	  cgraph_mark_needed_node (cgraph_node (METHOD_DEFINITION (chain)), 1);
	  chain = TREE_CHAIN (chain);
	}
      chain = CLASS_NST_METHODS (impent->imp_context);
      while (chain)
	{
	  cgraph_mark_needed_node (cgraph_node (METHOD_DEFINITION (chain)), 1);
	  chain = TREE_CHAIN (chain);
	}
    }
}
/* APPLE LOCAL end callgraph inlining */

static void
generate_method_descriptors (protocol)
     tree protocol;
{
  tree initlist, chain, method_list_template;
  tree cast, variable_length_type;
  int size;

  if (!objc_method_prototype_template)
    objc_method_prototype_template = build_method_prototype_template ();

  cast = build_tree_list (build_tree_list (NULL_TREE, xref_tag (RECORD_TYPE,
				get_identifier (UTAG_METHOD_PROTOTYPE_LIST))),
			  NULL_TREE);
  variable_length_type = groktypename (cast);

  chain = PROTOCOL_CLS_METHODS (protocol);
  if (chain)
    {
      size = list_length (chain);

      method_list_template
	= build_method_prototype_list_template (objc_method_prototype_template,
						size);

      initlist 
	= build_descriptor_table_initializer (objc_method_prototype_template,
					      chain);

      UOBJC_CLASS_METHODS_decl
	= generate_descriptor_table (method_list_template,
				     "_OBJC_PROTOCOL_CLASS_METHODS",
				     size, initlist, protocol);
      TREE_TYPE (UOBJC_CLASS_METHODS_decl) = variable_length_type;
    }
  else
    UOBJC_CLASS_METHODS_decl = 0;

  chain = PROTOCOL_NST_METHODS (protocol);
  if (chain)
    {
      size = list_length (chain);

      method_list_template
	= build_method_prototype_list_template (objc_method_prototype_template,
						size);
      initlist
	= build_descriptor_table_initializer (objc_method_prototype_template,
					      chain);

      UOBJC_INSTANCE_METHODS_decl
	= generate_descriptor_table (method_list_template,
				     "_OBJC_PROTOCOL_INSTANCE_METHODS",
				     size, initlist, protocol);
      TREE_TYPE (UOBJC_INSTANCE_METHODS_decl) = variable_length_type;
    }
  else
    UOBJC_INSTANCE_METHODS_decl = 0;
}


/* APPLE LOCAL Panther ObjC enhancements */
/* 'build_tmp_function_decl' and 'hack_method_prototype' have been removed */

static void
generate_protocol_references (plist)
     tree plist;
{
  tree lproto;

  /* Forward declare protocols referenced.  */
  for (lproto = plist; lproto; lproto = TREE_CHAIN (lproto))
    {
      tree proto = TREE_VALUE (lproto);

      if (TREE_CODE (proto) == PROTOCOL_INTERFACE_TYPE
	  && PROTOCOL_NAME (proto))
	{
          if (! PROTOCOL_FORWARD_DECL (proto))
            build_protocol_reference (proto);

          if (PROTOCOL_LIST (proto))
            generate_protocol_references (PROTOCOL_LIST (proto));
        }
    }
}

/* For each protocol which was referenced either from a @protocol()
   expression, or because a class/category implements it (then a
   pointer to the protocol is stored in the struct describing the
   class/category), we create a statically allocated instance of the
   Protocol class.  The code is written in such a way as to generate
   as few Protocol objects as possible; we generate a unique Protocol
   instance for each protocol, and we don't generate a Protocol
   instance if the protocol is never referenced (either from a
   @protocol() or from a class/category implementation).  These
   statically allocated objects can be referred to via the static
   (that is, private to this module) symbols _OBJC_PROTOCOL_n.
   
   The statically allocated Protocol objects that we generate here
   need to be fixed up at runtime in order to be used: the 'isa'
  pointer of the objects need to be set up to point to the 'Protocol'
   class, as known at runtime.

   The NeXT runtime fixes up all protocols at program startup time,
   before main() is entered.  It uses a low-level trick to look up all
   those symbols, then loops on them and fixes them up.

   The GNU runtime as well fixes up all protocols before user code
   from the module is executed; it requires pointers to those symbols
   to be put in the objc_symtab (which is then passed as argument to
   the function __objc_exec_class() which the compiler sets up to be
   executed automatically when the module is loaded); setup of those
   Protocol objects happen in two ways in the GNU runtime: all
   Protocol objects referred to by a class or category implementation
   are fixed up when the class/category is loaded; all Protocol
   objects referred to by a @protocol() expression are added by the
   compiler to the list of statically allocated instances to fixup
   (the same list holding the statically allocated constant string
   objects).  Because, as explained above, the compiler generates as
   few Protocol objects as possible, some Protocol object might end up
   being referenced multiple times when compiled with the GNU runtime,
   and end up being fixed up multiple times at runtime inizialization.
   But that doesn't hurt, it's just a little inefficient.  */
static void
generate_protocols ()
{
  tree p, encoding;
  /* APPLE LOCAL Panther ObjC enhancements */
  tree sc_spec, decl_specs, decl;
  tree initlist, protocol_name_expr, refs_decl, refs_expr;
  tree cast_type2;

  /* APPLE LOCAL Panther ObjC enhancements */
  if (! objc_protocol_template)
    objc_protocol_template = build_protocol_template ();

  /* If a protocol was directly referenced, pull in indirect references.  */
  for (p = protocol_chain; p; p = TREE_CHAIN (p))
    if (PROTOCOL_FORWARD_DECL (p) && PROTOCOL_LIST (p))
      generate_protocol_references (PROTOCOL_LIST (p));

  for (p = protocol_chain; p; p = TREE_CHAIN (p))
  
    {
      tree nst_methods = PROTOCOL_NST_METHODS (p);
      tree cls_methods = PROTOCOL_CLS_METHODS (p);

      /* If protocol wasn't referenced, don't generate any code.  */
      if (! PROTOCOL_FORWARD_DECL (p))
	continue;

      /* Make sure we link in the Protocol class.  */
      add_class_reference (get_identifier (PROTOCOL_OBJECT_CLASS_NAME));

      while (nst_methods)
	{
	  if (! METHOD_ENCODING (nst_methods))
	    {
	      /* APPLE LOCAL Panther ObjC enhancements */
	      encoding = encode_method_prototype (nst_methods);
	      METHOD_ENCODING (nst_methods) = encoding;
	    }
	  nst_methods = TREE_CHAIN (nst_methods);
	}

      while (cls_methods)
	{
	  if (! METHOD_ENCODING (cls_methods))
	    {
	      /* APPLE LOCAL Panther ObjC enhancements */
	      encoding = encode_method_prototype (cls_methods);
	      METHOD_ENCODING (cls_methods) = encoding;
	    }

	  cls_methods = TREE_CHAIN (cls_methods);
	}
      generate_method_descriptors (p);

      if (PROTOCOL_LIST (p))
	refs_decl = generate_protocol_list (p);
      else
	refs_decl = 0;

      /* static struct objc_protocol _OBJC_PROTOCOL_<mumble>; */

      sc_spec = tree_cons (NULL_TREE, ridpointers[(int) RID_STATIC],
			   NULL_TREE);
      decl_specs = tree_cons (NULL_TREE, objc_protocol_template, sc_spec);

      decl = start_decl (synth_id_with_class_suffix ("_OBJC_PROTOCOL", p),
			 decl_specs, 1, NULL_TREE);

      DECL_CONTEXT (decl) = NULL_TREE;

      protocol_name_expr = add_objc_string (PROTOCOL_NAME (p), class_names);

      if (refs_decl)
	{
	  cast_type2
	    = groktypename
		(build_tree_list (build_tree_list (NULL_TREE,
						   objc_protocol_template),
				  build1 (INDIRECT_REF, NULL_TREE,
					  build1 (INDIRECT_REF, NULL_TREE,
						  NULL_TREE))));

	  refs_expr = build_unary_op (ADDR_EXPR, refs_decl, 0);
	  TREE_TYPE (refs_expr) = cast_type2;
	}
      else
	refs_expr = build_int_2 (0, 0);

      /* UOBJC_INSTANCE_METHODS_decl/UOBJC_CLASS_METHODS_decl are set
	 by generate_method_descriptors, which is called above.  */
      initlist = build_protocol_initializer (TREE_TYPE (decl),
					     protocol_name_expr, refs_expr,
					     UOBJC_INSTANCE_METHODS_decl,
					     UOBJC_CLASS_METHODS_decl);
      finish_decl (decl, initlist, NULL_TREE);

      /* Mark the decl as used to avoid "defined but not used" warning.  */
      TREE_USED (decl) = 1;
    }
}

static tree
build_protocol_initializer (type, protocol_name, protocol_list,
			    instance_methods, class_methods)
     tree type;
     tree protocol_name;
     tree protocol_list;
     tree instance_methods;
     tree class_methods;
{
  tree initlist = NULL_TREE, expr;
  tree cast_type;

  cast_type = groktypename
    (build_tree_list
     (build_tree_list (NULL_TREE,
		       xref_tag (RECORD_TYPE,
				 get_identifier (UTAG_CLASS))),
      build1 (INDIRECT_REF, NULL_TREE, NULL_TREE)));

  /* Filling the "isa" in with one allows the runtime system to
     detect that the version change...should remove before final release.  */

  expr = build_int_2 (PROTOCOL_VERSION, 0);
  TREE_TYPE (expr) = cast_type;
  initlist = tree_cons (NULL_TREE, expr, initlist);
  initlist = tree_cons (NULL_TREE, protocol_name, initlist);
  initlist = tree_cons (NULL_TREE, protocol_list, initlist);

  if (!instance_methods)
    initlist = tree_cons (NULL_TREE, build_int_2 (0, 0), initlist);
  else
    {
      expr = build_unary_op (ADDR_EXPR, instance_methods, 0);
      initlist = tree_cons (NULL_TREE, expr, initlist);
    }

  if (!class_methods)
    initlist = tree_cons (NULL_TREE, build_int_2 (0, 0), initlist);
  else
    {
      expr = build_unary_op (ADDR_EXPR, class_methods, 0);
      initlist = tree_cons (NULL_TREE, expr, initlist);
    }

  return build_constructor (type, nreverse (initlist));
}

/* struct objc_category {
     char *category_name;
     char *class_name;
     struct objc_method_list *instance_methods;
     struct objc_method_list *class_methods;
     struct objc_protocol_list *protocols;
   };   */

static void
build_category_template ()
{
  tree decl_specs, field_decl, field_decl_chain;

  objc_category_template = start_struct (RECORD_TYPE,
					 get_identifier (UTAG_CATEGORY));
  /* char *category_name; */

  decl_specs = build_tree_list (NULL_TREE, ridpointers[(int) RID_CHAR]);
  field_decl
    = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("category_name"));
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  field_decl_chain = field_decl;

  /* char *class_name; */

  decl_specs = build_tree_list (NULL_TREE, ridpointers[(int) RID_CHAR]);
  field_decl = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("class_name"));
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  /* struct objc_method_list *instance_methods; */

  decl_specs = build_tree_list (NULL_TREE,
				xref_tag (RECORD_TYPE,
					  get_identifier (UTAG_METHOD_LIST)));
  field_decl
    = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("instance_methods"));
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  /* struct objc_method_list *class_methods; */

  decl_specs = build_tree_list (NULL_TREE,
				xref_tag (RECORD_TYPE,
					  get_identifier (UTAG_METHOD_LIST)));
  field_decl
    = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("class_methods"));
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  /* struct objc_protocol **protocol_list; */

  decl_specs = build_tree_list (NULL_TREE,
				xref_tag (RECORD_TYPE,
					  get_identifier (UTAG_PROTOCOL)));
  field_decl
    = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("protocol_list"));
  field_decl = build1 (INDIRECT_REF, NULL_TREE, field_decl);
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  finish_struct (objc_category_template, field_decl_chain, NULL_TREE);
}

/* struct objc_selector {
     void *sel_id;
     char *sel_type;
   }; */

static void
build_selector_template ()
{

  tree decl_specs, field_decl, field_decl_chain;

  objc_selector_template 
    = start_struct (RECORD_TYPE, get_identifier (UTAG_SELECTOR));

  /* void *sel_id; */

  decl_specs = build_tree_list (NULL_TREE, ridpointers[(int) RID_VOID]);
  field_decl = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("sel_id"));
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  field_decl_chain = field_decl;

  /* char *sel_type; */

  decl_specs = build_tree_list (NULL_TREE, ridpointers[(int) RID_CHAR]);
  field_decl = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("sel_type"));
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  finish_struct (objc_selector_template, field_decl_chain, NULL_TREE);
}

/* struct objc_class {
     struct objc_class *isa;
     struct objc_class *super_class;
     char *name;
     long version;
     long info;
     long instance_size;
     struct objc_ivar_list *ivars;
     struct objc_method_list *methods;
     if (flag_next_runtime)
       struct objc_cache *cache;
     else {
       struct sarray *dtable;
       struct objc_class *subclass_list;
       struct objc_class *sibling_class;
     }
     struct objc_protocol_list *protocols;
     APPLE LOCAL begin Panther ObjC enhancements
     if (flag_next_runtime)
       void *sel_id;
     APPLE LOCAL end Panther ObjC enhancements
     void *gc_object_type;
   };  */

/* APPLE LOCAL begin Panther ObjC enhancements */
/* NB: The 'sel_id' and 'gc_object_type' fields are not being used by the NeXT/Apple
   runtime; still, the compiler must generate them to maintain backward binary
   compatibility (and to allow for future expansion).  */
/* APPLE LOCAL end Panther ObjC enhancements */

static void
build_class_template ()
{
  tree decl_specs, field_decl, field_decl_chain;

  objc_class_template
    = start_struct (RECORD_TYPE, get_identifier (UTAG_CLASS));

  /* struct objc_class *isa; */

  decl_specs = build_tree_list (NULL_TREE, objc_class_template);
  field_decl = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("isa"));
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  field_decl_chain = field_decl;

  /* struct objc_class *super_class; */

  decl_specs = build_tree_list (NULL_TREE, objc_class_template);
  field_decl
    = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("super_class"));
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  /* char *name; */

  decl_specs = build_tree_list (NULL_TREE, ridpointers[(int) RID_CHAR]);
  field_decl = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("name"));
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  /* long version; */

  decl_specs = build_tree_list (NULL_TREE, ridpointers[(int) RID_LONG]);
  field_decl = get_identifier ("version");
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  /* long info; */

  decl_specs = build_tree_list (NULL_TREE, ridpointers[(int) RID_LONG]);
  field_decl = get_identifier ("info");
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  /* long instance_size; */

  decl_specs = build_tree_list (NULL_TREE, ridpointers[(int) RID_LONG]);
  field_decl = get_identifier ("instance_size");
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  /* struct objc_ivar_list *ivars; */

  decl_specs = build_tree_list (NULL_TREE,
				xref_tag (RECORD_TYPE,
					  get_identifier (UTAG_IVAR_LIST)));
  field_decl = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("ivars"));
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  /* struct objc_method_list *methods; */

  decl_specs = build_tree_list (NULL_TREE,
				xref_tag (RECORD_TYPE,
					  get_identifier (UTAG_METHOD_LIST)));
  field_decl = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("methods"));
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  if (flag_next_runtime)
    {
      /* struct objc_cache *cache; */

      decl_specs = build_tree_list (NULL_TREE,
				    xref_tag (RECORD_TYPE,
					      get_identifier ("objc_cache")));
      field_decl = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("cache"));
      field_decl = grokfield (input_filename, lineno, field_decl,
			      decl_specs, NULL_TREE);
      chainon (field_decl_chain, field_decl);
    }
  else
    {
      /* struct sarray *dtable; */

      decl_specs = build_tree_list (NULL_TREE,
				    xref_tag (RECORD_TYPE,
					      get_identifier ("sarray")));
      field_decl = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("dtable"));
      field_decl = grokfield (input_filename, lineno, field_decl,
			      decl_specs, NULL_TREE);
      chainon (field_decl_chain, field_decl);

      /* struct objc_class *subclass_list; */

      decl_specs = build_tree_list (NULL_TREE, objc_class_template);
      field_decl
	= build1 (INDIRECT_REF, NULL_TREE, get_identifier ("subclass_list"));
      field_decl = grokfield (input_filename, lineno, field_decl,
			      decl_specs, NULL_TREE);
      chainon (field_decl_chain, field_decl);

      /* struct objc_class *sibling_class; */

      decl_specs = build_tree_list (NULL_TREE, objc_class_template);
      field_decl
	= build1 (INDIRECT_REF, NULL_TREE, get_identifier ("sibling_class"));
      field_decl = grokfield (input_filename, lineno, field_decl,
			      decl_specs, NULL_TREE);
      chainon (field_decl_chain, field_decl);
    }

  /* struct objc_protocol **protocol_list; */

  decl_specs = build_tree_list (NULL_TREE, 
				xref_tag (RECORD_TYPE,
					  get_identifier (UTAG_PROTOCOL)));
  field_decl
    = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("protocol_list"));
  field_decl
    = build1 (INDIRECT_REF, NULL_TREE, field_decl);
  field_decl = grokfield (input_filename, lineno, field_decl,
			  decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  /* APPLE LOCAL begin Panther ObjC enhancements */
  if (flag_next_runtime)
    {
      /* void *sel_id; */

      decl_specs = build_tree_list (NULL_TREE, ridpointers[(int) RID_VOID]);
      field_decl = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("sel_id"));
      field_decl
	= grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
      chainon (field_decl_chain, field_decl);
    }
  /* APPLE LOCAL end Panther ObjC enhancements */

  /* void *gc_object_type; */

  decl_specs = build_tree_list (NULL_TREE, ridpointers[(int) RID_VOID]);
  field_decl = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("gc_object_type"));
  field_decl
    = grokfield (input_filename, lineno, field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  finish_struct (objc_class_template, field_decl_chain, NULL_TREE);
}

/* Generate appropriate forward declarations for an implementation.  */

static void
synth_forward_declarations ()
{
  /* APPLE LOCAL Objective-C++ */
  tree an_id;

  /* extern struct objc_class _OBJC_CLASS_<my_name>; */

  /* APPLE LOCAL Objective-C++ */
  UOBJC_CLASS_decl = build_metadata_decl ("_OBJC_CLASS", 
						  objc_class_template); 

  /* extern struct objc_class _OBJC_METACLASS_<my_name>; */

  /* APPLE LOCAL Objective-C++ */
  UOBJC_METACLASS_decl = build_metadata_decl ("_OBJC_METACLASS", 
						  objc_class_template); 

  /* Pre-build the following entities - for speed/convenience.  */

  an_id = get_identifier ("super_class");
  ucls_super_ref = build_component_ref (UOBJC_CLASS_decl, an_id);
  uucls_super_ref = build_component_ref (UOBJC_METACLASS_decl, an_id);
}

static void
error_with_ivar (message, decl, rawdecl)
     const char *message;
     tree decl;
     tree rawdecl;
{
  diagnostic_count_diagnostic (global_dc, DK_ERROR);

  diagnostic_report_current_function (global_dc);

  error_with_file_and_line (DECL_SOURCE_FILE (decl),
			    DECL_SOURCE_LINE (decl),
			    "%s `%s'",
			    message, gen_declaration (rawdecl, errbuf));

}

static void
check_ivars (inter, imp)
     tree inter;
     tree imp;
{
  tree intdecls = CLASS_IVARS (inter);
  tree impdecls = CLASS_IVARS (imp);
  tree rawintdecls = CLASS_RAW_IVARS (inter);
  tree rawimpdecls = CLASS_RAW_IVARS (imp);

  while (1)
    {
      tree t1, t2;

      /* APPLE LOCAL begin Objective-C++ */
#ifdef OBJCPLUS
      if (intdecls && TREE_CODE (intdecls) == TYPE_DECL)
	intdecls = TREE_CHAIN (intdecls);
#endif
      /* APPLE LOCAL end Objective-C++ */
      if (intdecls == 0 && impdecls == 0)
	break;
      if (intdecls == 0 || impdecls == 0)
	{
	  error ("inconsistent instance variable specification");
	  break;
	}

      t1 = TREE_TYPE (intdecls); t2 = TREE_TYPE (impdecls);

/* BEGIN APPLE LOCAL IMI */
      if (!comptypes (t1, t2, false)
/* END APPLE LOCAL IMI */
      /* APPLE LOCAL bitfields */
	  || !tree_int_cst_equal (TREE_VALUE (TREE_VALUE (rawintdecls)),
				  TREE_VALUE (TREE_VALUE (rawimpdecls))))
	{
	  if (DECL_NAME (intdecls) == DECL_NAME (impdecls))
	    {
	      error_with_ivar ("conflicting instance variable type",
			       impdecls, rawimpdecls);
	      error_with_ivar ("previous declaration of",
			       intdecls, rawintdecls);
	    }
	  else			/* both the type and the name don't match */
	    {
	      error ("inconsistent instance variable specification");
	      break;
	    }
	}

      else if (DECL_NAME (intdecls) != DECL_NAME (impdecls))
	{
	  error_with_ivar ("conflicting instance variable name",
			   impdecls, rawimpdecls);
	  error_with_ivar ("previous declaration of",
			   intdecls, rawintdecls);
	}

      intdecls = TREE_CHAIN (intdecls);
      impdecls = TREE_CHAIN (impdecls);
      rawintdecls = TREE_CHAIN (rawintdecls);
      rawimpdecls = TREE_CHAIN (rawimpdecls);
    }
}

/* APPLE LOCAL begin msg send super */
/* Set 'objc_super_template' to the data type node for 'struct _objc_super'.
   This needs to be done just once per compilation.  */

static void
build_super_template ()
{
  tree decl_specs, field_decl, field_decl_chain;

  /* Suppress outputting debug symbols, because
     dbxout_init hasn't been called yet.  */
  enum debug_info_type save_write_symbols = write_symbols;
  const struct gcc_debug_hooks *save_hooks = debug_hooks;
  
  write_symbols = NO_DEBUG;
  debug_hooks = &do_nothing_debug_hooks;

  objc_super_template = start_struct (RECORD_TYPE, get_identifier (UTAG_SUPER));
  /* APPLE LOCAL end msg send super */

  /* struct objc_object *self; */

  decl_specs = build_tree_list (NULL_TREE, objc_object_reference);
  field_decl = get_identifier ("self");
  field_decl = build1 (INDIRECT_REF, NULL_TREE, field_decl);
  field_decl = grokfield (input_filename, lineno,
			  field_decl, decl_specs, NULL_TREE);
  field_decl_chain = field_decl;

  /* struct objc_class *class; */

  decl_specs = get_identifier (UTAG_CLASS);
  decl_specs = build_tree_list (NULL_TREE, xref_tag (RECORD_TYPE, decl_specs));
  field_decl = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("class"));

  field_decl = grokfield (input_filename, lineno,
			  field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  /* APPLE LOCAL begin msg send super */
  finish_struct (objc_super_template, field_decl_chain, NULL_TREE);

  write_symbols = save_write_symbols;
  debug_hooks = save_hooks;
  /* APPLE LOCAL end msg send super */
}

/* struct objc_ivar {
     char *ivar_name;
     char *ivar_type;
     int ivar_offset;
   };  */

static tree
build_ivar_template ()
{
  tree objc_ivar_id, objc_ivar_record;
  tree decl_specs, field_decl, field_decl_chain;

  objc_ivar_id = get_identifier (UTAG_IVAR);
  objc_ivar_record = start_struct (RECORD_TYPE, objc_ivar_id);

  /* char *ivar_name; */

  decl_specs = build_tree_list (NULL_TREE, ridpointers[(int) RID_CHAR]);
  field_decl = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("ivar_name"));

  field_decl = grokfield (input_filename, lineno, field_decl,
			  decl_specs, NULL_TREE);
  field_decl_chain = field_decl;

  /* char *ivar_type; */

  decl_specs = build_tree_list (NULL_TREE, ridpointers[(int) RID_CHAR]);
  field_decl = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("ivar_type"));

  field_decl = grokfield (input_filename, lineno, field_decl,
			  decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  /* int ivar_offset; */

  decl_specs = build_tree_list (NULL_TREE, ridpointers[(int) RID_INT]);
  field_decl = get_identifier ("ivar_offset");

  field_decl = grokfield (input_filename, lineno, field_decl,
			  decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  finish_struct (objc_ivar_record, field_decl_chain, NULL_TREE);

  return objc_ivar_record;
}

/* struct {
     int ivar_count;
     struct objc_ivar ivar_list[ivar_count];
   };  */

static tree
build_ivar_list_template (list_type, size)
     tree list_type;
     int size;
{
  tree objc_ivar_list_record;
  tree decl_specs, field_decl, field_decl_chain;

  objc_ivar_list_record = start_struct (RECORD_TYPE, NULL_TREE);

  /* int ivar_count; */

  decl_specs = build_tree_list (NULL_TREE, ridpointers[(int) RID_INT]);
  field_decl = get_identifier ("ivar_count");

  field_decl = grokfield (input_filename, lineno, field_decl,
			  decl_specs, NULL_TREE);
  field_decl_chain = field_decl;

  /* struct objc_ivar ivar_list[]; */

  decl_specs = build_tree_list (NULL_TREE, list_type);
  field_decl = build_nt (ARRAY_REF, get_identifier ("ivar_list"),
			 build_int_2 (size, 0));

  field_decl = grokfield (input_filename, lineno,
			  field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  finish_struct (objc_ivar_list_record, field_decl_chain, NULL_TREE);

  return objc_ivar_list_record;
}

/* struct {
     int method_next;
     int method_count;
     struct objc_method method_list[method_count];
   };  */

static tree
build_method_list_template (list_type, size)
     tree list_type;
     int size;
{
  tree objc_ivar_list_record;
  tree decl_specs, field_decl, field_decl_chain;

  objc_ivar_list_record = start_struct (RECORD_TYPE, NULL_TREE);

  /* int method_next; */

  decl_specs
    = build_tree_list
      (NULL_TREE, 
       xref_tag (RECORD_TYPE,
		 get_identifier (UTAG_METHOD_PROTOTYPE_LIST)));
  field_decl
    = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("method_next"));
  field_decl = grokfield (input_filename, lineno, field_decl,
			  decl_specs, NULL_TREE);
  field_decl_chain = field_decl;

  /* int method_count; */

  decl_specs = build_tree_list (NULL_TREE, ridpointers[(int) RID_INT]);
  field_decl = get_identifier ("method_count");

  field_decl = grokfield (input_filename, lineno,
			  field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  /* struct objc_method method_list[]; */

  decl_specs = build_tree_list (NULL_TREE, list_type);
  field_decl = build_nt (ARRAY_REF, get_identifier ("method_list"),
			 build_int_2 (size, 0));

  field_decl = grokfield (input_filename, lineno,
			  field_decl, decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  finish_struct (objc_ivar_list_record, field_decl_chain, NULL_TREE);

  return objc_ivar_list_record;
}

static tree
build_ivar_list_initializer (type, field_decl)
     tree type;
     tree field_decl;
{
  tree initlist = NULL_TREE;

  do
    {
      tree ivar = NULL_TREE;

      /* Set name.  */
      if (DECL_NAME (field_decl))
	ivar = tree_cons (NULL_TREE,
			  add_objc_string (DECL_NAME (field_decl),
					   meth_var_names),
			  ivar);
      else
	/* Unnamed bit-field ivar (yuck).  */
	ivar = tree_cons (NULL_TREE, build_int_2 (0, 0), ivar);

      /* Set type.  */
      encode_field_decl (field_decl,
			 obstack_object_size (&util_obstack),
			 OBJC_ENCODE_DONT_INLINE_DEFS);

      /* Null terminate string.  */
      obstack_1grow (&util_obstack, 0);
      ivar
	= tree_cons
	  (NULL_TREE,
	   add_objc_string (get_identifier (obstack_finish (&util_obstack)),
			    meth_var_types),
	   ivar);
      obstack_free (&util_obstack, util_firstobj);

      /* Set offset.  */
      ivar = tree_cons (NULL_TREE, byte_position (field_decl), ivar);
      initlist = tree_cons (NULL_TREE, 
			    build_constructor (type, nreverse (ivar)),
			    initlist);
      /* APPLE LOCAL begin Objective-C++ */
      do
	field_decl = TREE_CHAIN (field_decl);
      while (field_decl && TREE_CODE (field_decl) != FIELD_DECL);
      /* APPLE LOCAL end Objective-C++ */
    }
  while (field_decl);

  return build_constructor (build_array_type (type, 0), nreverse (initlist));
}

static tree
generate_ivars_list (type, name, size, list)
     tree type;
     const char *name;
     int size;
     tree list;
{
  tree sc_spec, decl_specs, decl, initlist;

  sc_spec = tree_cons (NULL_TREE, ridpointers[(int) RID_STATIC], NULL_TREE);
  decl_specs = tree_cons (NULL_TREE, type, sc_spec);

  decl = start_decl (synth_id_with_class_suffix (name, objc_implementation_context),
		     decl_specs, 1, NULL_TREE);

  initlist = build_tree_list (NULL_TREE, build_int_2 (size, 0));
  initlist = tree_cons (NULL_TREE, list, initlist);

  finish_decl (decl,
	       build_constructor (TREE_TYPE (decl), nreverse (initlist)),
	       NULL_TREE);

  return decl;
}

/* APPLE LOCAL begin Objective-C++ */
/* Count only the fields occurring in T.  */
static int
ivar_list_length (t)
     tree t;
{
  int count = 0;

  for (; t; t = TREE_CHAIN (t))
    if (TREE_CODE (t) == FIELD_DECL)
      ++count;

  return count;
}
/* APPLE LOCAL end Objective-C++ */

static void
generate_ivar_lists ()
{
  tree initlist, ivar_list_template, chain;
  tree cast, variable_length_type;
  int size;
  
  generating_instance_variables = 1;

  if (!objc_ivar_template)
    objc_ivar_template = build_ivar_template ();

  cast
    = build_tree_list
      (build_tree_list (NULL_TREE, xref_tag (RECORD_TYPE,
					 get_identifier (UTAG_IVAR_LIST))),
       NULL_TREE);
  variable_length_type = groktypename (cast);

  /* Only generate class variables for the root of the inheritance
     hierarchy since these will be the same for every class.  */

  if (CLASS_SUPER_NAME (implementation_template) == NULL_TREE
      && (chain = TYPE_FIELDS (objc_class_template)))
    {
      /* APPLE LOCAL Objective-C++ */
      size = ivar_list_length (chain);

      ivar_list_template = build_ivar_list_template (objc_ivar_template, size);
      initlist = build_ivar_list_initializer (objc_ivar_template, chain);

      UOBJC_CLASS_VARIABLES_decl
	= generate_ivars_list (ivar_list_template, "_OBJC_CLASS_VARIABLES",
			       size, initlist);
      TREE_TYPE (UOBJC_CLASS_VARIABLES_decl) = variable_length_type;
    }
  else
    UOBJC_CLASS_VARIABLES_decl = 0;

  chain = CLASS_IVARS (implementation_template);
  if (chain)
    {
      /* APPLE LOCAl Objective-C++ */
      size = ivar_list_length (chain);
      ivar_list_template = build_ivar_list_template (objc_ivar_template, size);
      initlist = build_ivar_list_initializer (objc_ivar_template, chain);

      UOBJC_INSTANCE_VARIABLES_decl
	= generate_ivars_list (ivar_list_template, "_OBJC_INSTANCE_VARIABLES",
			       size, initlist);
      TREE_TYPE (UOBJC_INSTANCE_VARIABLES_decl) = variable_length_type;
    }
  else
    UOBJC_INSTANCE_VARIABLES_decl = 0;

  generating_instance_variables = 0;
}

static tree
build_dispatch_table_initializer (type, entries)
     tree type;
     tree entries;
{
  tree initlist = NULL_TREE;

  do
    {
      tree elemlist = NULL_TREE;

      elemlist = tree_cons (NULL_TREE,
			    build_selector (METHOD_SEL_NAME (entries)),
			    NULL_TREE);

      /* Generate the method encoding if we don't have one already.  */
      if (! METHOD_ENCODING (entries))
	METHOD_ENCODING (entries) =
	  /* APPLE LOCAL method encoding */
	  encode_method_prototype (entries);

      elemlist = tree_cons (NULL_TREE,
			    add_objc_string (METHOD_ENCODING (entries),
					     meth_var_types),
			    elemlist);

      elemlist = tree_cons (NULL_TREE, 
			    build_unary_op (ADDR_EXPR,
					    METHOD_DEFINITION (entries), 1),
			    elemlist);

      initlist = tree_cons (NULL_TREE, 
			    build_constructor (type, nreverse (elemlist)),
			    initlist);

      entries = TREE_CHAIN (entries);
    }
  while (entries);

  return build_constructor (build_array_type (type, 0), nreverse (initlist));
}

/* To accomplish method prototyping without generating all kinds of
   inane warnings, the definition of the dispatch table entries were
   changed from:

   	struct objc_method { SEL _cmd; ...; id (*_imp)(); };
   to:
   	struct objc_method { SEL _cmd; ...; void *_imp; };  */

static tree
build_method_template ()
{
  tree _SLT_record;
  tree decl_specs, field_decl, field_decl_chain;

  _SLT_record = start_struct (RECORD_TYPE, get_identifier (UTAG_METHOD));

  /* struct objc_selector *_cmd; */
  decl_specs = tree_cons (NULL_TREE,
			  xref_tag (RECORD_TYPE,
				    get_identifier (TAG_SELECTOR)),
			  NULL_TREE);
  field_decl = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("_cmd"));

  field_decl = grokfield (input_filename, lineno, field_decl,
			  decl_specs, NULL_TREE);
  field_decl_chain = field_decl;

  decl_specs = tree_cons (NULL_TREE, ridpointers[(int) RID_CHAR], NULL_TREE);
  field_decl = build1 (INDIRECT_REF, NULL_TREE,
		       get_identifier ("method_types"));
  field_decl = grokfield (input_filename, lineno, field_decl,
			  decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  /* void *_imp; */

  decl_specs = tree_cons (NULL_TREE, ridpointers[(int) RID_VOID], NULL_TREE);
  field_decl = build1 (INDIRECT_REF, NULL_TREE, get_identifier ("_imp"));
  field_decl = grokfield (input_filename, lineno, field_decl,
			  decl_specs, NULL_TREE);
  chainon (field_decl_chain, field_decl);

  finish_struct (_SLT_record, field_decl_chain, NULL_TREE);

  return _SLT_record;
}


static tree
generate_dispatch_table (type, name, size, list)
     tree type;
     const char *name;
     int size;
     tree list;
{
  tree sc_spec, decl_specs, decl, initlist;

  sc_spec = tree_cons (NULL_TREE, ridpointers[(int) RID_STATIC], NULL_TREE);
  decl_specs = tree_cons (NULL_TREE, type, sc_spec);

  decl = start_decl (synth_id_with_class_suffix (name, objc_implementation_context),
		     decl_specs, 1, NULL_TREE);

  initlist = build_tree_list (NULL_TREE, build_int_2 (0, 0));
  initlist = tree_cons (NULL_TREE, build_int_2 (size, 0), initlist);
  initlist = tree_cons (NULL_TREE, list, initlist);

  finish_decl (decl,
	       build_constructor (TREE_TYPE (decl), nreverse (initlist)),
	       NULL_TREE);

  return decl;
}

static void
generate_dispatch_tables ()
{
  tree initlist, chain, method_list_template;
  tree cast, variable_length_type;
  int size;

  if (!objc_method_template)
    objc_method_template = build_method_template ();

  cast
    = build_tree_list
      (build_tree_list (NULL_TREE,
			xref_tag (RECORD_TYPE,
				  get_identifier (UTAG_METHOD_LIST))),
       NULL_TREE);

  variable_length_type = groktypename (cast);

  chain = CLASS_CLS_METHODS (objc_implementation_context);
  if (chain)
    {
      size = list_length (chain);

      method_list_template
	= build_method_list_template (objc_method_template, size);
      initlist
	= build_dispatch_table_initializer (objc_method_template, chain);

      UOBJC_CLASS_METHODS_decl
	= generate_dispatch_table (method_list_template,
				   ((TREE_CODE (objc_implementation_context)
				     == CLASS_IMPLEMENTATION_TYPE)
				    ? "_OBJC_CLASS_METHODS"
				    : "_OBJC_CATEGORY_CLASS_METHODS"),
				   size, initlist);
      TREE_TYPE (UOBJC_CLASS_METHODS_decl) = variable_length_type;
    }
  else
    UOBJC_CLASS_METHODS_decl = 0;

  chain = CLASS_NST_METHODS (objc_implementation_context);
  if (chain)
    {
      size = list_length (chain);

      method_list_template
	= build_method_list_template (objc_method_template, size);
      initlist
	= build_dispatch_table_initializer (objc_method_template, chain);

      if (TREE_CODE (objc_implementation_context) == CLASS_IMPLEMENTATION_TYPE)
	UOBJC_INSTANCE_METHODS_decl
	  = generate_dispatch_table (method_list_template,
				     "_OBJC_INSTANCE_METHODS",
				     size, initlist);
      else
	/* We have a category.  */
	UOBJC_INSTANCE_METHODS_decl
	  = generate_dispatch_table (method_list_template,
				     "_OBJC_CATEGORY_INSTANCE_METHODS",
				     size, initlist);
      TREE_TYPE (UOBJC_INSTANCE_METHODS_decl) = variable_length_type;
    }
  else
    UOBJC_INSTANCE_METHODS_decl = 0;
}

static tree
generate_protocol_list (i_or_p)
     tree i_or_p;
{
  tree initlist, decl_specs, sc_spec;
  tree refs_decl, expr_decl, lproto, e, plist;
  tree cast_type;
  int size = 0;

  if (TREE_CODE (i_or_p) == CLASS_INTERFACE_TYPE
      || TREE_CODE (i_or_p) == CATEGORY_INTERFACE_TYPE)
    plist = CLASS_PROTOCOL_LIST (i_or_p);
  else if (TREE_CODE (i_or_p) == PROTOCOL_INTERFACE_TYPE)
    plist = PROTOCOL_LIST (i_or_p);
  else
    abort ();

  cast_type = groktypename
    (build_tree_list
     (build_tree_list (NULL_TREE,
		       xref_tag (RECORD_TYPE,
				 get_identifier (UTAG_PROTOCOL))),
      build1 (INDIRECT_REF, NULL_TREE, NULL_TREE)));

  /* Compute size.  */
  for (lproto = plist; lproto; lproto = TREE_CHAIN (lproto))
    if (TREE_CODE (TREE_VALUE (lproto)) == PROTOCOL_INTERFACE_TYPE
	&& PROTOCOL_FORWARD_DECL (TREE_VALUE (lproto)))
      size++;

  /* Build initializer.  */
  initlist = tree_cons (NULL_TREE, build_int_2 (0, 0), NULL_TREE);

  e = build_int_2 (size, 0);
  TREE_TYPE (e) = cast_type;
  initlist = tree_cons (NULL_TREE, e, initlist);

  for (lproto = plist; lproto; lproto = TREE_CHAIN (lproto))
    {
      tree pval = TREE_VALUE (lproto);

      if (TREE_CODE (pval) == PROTOCOL_INTERFACE_TYPE
	  && PROTOCOL_FORWARD_DECL (pval))
	{
	  e = build_unary_op (ADDR_EXPR, PROTOCOL_FORWARD_DECL (pval), 0);
	  initlist = tree_cons (NULL_TREE, e, initlist);
	}
    }

  /* static struct objc_protocol *refs[n]; */

  sc_spec = tree_cons (NULL_TREE, ridpointers[(int) RID_STATIC], NULL_TREE);
  decl_specs = tree_cons (NULL_TREE, xref_tag (RECORD_TYPE,
					   get_identifier (UTAG_PROTOCOL)),
			  sc_spec);

  if (TREE_CODE (i_or_p) == PROTOCOL_INTERFACE_TYPE)
    expr_decl = build_nt (ARRAY_REF,
			  synth_id_with_class_suffix ("_OBJC_PROTOCOL_REFS",
						      i_or_p),
			  build_int_2 (size + 2, 0));
  else if (TREE_CODE (i_or_p) == CLASS_INTERFACE_TYPE)
    expr_decl = build_nt (ARRAY_REF,
			  synth_id_with_class_suffix ("_OBJC_CLASS_PROTOCOLS",
						      i_or_p),
			  build_int_2 (size + 2, 0));
  else if (TREE_CODE (i_or_p) == CATEGORY_INTERFACE_TYPE)
    expr_decl
      = build_nt (ARRAY_REF,
		  synth_id_with_class_suffix ("_OBJC_CATEGORY_PROTOCOLS",
					      i_or_p),
		  build_int_2 (size + 2, 0));
  else
    abort ();

  expr_decl = build1 (INDIRECT_REF, NULL_TREE, expr_decl);

  refs_decl = start_decl (expr_decl, decl_specs, 1, NULL_TREE);
  DECL_CONTEXT (refs_decl) = NULL_TREE;

  finish_decl (refs_decl, build_constructor (TREE_TYPE (refs_decl),
					     nreverse (initlist)),
	       NULL_TREE);

  return refs_decl;
}

static tree
build_category_initializer (type, cat_name, class_name,
			    instance_methods, class_methods, protocol_list)
     tree type;
     tree cat_name;
     tree class_name;
     tree instance_methods;
     tree class_methods;
     tree protocol_list;
{
  tree initlist = NULL_TREE, expr;

  initlist = tree_cons (NULL_TREE, cat_name, initlist);
  initlist = tree_cons (NULL_TREE, class_name, initlist);

  if (!instance_methods)
    initlist = tree_cons (NULL_TREE, build_int_2 (0, 0), initlist);
  else
    {
      expr = build_unary_op (ADDR_EXPR, instance_methods, 0);
      initlist = tree_cons (NULL_TREE, expr, initlist);
    }
  if (!class_methods)
    initlist = tree_cons (NULL_TREE, build_int_2 (0, 0), initlist);
  else
    {
      expr = build_unary_op (ADDR_EXPR, class_methods, 0);
      initlist = tree_cons (NULL_TREE, expr, initlist);
    }

  /* protocol_list = */
  if (!protocol_list)
     initlist = tree_cons (NULL_TREE, build_int_2 (0, 0), initlist);
  else
     {
       tree cast_type2 = groktypename
	 (build_tree_list
	  (build_tree_list (NULL_TREE,
			    xref_tag (RECORD_TYPE,
				      get_identifier (UTAG_PROTOCOL))),
	   build1 (INDIRECT_REF, NULL_TREE,
		   build1 (INDIRECT_REF, NULL_TREE, NULL_TREE))));

	expr = build_unary_op (ADDR_EXPR, protocol_list, 0);
	TREE_TYPE (expr) = cast_type2;
	initlist = tree_cons (NULL_TREE, expr, initlist);
     }

  return build_constructor (type, nreverse (initlist));
}

/* struct objc_class {
     struct objc_class *isa;
     struct objc_class *super_class;
     char *name;
     long version;
     long info;
     long instance_size;
     struct objc_ivar_list *ivars;
     struct objc_method_list *methods;
     if (flag_next_runtime)
       struct objc_cache *cache;
     else {
       struct sarray *dtable;
       struct objc_class *subclass_list;
       struct objc_class *sibling_class;
     }
     struct objc_protocol_list *protocols;
     APPLE LOCAL begin Panther ObjC enhancements
     if (flag_next_runtime)
       void *sel_id;
     APPLE LOCAL end Panther ObjC enhancements
     void *gc_object_type;
   };  */

static tree
build_shared_structure_initializer (type, isa, super, name, size, status,
				    dispatch_table, ivar_list, protocol_list)
     tree type;
     tree isa;
     tree super;
     tree name;
     tree size;
     int status;
     tree dispatch_table;
     tree ivar_list;
     tree protocol_list;
{
  tree initlist = NULL_TREE, expr;

  /* isa = */
  initlist = tree_cons (NULL_TREE, isa, initlist);

  /* super_class = */
  initlist = tree_cons (NULL_TREE, super, initlist);

  /* name = */
  initlist = tree_cons (NULL_TREE, default_conversion (name), initlist);

  /* version = */
  initlist = tree_cons (NULL_TREE, build_int_2 (0, 0), initlist);

  /* info = */
  initlist = tree_cons (NULL_TREE, build_int_2 (status, 0), initlist);

  /* instance_size = */
  initlist = tree_cons (NULL_TREE, size, initlist);

  /* objc_ivar_list = */
  if (!ivar_list)
    initlist = tree_cons (NULL_TREE, build_int_2 (0, 0), initlist);
  else
    {
      expr = build_unary_op (ADDR_EXPR, ivar_list, 0);
      initlist = tree_cons (NULL_TREE, expr, initlist);
    }

  /* objc_method_list = */
  if (!dispatch_table)
    initlist = tree_cons (NULL_TREE, build_int_2 (0, 0), initlist);
  else
    {
      expr = build_unary_op (ADDR_EXPR, dispatch_table, 0);
      initlist = tree_cons (NULL_TREE, expr, initlist);
    }

  if (flag_next_runtime)
    /* method_cache = */
    initlist = tree_cons (NULL_TREE, build_int_2 (0, 0), initlist);
  else
    {
      /* dtable = */
      initlist = tree_cons (NULL_TREE, build_int_2 (0, 0), initlist);

      /* subclass_list = */
      initlist = tree_cons (NULL_TREE, build_int_2 (0, 0), initlist);

      /* sibling_class = */
      initlist = tree_cons (NULL_TREE, build_int_2 (0, 0), initlist);
    }

  /* protocol_list = */
  if (! protocol_list)
    initlist = tree_cons (NULL_TREE, build_int_2 (0, 0), initlist);
  else
     {
       tree cast_type2
	 = groktypename
	 (build_tree_list
	  (build_tree_list (NULL_TREE,
			    xref_tag (RECORD_TYPE,
				      get_identifier (UTAG_PROTOCOL))),
	   build1 (INDIRECT_REF, NULL_TREE,
		   build1 (INDIRECT_REF, NULL_TREE, NULL_TREE))));

     expr = build_unary_op (ADDR_EXPR, protocol_list, 0);
     TREE_TYPE (expr) = cast_type2;
     initlist = tree_cons (NULL_TREE, expr, initlist);
     }

  /* APPLE LOCAL begin Panther ObjC enhancements */
  if (flag_next_runtime)
    /* sel_id = NULL */
    initlist = tree_cons (NULL_TREE, build_int_2 (0, 0), initlist);
  /* APPLE LOCAL end Panther ObjC enhancements */

  /* gc_object_type = NULL */
  initlist = tree_cons (NULL_TREE, build_int_2 (0, 0), initlist);

  return build_constructor (type, nreverse (initlist));
}

/* static struct objc_category _OBJC_CATEGORY_<name> = { ... };  */

static void
generate_category (cat)
     tree cat;
{
  tree sc_spec, decl_specs, decl;
  tree initlist, cat_name_expr, class_name_expr;
  tree protocol_decl, category;

  add_class_reference (CLASS_NAME (cat));
  cat_name_expr = add_objc_string (CLASS_SUPER_NAME (cat), class_names);

  class_name_expr = add_objc_string (CLASS_NAME (cat), class_names);

  category = CLASS_CATEGORY_LIST (implementation_template);

  /* find the category interface from the class it is associated with */
  while (category)
    {
      if (CLASS_SUPER_NAME (cat) == CLASS_SUPER_NAME (category))
	break;
      category = CLASS_CATEGORY_LIST (category);
    }

  if (category && CLASS_PROTOCOL_LIST (category))
    {
      generate_protocol_references (CLASS_PROTOCOL_LIST (category));
      protocol_decl = generate_protocol_list (category);
    }
  else
    protocol_decl = 0;

  sc_spec = tree_cons (NULL_TREE, ridpointers[(int) RID_STATIC], NULL_TREE);
  decl_specs = tree_cons (NULL_TREE, objc_category_template, sc_spec);

  decl = start_decl (synth_id_with_class_suffix ("_OBJC_CATEGORY",
						 objc_implementation_context),
		     decl_specs, 1, NULL_TREE);

  initlist = build_category_initializer (TREE_TYPE (decl),
					 cat_name_expr, class_name_expr,
					 UOBJC_INSTANCE_METHODS_decl,
					 UOBJC_CLASS_METHODS_decl,
					 protocol_decl);

  /* APPLE LOCAL Objective-C++ */
  finish_decl (decl, initlist, NULL_TREE);
}

/* static struct objc_class _OBJC_METACLASS_Foo={ ... };
   static struct objc_class _OBJC_CLASS_Foo={ ... };  */

static void
generate_shared_structures ()
{
  tree sc_spec, decl_specs, decl;
  tree name_expr, super_expr, root_expr;
  tree my_root_id = NULL_TREE, my_super_id = NULL_TREE;
  tree cast_type, initlist, protocol_decl;

  my_super_id = CLASS_SUPER_NAME (implementation_template);
  if (my_super_id)
    {
      add_class_reference (my_super_id);

      /* Compute "my_root_id" - this is required for code generation.
         the "isa" for all meta class structures points to the root of
         the inheritance hierarchy (e.g. "__Object")...  */
      my_root_id = my_super_id;
      do
	{
	  tree my_root_int = lookup_interface (my_root_id);

	  if (my_root_int && CLASS_SUPER_NAME (my_root_int))
	    my_root_id = CLASS_SUPER_NAME (my_root_int);
	  else
	    break;
	}
      while (1);
    }
  else
    /* No super class.  */
    my_root_id = CLASS_NAME (implementation_template);

  cast_type
    = groktypename (build_tree_list (build_tree_list (NULL_TREE,
						      objc_class_template),
				     build1 (INDIRECT_REF,
					     NULL_TREE, NULL_TREE)));

  name_expr = add_objc_string (CLASS_NAME (implementation_template),
			       class_names);

  /* Install class `isa' and `super' pointers at runtime.  */
  if (my_super_id)
    {
      super_expr = add_objc_string (my_super_id, class_names);
      super_expr = build_c_cast (cast_type, super_expr); /* cast! */
    }
  else
    super_expr = build_int_2 (0, 0);

  root_expr = add_objc_string (my_root_id, class_names);
  root_expr = build_c_cast (cast_type, root_expr); /* cast! */

  if (CLASS_PROTOCOL_LIST (implementation_template))
    {
      generate_protocol_references
	(CLASS_PROTOCOL_LIST (implementation_template));
      protocol_decl = generate_protocol_list (implementation_template);
    }
  else
    protocol_decl = 0;

  /* static struct objc_class _OBJC_METACLASS_Foo = { ... }; */

  sc_spec = build_tree_list (NULL_TREE, ridpointers[(int) RID_STATIC]);
  decl_specs = tree_cons (NULL_TREE, objc_class_template, sc_spec);

  decl = start_decl (DECL_NAME (UOBJC_METACLASS_decl), decl_specs, 1,
		     NULL_TREE);

  initlist
    = build_shared_structure_initializer
      (TREE_TYPE (decl),
       root_expr, super_expr, name_expr,
       convert (integer_type_node, TYPE_SIZE_UNIT (objc_class_template)),
       2 /*CLS_META*/,
       UOBJC_CLASS_METHODS_decl,
       UOBJC_CLASS_VARIABLES_decl,
       protocol_decl);

  finish_decl (decl, initlist, NULL_TREE);

  /* static struct objc_class _OBJC_CLASS_Foo={ ... }; */

  decl = start_decl (DECL_NAME (UOBJC_CLASS_decl), decl_specs, 1,
		     NULL_TREE);

  initlist
    = build_shared_structure_initializer
      (TREE_TYPE (decl),
       build_unary_op (ADDR_EXPR, UOBJC_METACLASS_decl, 0),
       super_expr, name_expr,
       convert (integer_type_node,
		TYPE_SIZE_UNIT (CLASS_STATIC_TEMPLATE
				(implementation_template))),
       1 /*CLS_FACTORY*/,
       UOBJC_INSTANCE_METHODS_decl,
       UOBJC_INSTANCE_VARIABLES_decl,
       protocol_decl);

  finish_decl (decl, initlist, NULL_TREE);
}

static tree
synth_id_with_class_suffix (preamble, ctxt)
     const char *preamble;
     tree ctxt;
{
  char *string;
  if (TREE_CODE (ctxt) == CLASS_IMPLEMENTATION_TYPE
      || TREE_CODE (ctxt) == CLASS_INTERFACE_TYPE)
    {
      const char *const class_name
	= IDENTIFIER_POINTER (CLASS_NAME (objc_implementation_context));
      string = (char *) alloca (strlen (preamble) + strlen (class_name) + 3);
      sprintf (string, "%s_%s", preamble,
	       IDENTIFIER_POINTER (CLASS_NAME (ctxt)));
    }
  else if (TREE_CODE (ctxt) == CATEGORY_IMPLEMENTATION_TYPE
	   || TREE_CODE (ctxt) == CATEGORY_INTERFACE_TYPE)
    {
      /* We have a category.  */
      const char *const class_name
	= IDENTIFIER_POINTER (CLASS_NAME (objc_implementation_context));
      const char *const class_super_name
	= IDENTIFIER_POINTER (CLASS_SUPER_NAME (objc_implementation_context));
      string = (char *) alloca (strlen (preamble)
				+ strlen (class_name)
				+ strlen (class_super_name)
				+ 3);
      sprintf (string, "%s_%s_%s", preamble, class_name, class_super_name);
    }
  else if (TREE_CODE (ctxt) == PROTOCOL_INTERFACE_TYPE)
    {
      const char *protocol_name = IDENTIFIER_POINTER (PROTOCOL_NAME (ctxt));
      string
	= (char *) alloca (strlen (preamble) + strlen (protocol_name) + 3);
      sprintf (string, "%s_%s", preamble, protocol_name);
    }
  else
    abort ();
  
  return get_identifier (string);
}

static int
is_objc_type_qualifier (node)
     tree node;
{
  return (TREE_CODE (node) == IDENTIFIER_NODE
	  && (node == ridpointers [(int) RID_CONST]
	      || node == ridpointers [(int) RID_VOLATILE]
	      || node == ridpointers [(int) RID_IN]
	      || node == ridpointers [(int) RID_OUT]
	      || node == ridpointers [(int) RID_INOUT]
	      || node == ridpointers [(int) RID_BYCOPY]
              || node == ridpointers [(int) RID_BYREF]
	      || node == ridpointers [(int) RID_ONEWAY]));
}

/* If type is empty or only type qualifiers are present, add default
   type of id (otherwise grokdeclarator will default to int).  */

static tree
adjust_type_for_id_default (type)
     tree type;
{
  tree declspecs, chain;

  if (!type)
    return build_tree_list (build_tree_list (NULL_TREE, objc_object_reference),
			    build1 (INDIRECT_REF, NULL_TREE, NULL_TREE));

  declspecs = TREE_PURPOSE (type);

  /* Determine if a typespec is present.  */
  for (chain = declspecs;
       chain;
       chain = TREE_CHAIN (chain))
    {
      if (TYPED_OBJECT (TREE_VALUE (chain))
          && !(TREE_VALUE (type) 
               && TREE_CODE (TREE_VALUE (type)) == INDIRECT_REF))
        error ("can not use an object as parameter to a method\n");
      if (!is_objc_type_qualifier (TREE_VALUE (chain)))
	return type;
    }

  return build_tree_list (tree_cons (NULL_TREE, objc_object_reference,
				     declspecs),
			  build1 (INDIRECT_REF, NULL_TREE, NULL_TREE));
}

/*   Usage:
  		keyworddecl:
  			selector ':' '(' typename ')' identifier
  
     Purpose:
  		Transform an Objective-C keyword argument into
  		the C equivalent parameter declarator.
  
     In:	key_name, an "identifier_node" (optional).
  		arg_type, a  "tree_list" (optional).
  		arg_name, an "identifier_node".
  
     Note:	It would be really nice to strongly type the preceding
  		arguments in the function prototype; however, then I
  		could not use the "accessor" macros defined in "tree.h".
  
     Out:	an instance of "keyword_decl".  */

tree
build_keyword_decl (key_name, arg_type, arg_name)
     tree key_name;
     tree arg_type;
     tree arg_name;
{
  tree keyword_decl;

  /* If no type is specified, default to "id".  */
  arg_type = adjust_type_for_id_default (arg_type);

  keyword_decl = make_node (KEYWORD_DECL);

  TREE_TYPE (keyword_decl) = arg_type;
  KEYWORD_ARG_NAME (keyword_decl) = arg_name;
  KEYWORD_KEY_NAME (keyword_decl) = key_name;

  return keyword_decl;
}

/* Given a chain of keyword_decl's, synthesize the full keyword selector.  */

static tree
build_keyword_selector (selector)
     tree selector;
{
  int len = 0;
  tree key_chain, key_name;
  char *buf;

  /* Scan the selector to see how much space we'll need.  */
  for (key_chain = selector; key_chain; key_chain = TREE_CHAIN (key_chain))
    {
      if (TREE_CODE (selector) == KEYWORD_DECL)
	key_name = KEYWORD_KEY_NAME (key_chain);
      else if (TREE_CODE (selector) == TREE_LIST)
	key_name = TREE_PURPOSE (key_chain);
      else
	abort ();

      if (key_name)
	len += IDENTIFIER_LENGTH (key_name) + 1;
      else
	/* Just a ':' arg.  */
	len++;
    }

  buf = (char *) alloca (len + 1);
  /* Start the buffer out as an empty string.  */
  buf[0] = '\0';

  for (key_chain = selector; key_chain; key_chain = TREE_CHAIN (key_chain))
    {
      if (TREE_CODE (selector) == KEYWORD_DECL)
	key_name = KEYWORD_KEY_NAME (key_chain);
      else if (TREE_CODE (selector) == TREE_LIST)
	/* APPLE LOCAL begin Objective-C++ */
	{
	  key_name = TREE_PURPOSE (key_chain);
	  /* The keyword decl chain will later be used as a function argument
	     chain.  Unhook the selector itself so as to not confuse other
	     parts of the compiler.  */
	  TREE_PURPOSE (key_chain) = NULL_TREE;
	}
	/* APPLE LOCAL end Objective-C++ */
      else
	abort ();

      if (key_name)
	strcat (buf, IDENTIFIER_POINTER (key_name));
      strcat (buf, ":");
    }

  return get_identifier (buf);
}

/* Used for declarations and definitions.  */

tree
build_method_decl (code, ret_type, selector, add_args)
     enum tree_code code;
     tree ret_type;
     tree selector;
     tree add_args;
{
  tree method_decl;

  /* If no type is specified, default to "id".  */
  ret_type = adjust_type_for_id_default (ret_type);

  method_decl = make_node (code);
  TREE_TYPE (method_decl) = ret_type;

  /* If we have a keyword selector, create an identifier_node that
     represents the full selector name (`:' included)...  */
  if (TREE_CODE (selector) == KEYWORD_DECL)
    {
      METHOD_SEL_NAME (method_decl) = build_keyword_selector (selector);
      METHOD_SEL_ARGS (method_decl) = selector;
      METHOD_ADD_ARGS (method_decl) = add_args;
    }
  else
    {
      METHOD_SEL_NAME (method_decl) = selector;
      METHOD_SEL_ARGS (method_decl) = NULL_TREE;
      METHOD_ADD_ARGS (method_decl) = NULL_TREE;
    }

  return method_decl;
}

#define METHOD_DEF 0
#define METHOD_REF 1

/* Used by `build_objc_method_call' and `comp_method_types'.  Return
   an argument list for method METH.  CONTEXT is either METHOD_DEF or
   METHOD_REF, saying whether we are trying to define a method or call
   one.  SUPERFLAG says this is for a send to super; this makes a
   difference for the NeXT calling sequence in which the lookup and
   the method call are done together.  */

static tree
get_arg_type_list (meth, context, superflag)
     tree meth;
     int context;
     int superflag;
{
  tree arglist, akey;

  /* Receiver type.  */
  if (flag_next_runtime && superflag)
    arglist = build_tree_list (NULL_TREE, super_type);
  else if (context == METHOD_DEF)
    arglist = build_tree_list (NULL_TREE, TREE_TYPE (self_decl));
  else
    arglist = build_tree_list (NULL_TREE, id_type);

  /* Selector type - will eventually change to `int'.  */
  chainon (arglist, build_tree_list (NULL_TREE, selector_type));

  /* Build a list of argument types.  */
  for (akey = METHOD_SEL_ARGS (meth); akey; akey = TREE_CHAIN (akey))
    {
      tree arg_decl = groktypename_in_parm_context (TREE_TYPE (akey));
      chainon (arglist, build_tree_list (NULL_TREE, TREE_TYPE (arg_decl)));
    }

  if (METHOD_ADD_ARGS (meth) == objc_ellipsis_node)
    /* We have a `, ...' immediately following the selector,
       finalize the arglist...simulate get_parm_info (0).  */
    ;
  else if (METHOD_ADD_ARGS (meth))
    {
      /* we have a variable length selector */
      tree add_arg_list = TREE_CHAIN (METHOD_ADD_ARGS (meth));
      chainon (arglist, add_arg_list);
    }
  else
    /* finalize the arglist...simulate get_parm_info (1) */
    /* APPLE LOCAL Panther ObjC enhancements  */
    chainon (arglist, OBJC_VOID_AT_END);

  return arglist;
}

static tree
check_duplicates (hsh)
     hash hsh;
{
  tree meth = NULL_TREE;

  if (hsh)
    {
      meth = hsh->key;

      if (hsh->list)
        {
	  /* We have two methods with the same name and different types.  */
	  attr loop;
	  char type = (TREE_CODE (meth) == INSTANCE_METHOD_DECL) ? '-' : '+';

	  warning ("multiple declarations for method `%s'",
		   IDENTIFIER_POINTER (METHOD_SEL_NAME (meth)));

	  warn_with_method ("using", type, meth);
	  for (loop = hsh->list; loop; loop = loop->next)
	    warn_with_method ("also found", type, loop->value);
        }
    }
  return meth;
}

/* If RECEIVER is a class reference, return the identifier node for
   the referenced class.  RECEIVER is created by get_class_reference,
   so we check the exact form created depending on which runtimes are
   used.  */

/* APPLE LOCAL Panther ObjC enhancements */
static tree
receiver_is_class_object (receiver, self, super)
      tree receiver;
      int self, super;
{
  tree chain, exp, arg;

  /* APPLE LOCAL begin Panther ObjC enhancements */
  /* The receiver is 'self' or 'super' in the context of a class method.  */
  if (objc_method_context
      && TREE_CODE (objc_method_context) == CLASS_METHOD_DECL
      && (self || super))
    return (super 
	    ? CLASS_SUPER_NAME (implementation_template)
	    : CLASS_NAME (implementation_template));
  /* APPLE LOCAL end Panther ObjC enhancements */

  /* APPLE LOCAL Panther ObjC enhancements */  
  if (flag_next_runtime)
    {
      /* The receiver is a variable created by
         build_class_reference_decl.  */
      if (TREE_CODE (receiver) == VAR_DECL
	/* APPLE LOCAL begin Panther ObjC enhancements */
	  && TREE_TYPE (TREE_TYPE (receiver)) == TREE_TYPE (objc_class_type))
        /* Look up the identifier.  */
	for (chain = cls_ref_chain; chain; chain = TREE_CHAIN (chain))
	  if (TREE_PURPOSE (chain) == receiver)
            return TREE_VALUE (chain);
	/* APPLE LOCAL end Panther ObjC enhancements */
    }

  /* The receiver is a function call that returns an id.  Check if
     it is a call to objc_getClass, if so, pick up the class name.  */
  if (TREE_CODE (receiver) == CALL_EXPR 
      && (exp = TREE_OPERAND (receiver, 0))
      && TREE_CODE (exp) == ADDR_EXPR
      && (exp = TREE_OPERAND (exp, 0))
      && TREE_CODE (exp) == FUNCTION_DECL
      && exp == objc_get_class_decl
      /* We have a call to objc_getClass!  */
      && (arg = TREE_OPERAND (receiver, 1))
      && TREE_CODE (arg) == TREE_LIST
      && (arg = TREE_VALUE (arg)))
    {
      STRIP_NOPS (arg);
      if (TREE_CODE (arg) == ADDR_EXPR
	  && (arg = TREE_OPERAND (arg, 0))
	  && TREE_CODE (arg) == STRING_CST)
	/* Finally, we have the class name.  */
	return get_identifier (TREE_STRING_POINTER (arg));
    }

  return 0;
}

/* If we are currently building a message expr, this holds
   the identifier of the selector of the message.  This is
   used when printing warnings about argument mismatches.  */

static tree current_objc_message_selector = 0;

tree
objc_message_selector ()
{
  return current_objc_message_selector;
}

/* Construct an expression for sending a message.
   MESS has the object to send to in TREE_PURPOSE
   and the argument list (including selector) in TREE_VALUE.

   (*(<abstract_decl>(*)())_msg)(receiver, selTransTbl[n], ...);
   (*(<abstract_decl>(*)())_msgSuper)(receiver, selTransTbl[n], ...);  */

tree
build_message_expr (mess)
     tree mess;
{
  tree receiver = TREE_PURPOSE (mess);
  tree sel_name;
  tree args = TREE_VALUE (mess);
  tree method_params = NULL_TREE;

  if (TREE_CODE (receiver) == ERROR_MARK)
    return error_mark_node;

  /* Obtain the full selector name.  */
  if (TREE_CODE (args) == IDENTIFIER_NODE)
    /* A unary selector.  */
    sel_name = args;
  else if (TREE_CODE (args) == TREE_LIST)
    sel_name = build_keyword_selector (args);
  else
    abort ();

  /* Build the parameter list to give to the method.  */
  if (TREE_CODE (args) == TREE_LIST)
    {
      tree chain = args, prev = NULL_TREE;

      /* We have a keyword selector--check for comma expressions.  */
      while (chain)
	{
	  tree element = TREE_VALUE (chain);

	  /* We have a comma expression, must collapse...  */
	  if (TREE_CODE (element) == TREE_LIST)
	    {
	      if (prev)
		TREE_CHAIN (prev) = element;
	      else
		args = element;
	    }
	  prev = chain;
	  chain = TREE_CHAIN (chain);
        }
      method_params = args;
    }

/* APPLE LOCAL: Objective-C++ */
#ifdef OBJCPLUS
  if (processing_template_decl)
    /* Must wait until template instantiation time.  */
    return build_min_nt (MESSAGE_SEND_EXPR, receiver, sel_name, 
			 method_params);
#endif

  return finish_message_expr (receiver, sel_name, method_params);
}

/* APPLE LOCAL begin msg send super */
static tree
lookup_method_in_hash_lists (sel_name)
     tree sel_name;
{
  tree method_prototype = check_duplicates 
			  (hash_lookup (nst_method_hash_list, 
					sel_name));
					  
  if (!method_prototype)
    method_prototype = check_duplicates 
		       (hash_lookup (cls_method_hash_list, 
			sel_name));
  return method_prototype;
}    
/* APPLE LOCAL end msg send super */

/* The 'finish_message_expr' routine is called from within
   'build_message_expr' for non-template functions.  In the case of
   C++ template functions, it is called from 'build_expr_from_tree'
   (in decl2.c) after RECEIVER and METHOD_PARAMS have been expanded.  */

tree
finish_message_expr (receiver, sel_name, method_params)
     tree receiver, sel_name, method_params; 
{      
  /* APPLE LOCAL begin Panther ObjC enhancements */
  tree method_prototype = NULL_TREE, rprotos = NULL_TREE, rtype;
  tree selector, retval, is_class;
  int self, super, have_cast;

  /* Extract the receiver of the message, as well as its type
     (where the latter may take the form of a cast or be inferred
     from the implementation context).  */
  rtype = receiver;
  while (TREE_CODE (rtype) == COMPOUND_EXPR
	      || TREE_CODE (rtype) == MODIFY_EXPR
	      || TREE_CODE (rtype) == NOP_EXPR
	      || TREE_CODE (rtype) == COMPONENT_REF)
    rtype = TREE_OPERAND (rtype, 0);
  self = (rtype == self_decl);  
  super = (rtype == UOBJC_SUPER_decl);  
  rtype = TREE_TYPE (receiver);
  have_cast = (TREE_CODE (receiver) == NOP_EXPR
	       || (TREE_CODE (receiver) == COMPOUND_EXPR 
		   && !IS_SUPER (rtype)));

  /* If the receiver is a class object, retrieve the corresponding 
     @interface, if one exists. */
  is_class = receiver_is_class_object (receiver, self, super);
  
  /* Now determine the receiver type (if an explicit cast has not been
     provided).  */
  if (!have_cast)   
    {
      if (is_class)
	rtype = lookup_interface (is_class);
      /* Handle `self' and `super'.  */
      else if (super) 
	{
	  if (!CLASS_SUPER_NAME (implementation_template))
	    {
	      error ("no super class declared in @interface for `%s'",
		     IDENTIFIER_POINTER (CLASS_NAME (implementation_template)));
	      return error_mark_node;
	    }
	  rtype = lookup_interface (CLASS_SUPER_NAME (implementation_template));
	}
      else if (self)
	rtype = lookup_interface (CLASS_NAME (implementation_template));
    }
    
  /* If receiver is of type `id' or `Class' (or if the @interface for a
     class is not visible), we shall be satisfied with the existence of 
     any instance or class method. */
  if (!rtype || IS_ID (rtype) 
	|| TREE_TYPE (rtype) == TREE_TYPE (objc_class_type))
    {
      if (!rtype)
	rtype = xref_tag (RECORD_TYPE, is_class);
      else if (IS_ID (rtype))
	{
	  /* APPLE LOCAL SPEC */
	  rprotos = type_protocol_list (rtype);
	  rtype = NULL_TREE;
	}
      else	
	is_class = TYPE_NAME (rtype) = get_identifier ("Class");

      if (rprotos)
	method_prototype
	  = lookup_method_in_protocol_list (rprotos, sel_name, 
					    is_class != NULL_TREE);
      if (!method_prototype && !rprotos)
	method_prototype
	  = (is_class
	     ? check_duplicates (hash_lookup (cls_method_hash_list, sel_name))
	     : lookup_method_in_hash_lists (sel_name));
    }
  else
    {
      tree orig_rtype = rtype, saved_rtype;
      
      if (TREE_CODE (rtype) == POINTER_TYPE)
	rtype = TREE_TYPE (rtype);
      /* Traverse typedef aliases */
      while (TREE_CODE (rtype) == RECORD_TYPE && TYPE_NAME (rtype)
	     && TREE_CODE (TYPE_NAME (rtype)) == TYPE_DECL
	     && DECL_RESULT (TYPE_NAME (rtype)))
	rtype = DECL_RESULT (TYPE_NAME (rtype));
      saved_rtype = rtype;
      if (TYPED_OBJECT (rtype))
	{
	  /* APPLE LOCAL SPEC */
	  rprotos = type_protocol_list (rtype);
	  rtype = lookup_interface (OBJC_TYPE_NAME (rtype));
	}
      /* If we could not find an @interface declaration, we must have
	 only seen a @class declaration; so, we cannot say anything
	 more intelligent about which methods the receiver will
	 understand. */
      if (!rtype)
	rtype = saved_rtype;
      else if (TREE_CODE (rtype) == CLASS_INTERFACE_TYPE
	  || TREE_CODE (rtype) == CLASS_IMPLEMENTATION_TYPE) 
	{
	  /* We have a valid ObjC class name.  Look up the method name
	     in the published @interface for the class (and its
	     superclasses). */
	  method_prototype 
	    = (is_class
	       ? lookup_class_method_static (rtype, sel_name)
	       : lookup_instance_method_static (rtype, sel_name));

	  /* If the method was not found in the @interface, it may still
	     exist locally as part of the @implementation.  */
	  if (!method_prototype && objc_implementation_context
	     && CLASS_NAME (objc_implementation_context)
		== OBJC_TYPE_NAME (rtype))			
	    method_prototype
	      = lookup_method 
		((is_class
		  ? CLASS_CLS_METHODS (objc_implementation_context)
		  : CLASS_NST_METHODS (objc_implementation_context)),
		  sel_name);

	  /* If we haven't found a candidate method by now, try looking for
	     it in the protocol list.  */
	  if (!method_prototype && rprotos)
	    method_prototype
	      = lookup_method_in_protocol_list (rprotos, sel_name, 
						is_class != NULL_TREE);
	}
      else
	{
	  warning ("invalid receiver type `%s'",
		   gen_declaration (orig_rtype, errbuf));   
	  rtype = NULL_TREE;
	  method_prototype = lookup_method_in_hash_lists (sel_name);
	}
    }	  

  if (!method_prototype)
    {
      if (rtype)
	warning ("`%s' may not respond to `%c%s'",
		 IDENTIFIER_POINTER (OBJC_TYPE_NAME (rtype)),
		 (is_class ? '+' : '-'),
		 IDENTIFIER_POINTER (sel_name));
      if (rprotos)
	warning ("`%c%s' not implemented by protocol(s)",
		 (is_class ? '+' : '-'),
		 IDENTIFIER_POINTER (sel_name));
      if (!(method_prototype = lookup_method_in_hash_lists (sel_name)))
	warning ("cannot find method `%c%s'; return type `id' assumed",
		 (is_class ? '+' : '-'),
		 IDENTIFIER_POINTER (sel_name));
    }
  /* APPLE LOCAL end Panther ObjC enhancements */

  /* Save the selector name for printing error messages.  */
  current_objc_message_selector = sel_name;

  /* Build the parameters list for looking up the method.
     These are the object itself and the selector.  */

  if (flag_typed_selectors)
    selector = build_typed_selector_reference (sel_name, method_prototype);
  else
    selector = build_selector_reference (sel_name);

  retval = build_objc_method_call (super, method_prototype,
				   /* APPLE LOCAL msg send super */
				   receiver,
				   selector, method_params);

  current_objc_message_selector = 0;

  return retval;
}

/* Build a tree expression to send OBJECT the operation SELECTOR,
   looking up the method on object LOOKUP_OBJECT (often same as OBJECT),
   assuming the method has prototype METHOD_PROTOTYPE.
   (That is an INSTANCE_METHOD_DECL or CLASS_METHOD_DECL.)
   Use METHOD_PARAMS as list of args to pass to the method.
   If SUPER_FLAG is nonzero, we look up the superclass's method.  */

static tree
/* APPLE LOCAL begin Panther ObjC enhancements */ 
build_objc_method_call (super_flag, method_prototype, lookup_object,
			selector, method_params)
     int super_flag;
     tree method_prototype, lookup_object, selector, method_params;
{
  tree sender = (super_flag ? umsg_super_decl :
		 flag_nil_receivers ? umsg_decl : umsg_nonnil_decl);
  tree rcv_p = (super_flag ? super_type	: id_type);
  tree object = (super_flag ? self_decl : lookup_object);
  /* APPLE LOCAL end Panther ObjC enhancements */
  
  if (flag_next_runtime)
    {
      /* APPLE LOCAL begin Objective-C++ */
      /* All of the following (except the stret method stuff) is FSF
	 material.  */
      /* If a prototype for the method to be called exists, then cast
	 the sender's arguments and return type to match that of the
	 method.  Otherwise, leave sender as is.  */
      /* APPLE LOCAL begin Panther ObjC enhancements */
      /* If the sender's return type is 'void', treat it as if it were
	 'id' instead.  This is because the sender signature may in
	 fact be incorrect, especially if the type of the receiver is
	 not known.  */
      /* APPLE LOCAL end Panther ObjC enhancements */	 
      tree ret_type = (method_prototype
		       ? groktypename (TREE_TYPE (method_prototype))
		       : id_type);
      tree sender_cast = (method_prototype
			  ? build_pointer_type
			    (build_function_type 
			    /* APPLE LOCAL begin Panther ObjC enhancements */
			     ((ret_type == void_type_node 
			       ? id_type 
			       : ret_type),
			    /* APPLE LOCAL end Panther ObjC enhancements */    
			      get_arg_type_list (method_prototype, 
						 METHOD_REF,
						 super_flag)))
			  : NULL_TREE);
	 
      /* APPLE LOCAL begin objc stret methods */
#ifdef STRUCT_VALUE
      /* If we are returning a struct in memory, and the address
	 of that memory location is passed as a hidden first
	 argument, then change which messenger entry point this
	 expr will call.  NB: Note that sender_cast remains
	 unchanged (it already has a struct return type).  */
      if ((TREE_CODE (ret_type) == RECORD_TYPE
	   || TREE_CODE (ret_type) == UNION_TYPE)
#if defined (DEFAULT_PCC_STRUCT_RETURN) && DEFAULT_PCC_STRUCT_RETURN == 0
	   && RETURN_IN_MEMORY (ret_type)
#endif
	   && STRUCT_VALUE == 0)
	sender = (super_flag ? umsg_super_stret_decl : 
		/* APPLE LOCAL Panther ObjC enhancements */
		flag_nil_receivers ? umsg_stret_decl : umsg_nonnil_stret_decl);
#endif
      /* APPLE LOCAL end objc stret methods */
      /* APPLE LOCAL begin Panther ObjC enhancements */
      method_params = tree_cons (NULL_TREE, 
				 build_c_cast (rcv_p, lookup_object),
				 /* APPLE LOCAL end Panther ObjC enhancements */
				 tree_cons (NULL_TREE, selector,
					    method_params));
      assemble_external (sender);
      /* We want to cast the sender, not convert it.  */
      return build_function_call 
	     (sender_cast ? build_c_cast (sender_cast, sender) : sender, 
	      method_params);
      /* APPLE LOCAL end Objective-C++ */
    }
  else
    {
      /* This is the portable way.
	 First call the lookup function to get a pointer to the method,
	 then cast the pointer, then call it with the method arguments.  */
      tree method;

      /* Avoid trouble since we may evaluate each of these twice.  */
      object = save_expr (object);
      selector = save_expr (selector);

      lookup_object = build_c_cast (rcv_p, lookup_object);

      assemble_external (sender);
      method
	= build_function_call (sender,
			       tree_cons (NULL_TREE, lookup_object,
					  tree_cons (NULL_TREE, selector,
						     NULL_TREE)));

      /* If we have a method prototype, construct the data type this
	 method needs, and cast what we got from SENDER into a pointer
	 to that type.  */
      if (method_prototype)
	{
	  tree arglist = get_arg_type_list (method_prototype, METHOD_REF,
					    super_flag);
	  tree valtype = groktypename (TREE_TYPE (method_prototype));
	  tree fake_function_type = build_function_type (valtype, arglist);
	  TREE_TYPE (method) = build_pointer_type (fake_function_type);
	}
      else
	TREE_TYPE (method)
	  = build_pointer_type (build_function_type (ptr_type_node, NULL_TREE));

      /* Pass the object to the method.  */
      assemble_external (method);
      return build_function_call (method,
				  tree_cons (NULL_TREE, object,
					     tree_cons (NULL_TREE, selector,
							method_params)));
    }
}

static void
build_protocol_reference (p)
     tree p;
{
  tree decl, ident, ptype;

  /* extern struct objc_protocol _OBJC_PROTOCOL_<mumble>; */

  ident = synth_id_with_class_suffix ("_OBJC_PROTOCOL", p);
  ptype
    = groktypename (build_tree_list (build_tree_list (NULL_TREE,
						      objc_protocol_template),
				     NULL_TREE));

  if (IDENTIFIER_GLOBAL_VALUE (ident))
    decl = IDENTIFIER_GLOBAL_VALUE (ident); /* Set by pushdecl.  */
  else
    {
      decl = build_decl (VAR_DECL, ident, ptype);
      DECL_EXTERNAL (decl) = 1;
      /* APPLE LOCAL Objective-C++ */
      TREE_PUBLIC (decl) = 0;
      TREE_USED (decl) = 1;
      DECL_ARTIFICIAL (decl) = 1;

      make_decl_rtl (decl, 0);
      pushdecl_top_level (decl);
   }

  PROTOCOL_FORWARD_DECL (p) = decl;
}

/* This function is called by the parser when (and only when) a
   @protocol() expression is found, in order to compile it.  */
tree
build_protocol_expr (protoname)
     tree protoname;
{
  tree expr;
  tree p = lookup_protocol (protoname);

  if (!p)
    {
      /* APPLE LOCAL begin Panther ObjC enhancements */
      error ("cannot find protocol declaration for `%s'",
	     IDENTIFIER_POINTER (protoname));
      return error_mark_node;
      /* APPLE LOCAL end Panther ObjC enhancements */
    }

  if (!PROTOCOL_FORWARD_DECL (p))
    build_protocol_reference (p);

  expr = build_unary_op (ADDR_EXPR, PROTOCOL_FORWARD_DECL (p), 0);

  TREE_TYPE (expr) = protocol_type;

  /* The @protocol() expression is being compiled into a pointer to a
     statically allocated instance of the Protocol class.  To become
     usable at runtime, the 'isa' pointer of the instance need to be
     fixed up at runtime by the runtime library, to point to the
     actual 'Protocol' class.  */

  /* For the GNU runtime, put the static Protocol instance in the list
     of statically allocated instances, so that we make sure that its
     'isa' pointer is fixed up at runtime by the GNU runtime library
     to point to the Protocol class (at runtime, when loading the
     module, the GNU runtime library loops on the statically allocated
     instances (as found in the defs field in objc_symtab) and fixups
     all the 'isa' pointers of those objects).  */
  if (! flag_next_runtime)
    {
      /* This type is a struct containing the fields of a Protocol
        object.  (Cfr. protocol_type instead is the type of a pointer
        to such a struct).  */
      tree protocol_struct_type = xref_tag 
       (RECORD_TYPE, get_identifier (PROTOCOL_OBJECT_CLASS_NAME));
      tree *chain;
      
      /* Look for the list of Protocol statically allocated instances
        to fixup at runtime.  Create a new list to hold Protocol
        statically allocated instances, if the list is not found.  At
        present there is only another list, holding NSConstantString
        static instances to be fixed up at runtime.  */
      for (chain = &objc_static_instances;
	   *chain && TREE_VALUE (*chain) != protocol_struct_type;
	   chain = &TREE_CHAIN (*chain));
      if (!*chain)
	{
         *chain = tree_cons (NULL_TREE, protocol_struct_type, NULL_TREE);
	 /* APPLE LOCAL type aliasing */
         add_objc_string (OBJC_TYPE_NAME (protocol_struct_type),
                          class_names);
       }
      
      /* Add this statically allocated instance to the Protocol list.  */
      TREE_PURPOSE (*chain) = tree_cons (NULL_TREE, 
					 PROTOCOL_FORWARD_DECL (p),
					 TREE_PURPOSE (*chain));
    }
  

  return expr;
}

/* This function is called by the parser when a @selector() expression
   is found, in order to compile it.  It is only called by the parser
   and only to compile a @selector().  */
tree
build_selector_expr (selnamelist)
     tree selnamelist;
{
  tree selname;

  /* Obtain the full selector name.  */
  if (TREE_CODE (selnamelist) == IDENTIFIER_NODE)
    /* A unary selector.  */
    selname = selnamelist;
  else if (TREE_CODE (selnamelist) == TREE_LIST)
    selname = build_keyword_selector (selnamelist);
  else
    abort ();

  /* If we are required to check @selector() expressions as they
     are found, check that the selector has been declared.  */
  if (warn_undeclared_selector)
    {
      /* Look the selector up in the list of all known class and
         instance methods (up to this line) to check that the selector
         exists.  */
      hash hsh;

      /* First try with instance methods.  */
      hsh = hash_lookup (nst_method_hash_list, selname);
      
      /* If not found, try with class methods.  */
      if (!hsh)
	{
	  hsh = hash_lookup (cls_method_hash_list, selname);
	}
      
      /* If still not found, print out a warning.  */
      if (!hsh)
	{
	  warning ("undeclared selector `%s'", IDENTIFIER_POINTER (selname));
	}
    }
  

  if (flag_typed_selectors)
    return build_typed_selector_reference (selname, 0);
  else
    return build_selector_reference (selname);
}

tree
build_encode_expr (type)
     tree type;
{
  tree result;
  const char *string;

  encode_type (type, obstack_object_size (&util_obstack), 
	       OBJC_ENCODE_INLINE_DEFS);
  obstack_1grow (&util_obstack, 0);    /* null terminate string */
  string = obstack_finish (&util_obstack);

  /* Synthesize a string that represents the encoded struct/union.  */
  result = my_build_string (strlen (string) + 1, string);
  obstack_free (&util_obstack, util_firstobj);
  return result;
}

tree
build_ivar_reference (id)
     tree id;
{
  if (TREE_CODE (objc_method_context) == CLASS_METHOD_DECL)
    {
      /* Historically, a class method that produced objects (factory
	 method) would assign `self' to the instance that it
	 allocated.  This would effectively turn the class method into
	 an instance method.  Following this assignment, the instance
	 variables could be accessed.  That practice, while safe,
	 violates the simple rule that a class method should not refer
	 to an instance variable.  It's better to catch the cases
	 where this is done unknowingly than to support the above
	 paradigm.  */
      warning ("instance variable `%s' accessed in class method",
	       IDENTIFIER_POINTER (id));
      TREE_TYPE (self_decl) = instance_type; /* cast */
    }

  return build_component_ref (build_indirect_ref (self_decl, "->"), id);
}

/* Compute a hash value for a given method SEL_NAME.  */

static size_t
hash_func (sel_name)
     tree sel_name;
{
  const unsigned char *s 
    = (const unsigned char *)IDENTIFIER_POINTER (sel_name);
  size_t h = 0;
  
  while (*s)
    h = h * 67 + *s++ - 113;
  return h;  
}
     
static void
hash_init ()
{
  nst_method_hash_list 
    = (hash *) ggc_alloc_cleared (SIZEHASHTABLE * sizeof (hash));
  cls_method_hash_list 
    = (hash *) ggc_alloc_cleared (SIZEHASHTABLE * sizeof (hash));
}

/* WARNING!!!!  hash_enter is called with a method, and will peek
   inside to find its selector!  But hash_lookup is given a selector
   directly, and looks for the selector that's inside the found
   entry's key (method) for comparison.  */

static void
hash_enter (hashlist, method)
     hash *hashlist;
     tree method;
{
  hash obj;
  int slot = hash_func (METHOD_SEL_NAME (method)) % SIZEHASHTABLE;

  obj = (hash) ggc_alloc (sizeof (struct hashed_entry));
  obj->list = 0;
  obj->next = hashlist[slot];
  obj->key = method;

  hashlist[slot] = obj;		/* append to front */
}

static hash
hash_lookup (hashlist, sel_name)
     hash *hashlist;
     tree sel_name;
{
  hash target;

  target = hashlist[hash_func (sel_name) % SIZEHASHTABLE];

  while (target)
    {
      if (sel_name == METHOD_SEL_NAME (target->key))
	return target;

      target = target->next;
    }
  return 0;
}

static void
hash_add_attr (entry, value)
     hash entry;
     tree value;
{
  attr obj;

  obj = (attr) ggc_alloc (sizeof (struct hashed_attribute));
  obj->next = entry->list;
  obj->value = value;

  entry->list = obj;		/* append to front */
}

static tree
lookup_method (mchain, method)
     tree mchain;
     tree method;
{
  tree key;

  if (TREE_CODE (method) == IDENTIFIER_NODE)
    key = method;
  else
    key = METHOD_SEL_NAME (method);

  while (mchain)
    {
      if (METHOD_SEL_NAME (mchain) == key)
	return mchain;

      mchain = TREE_CHAIN (mchain);
    }
  return NULL_TREE;
}

static tree
lookup_instance_method_static (interface, ident)
     tree interface;
     tree ident;
{
  tree inter = interface;
  tree chain = CLASS_NST_METHODS (inter);
  tree meth = NULL_TREE;

  do
    {
      if ((meth = lookup_method (chain, ident)))
	return meth;

      if (CLASS_CATEGORY_LIST (inter))
	{
	  tree category = CLASS_CATEGORY_LIST (inter);
	  chain = CLASS_NST_METHODS (category);

	  do
	    {
	      if ((meth = lookup_method (chain, ident)))
		return meth;

	      /* Check for instance methods in protocols in categories.  */
	      if (CLASS_PROTOCOL_LIST (category))
		{
		  if ((meth = (lookup_method_in_protocol_list
			       (CLASS_PROTOCOL_LIST (category), ident, 0))))
		    return meth;
		}

	      if ((category = CLASS_CATEGORY_LIST (category)))
		chain = CLASS_NST_METHODS (category);
	    }
	  while (category);
	}

      if (CLASS_PROTOCOL_LIST (inter))
	{
	  if ((meth = (lookup_method_in_protocol_list
		       (CLASS_PROTOCOL_LIST (inter), ident, 0))))
	    return meth;
	}

      if ((inter = lookup_interface (CLASS_SUPER_NAME (inter))))
	chain = CLASS_NST_METHODS (inter);
    }
  while (inter);

  return meth;
}

static tree
lookup_class_method_static (interface, ident)
     tree interface;
     tree ident;
{
  tree inter = interface;
  tree chain = CLASS_CLS_METHODS (inter);
  tree meth = NULL_TREE;
  tree root_inter = NULL_TREE;

  do
    {
      if ((meth = lookup_method (chain, ident)))
	return meth;

      if (CLASS_CATEGORY_LIST (inter))
	{
	  tree category = CLASS_CATEGORY_LIST (inter);
	  chain = CLASS_CLS_METHODS (category);

	  do
	    {
	      if ((meth = lookup_method (chain, ident)))
		return meth;

	      /* Check for class methods in protocols in categories.  */
	      if (CLASS_PROTOCOL_LIST (category))
		{
		  if ((meth = (lookup_method_in_protocol_list
			       (CLASS_PROTOCOL_LIST (category), ident, 1))))
		    return meth;
		}

	      if ((category = CLASS_CATEGORY_LIST (category)))
		chain = CLASS_CLS_METHODS (category);
	    }
	  while (category);
	}

      /* Check for class methods in protocols.  */
      if (CLASS_PROTOCOL_LIST (inter))
	{
	  if ((meth = (lookup_method_in_protocol_list
		       (CLASS_PROTOCOL_LIST (inter), ident, 1))))
	    return meth;
	}

      root_inter = inter;
      if ((inter = lookup_interface (CLASS_SUPER_NAME (inter))))
	chain = CLASS_CLS_METHODS (inter);
    }
  while (inter);

  /* If no class (factory) method was found, check if an _instance_
     method of the same name exists in the root class.  This is what
     the Objective-C runtime will do.  */
  return lookup_instance_method_static (root_inter, ident);
}

tree
add_class_method (class, method)
     tree class;
     tree method;
{
  tree mth;
  hash hsh;

  if (!(mth = lookup_method (CLASS_CLS_METHODS (class), method)))
    {
      /* put method on list in reverse order */
      TREE_CHAIN (method) = CLASS_CLS_METHODS (class);
      CLASS_CLS_METHODS (class) = method;
    }
  else
    {
      if (TREE_CODE (class) == CLASS_IMPLEMENTATION_TYPE)
	error ("duplicate definition of class method `%s'",
	       IDENTIFIER_POINTER (METHOD_SEL_NAME (mth)));
      else
        {
	  /* Check types; if different, complain.  */
	  if (!comp_proto_with_proto (method, mth))
	    error ("duplicate declaration of class method `%s'",
		   IDENTIFIER_POINTER (METHOD_SEL_NAME (mth)));
        }
    }

  if (!(hsh = hash_lookup (cls_method_hash_list, METHOD_SEL_NAME (method))))
    {
      /* Install on a global chain.  */
      hash_enter (cls_method_hash_list, method);
    }
  else
    {
      /* Check types; if different, add to a list.  */
      if (!comp_proto_with_proto (method, hsh->key))
        hash_add_attr (hsh, method);
    }

  /* APPLE LOCAL begin indexing dpatel */
  if (flag_gen_index 
      && (TREE_CODE (class) == CLASS_INTERFACE_TYPE 
	  || TREE_CODE (class) == CATEGORY_INTERFACE_TYPE
	  || TREE_CODE (class) == PROTOCOL_INTERFACE_TYPE))
    gen_indexing_info (INDEX_CLASS_METHOD_DECL,
		       IDENTIFIER_POINTER (DECL_NAME (method)),
		       DECL_SOURCE_LINE (method) /*lineno*/ );
  /* APPLE LOCAL end indexing dpatel */
  return method;
}

tree
add_instance_method (class, method)
     tree class;
     tree method;
{
  tree mth;
  hash hsh;

  if (!(mth = lookup_method (CLASS_NST_METHODS (class), method)))
    {
      /* Put method on list in reverse order.  */
      TREE_CHAIN (method) = CLASS_NST_METHODS (class);
      CLASS_NST_METHODS (class) = method;
    }
  else
    {
      if (TREE_CODE (class) == CLASS_IMPLEMENTATION_TYPE)
	error ("duplicate definition of instance method `%s'",
	       IDENTIFIER_POINTER (METHOD_SEL_NAME (mth)));
      else
        {
	  /* Check types; if different, complain.  */
	  if (!comp_proto_with_proto (method, mth))
	    error ("duplicate declaration of instance method `%s'",
		   IDENTIFIER_POINTER (METHOD_SEL_NAME (mth)));
        }
    }

  if (!(hsh = hash_lookup (nst_method_hash_list, METHOD_SEL_NAME (method))))
    {
      /* Install on a global chain.  */
      hash_enter (nst_method_hash_list, method);
    }
  else
    {
      /* Check types; if different, add to a list.  */
      if (!comp_proto_with_proto (method, hsh->key))
        hash_add_attr (hsh, method);
    }
  /* APPLE LOCAL begin indexing dpatel */
  if (flag_gen_index 
      && (TREE_CODE (class) == CLASS_INTERFACE_TYPE 
	  || TREE_CODE (class) == CATEGORY_INTERFACE_TYPE
	  || TREE_CODE (class) == PROTOCOL_INTERFACE_TYPE))
    gen_indexing_info (INDEX_INSTANCE_METHOD_DECL,
		       IDENTIFIER_POINTER (DECL_NAME (method)),
		       lineno);
  /* APPLE LOCAL end indexing dpatel */
  return method;
}

/* APPLE begin LOCAL objc speedup dpatel */
/* New parameter, name */
static tree
add_class (class, name)
     tree class;
     tree name;
{
  /* APPLE LOCAL objc speedup dpatel */
  IDENTIFIER_INTERFACE_VALUE (name) = class;

  /* Put interfaces on list in reverse order.  */
  TREE_CHAIN (class) = interface_chain;
  interface_chain = class;
  return interface_chain;
}

static void
add_category (class, category)
      tree class;
      tree category;
{
  /* Put categories on list in reverse order.  */
  tree cat = CLASS_CATEGORY_LIST (class);

  while (cat)
    {
      if (CLASS_SUPER_NAME (cat) == CLASS_SUPER_NAME (category))
	/* APPLE LOCAL begin Objective-C++ */
#ifdef OBJCPLUS
	error ("duplicate interface declaration for category `%s(%s)'",
#else	
	warning ("duplicate interface declaration for category `%s(%s)'",
#endif	
	/* APPLE LOCAL end Objective-C++ */
		 IDENTIFIER_POINTER (CLASS_NAME (class)),
		 IDENTIFIER_POINTER (CLASS_SUPER_NAME (category)));
      cat = CLASS_CATEGORY_LIST (cat);
    }

  CLASS_CATEGORY_LIST (category) = CLASS_CATEGORY_LIST (class);
  CLASS_CATEGORY_LIST (class) = category;
}

/* Called after parsing each instance variable declaration. Necessary to
   preserve typedefs and implement public/private...

   PUBLIC is 1 for public, 0 for protected, and 2 for private.  */

tree
add_instance_variable (class, public, declarator, declspecs, width)
     tree class;
     int public;
     tree declarator;
     tree declspecs;
     tree width;
{
  /* APPLE LOCAL begin type size */
  tree field_decl, field_type, raw_decl;
  const char *ivar_name;
  /* APPLE LOCAL end type size */

  field_decl = grokfield (input_filename, lineno,
			  declarator, declspecs, width);

  /* APPLE LOCAL begin type size */
  field_type = TREE_TYPE (field_decl);
  ivar_name = declarator
	      ? (IDENTIFIER_POINTER (TREE_CODE (declarator) == IDENTIFIER_NODE
		 ? declarator 
		 : TREE_OPERAND (declarator, 0)))
	      : "<unnamed>";
  /* APPLE LOCAL end type size */

  /* APPLE LOCAL begin Objective-C++  */
#ifdef OBJCPLUS
  if (TREE_CODE (field_type) == REFERENCE_TYPE)
    {
      error ("illegal reference type specified for instance variable `%s'", 
	     ivar_name);
      /* Return class as is without adding this ivar.  */
      return class;
    }
#endif
  /* APPLE LOCAL end Objective-C++ */

  /* APPLE LOCAL begin type size */
  if (field_type == error_mark_node || !TYPE_SIZE (field_type)
      || TYPE_SIZE (field_type) == error_mark_node
      /* 'type[0]' is allowed, but 'type[]' is not! */
#ifdef OBJCPLUS
      || (TYPE_SIZE (field_type) == bitsize_zero_node
	  && !TREE_OPERAND (declarator, 1)) 
#endif
      )
    {
      error ("instance variable `%s' has unknown size", ivar_name);
      /* Return class as is without adding this ivar.  */
      return class;
    }
  /* APPLE LOCAL end type size */

  /* APPLE LOCAL begin Objective-C++  */
#ifdef OBJCPLUS
  /* zlaski 2001-Apr-24: C++ classes with non-trivial constructors and/or destructors
     cannot be ivars; ditto for classes with vtables. */
  if(IS_AGGR_TYPE (field_type) && (TYPE_NEEDS_CONSTRUCTING (field_type) 
      || TYPE_HAS_NONTRIVIAL_DESTRUCTOR (field_type) || TYPE_POLYMORPHIC_P (field_type))) 
    {
      const char *type_name = IDENTIFIER_POINTER (OBJC_TYPE_NAME (field_type));
      if(TYPE_POLYMORPHIC_P (field_type)) {
        /* vtable pointers are Real Bad(tm), since Obj-C cannot initialize them */ 
        error ("type `%s' has virtual member functions", type_name);
        error ("illegal aggregate type `%s' specified for instance variable `%s'", 
  	       type_name, ivar_name);
        /* Return class as is without adding this ivar.  */
        return class;
      }         
      /* user-defined constructors and destructors are not known to Obj-C and
         hence will not be called.  This may or may not be a problem. */
      if (TYPE_NEEDS_CONSTRUCTING (field_type)) 
        warning ("type `%s' has a user-defined constructor", type_name);
      if (TYPE_HAS_NONTRIVIAL_DESTRUCTOR (field_type))
        warning ("type `%s' has a user-defined destructor", type_name);
      warning ("C++ constructors and destructors will not be invoked for Objective-C fields");               
    }     
#endif
  /* APPLE LOCAL end Objective-C++  */
  
  /* Overload the public attribute, it is not used for FIELD_DECLs.  */
  switch (public)
    {
    case 0:
      TREE_PUBLIC (field_decl) = 0;
      TREE_PRIVATE (field_decl) = 0;
      TREE_PROTECTED (field_decl) = 1;
      break;

    case 1:
      TREE_PUBLIC (field_decl) = 1;
      TREE_PRIVATE (field_decl) = 0;
      TREE_PROTECTED (field_decl) = 0;
      break;

    case 2:
      TREE_PUBLIC (field_decl) = 0;
      TREE_PRIVATE (field_decl) = 1;
      TREE_PROTECTED (field_decl) = 0;
      break;

    }

  /* APPLE LOCAL begin objc type size */
  raw_decl = build_tree_list (declspecs, build_tree_list (declarator, width));
  CLASS_RAW_IVARS (class) = chainon (CLASS_RAW_IVARS (class), raw_decl);
  CLASS_IVARS (class) = chainon (CLASS_IVARS (class), field_decl);
  /* APPLE LOCAL end objc type size */

  /* APPLE LOCAL begin indexing dpatel */
  if (flag_gen_index)
    gen_indexing_info (INDEX_DATA_DECL,
		       IDENTIFIER_POINTER (DECL_NAME (field_decl)),
		       lineno);
  /* APPLE LOCAL end indexing dpatel */
  return class;
}

tree
is_ivar (decl_chain, ident)
     tree decl_chain;
     tree ident;
{
  for ( ; decl_chain; decl_chain = TREE_CHAIN (decl_chain))
    if (DECL_NAME (decl_chain) == ident)
      return decl_chain;
  return NULL_TREE;
}

/* True if the ivar is private and we are not in its implementation.  */

int
is_private (decl)
     tree decl;
{
  if (TREE_PRIVATE (decl)
      && ! is_ivar (CLASS_IVARS (implementation_template), DECL_NAME (decl)))
    {
      error ("instance variable `%s' is declared private",
	     IDENTIFIER_POINTER (DECL_NAME (decl)));
      return 1;
    }
  else
    return 0;
}

/* We have an instance variable reference;, check to see if it is public.  */

int
is_public (expr, identifier)
     tree expr;
     tree identifier;
{
  tree basetype = TREE_TYPE (expr);
  enum tree_code code = TREE_CODE (basetype);
  tree decl;

  if (code == RECORD_TYPE)
    {
      if (TREE_STATIC_TEMPLATE (basetype))
	{
	  /* APPLE LOCAL type aliasing */
	  if (!lookup_interface (OBJC_TYPE_NAME (basetype)))
	    {
	      error ("cannot find interface declaration for `%s'",
		     /* APPLE LOCAL type aliasing */
		     IDENTIFIER_POINTER (OBJC_TYPE_NAME (basetype)));
	      return 0;
	    }

	  if ((decl = is_ivar (TYPE_FIELDS (basetype), identifier)))
	    {
	      if (TREE_PUBLIC (decl))
		return 1;

	      /* Important difference between the Stepstone translator:
		 all instance variables should be public within the context
		 of the implementation.  */
	      if (objc_implementation_context
		  && (((TREE_CODE (objc_implementation_context)
			== CLASS_IMPLEMENTATION_TYPE)
		       || (TREE_CODE (objc_implementation_context)
			   == CATEGORY_IMPLEMENTATION_TYPE))
		      && (CLASS_NAME (objc_implementation_context)
			  /* APPLE LOCAL type aliasing */
			  == OBJC_TYPE_NAME (basetype))))
		return ! is_private (decl);

	      /* APPLE LOCAL begin ivar access */
	      /* The 2.95.2 compiler sometimes allowed C functions to access
		 non-@public ivars.  We will let this slide for now...  */
	      if (!objc_method_context)
	      {
		warning ("instance variable `%s' is %s; "
			 "this will be a hard error in the future",
			 IDENTIFIER_POINTER (identifier),
			 TREE_PRIVATE (decl) ? "@private" : "@protected");
		return 1;
	      }
	      /* APPLE LOCAL end ivar access */
		 
	      error ("instance variable `%s' is declared %s",
		     IDENTIFIER_POINTER (identifier),
		     TREE_PRIVATE (decl) ? "private" : "protected");
	      return 0;
	    }
	}

      else if (objc_implementation_context && (basetype == objc_object_reference))
	{
	  TREE_TYPE (expr) = uprivate_record;
	  warning ("static access to object of type `id'");
	}
    }

  return 1;
}

/* Make sure all entries in CHAIN are also in LIST.  */

static int
check_methods (chain, list, mtype)
     tree chain;
     tree list;
     int mtype;
{
  int first = 1;

  while (chain)
    {
      if (!lookup_method (list, chain))
	{
	  if (first)
	    {
	      if (TREE_CODE (objc_implementation_context)
		  == CLASS_IMPLEMENTATION_TYPE)
		warning ("incomplete implementation of class `%s'",
			 IDENTIFIER_POINTER (CLASS_NAME (objc_implementation_context)));
	      else if (TREE_CODE (objc_implementation_context)
		       == CATEGORY_IMPLEMENTATION_TYPE)
		warning ("incomplete implementation of category `%s'",
			 IDENTIFIER_POINTER (CLASS_SUPER_NAME (objc_implementation_context)));
	      first = 0;
	    }

	  warning ("method definition for `%c%s' not found",
		   mtype, IDENTIFIER_POINTER (METHOD_SEL_NAME (chain)));
	}

      chain = TREE_CHAIN (chain);
    }

    return first;
}

/* Check if CLASS, or its superclasses, explicitly conforms to PROTOCOL.  */

static int
conforms_to_protocol (class, protocol)
     tree class;
     tree protocol;
{
   if (TREE_CODE (protocol) == PROTOCOL_INTERFACE_TYPE)
     {
       tree p = CLASS_PROTOCOL_LIST (class);
       while (p && TREE_VALUE (p) != protocol)
	 p = TREE_CHAIN (p);

       if (!p)
	 {
	   tree super = (CLASS_SUPER_NAME (class)
			 ? lookup_interface (CLASS_SUPER_NAME (class))
			 : NULL_TREE);
	   int tmp = super ? conforms_to_protocol (super, protocol) : 0;
	   if (!tmp)
	     return 0;
	 }
     }

   return 1;
}

/* Make sure all methods in CHAIN are accessible as MTYPE methods in 
   CONTEXT.  This is one of two mechanisms to check protocol integrity.  */

static int
check_methods_accessible (chain, context, mtype)
     tree chain;
     tree context;
     int mtype;
{
  int first = 1;
  tree list;
  tree base_context = context;

  while (chain)
    {
      context = base_context;
      while (context)
	{
	  if (mtype == '+')
	    list = CLASS_CLS_METHODS (context);
	  else
	    list = CLASS_NST_METHODS (context);

	  if (lookup_method (list, chain))
	      break; 

	  else if (TREE_CODE (context) == CLASS_IMPLEMENTATION_TYPE
		   || TREE_CODE (context) == CLASS_INTERFACE_TYPE)
	    context = (CLASS_SUPER_NAME (context) 
		       ? lookup_interface (CLASS_SUPER_NAME (context))
		       : NULL_TREE);

	  else if (TREE_CODE (context) == CATEGORY_IMPLEMENTATION_TYPE
		   || TREE_CODE (context) == CATEGORY_INTERFACE_TYPE)
	    context = (CLASS_NAME (context) 
		       ? lookup_interface (CLASS_NAME (context))
		       : NULL_TREE);
	  else
	    abort ();
	}

      if (context == NULL_TREE)
	{
	  if (first)
	    {
	      if (TREE_CODE (objc_implementation_context)
		  == CLASS_IMPLEMENTATION_TYPE)
		warning ("incomplete implementation of class `%s'",
			 IDENTIFIER_POINTER
			   (CLASS_NAME (objc_implementation_context)));
	      else if (TREE_CODE (objc_implementation_context)
		       == CATEGORY_IMPLEMENTATION_TYPE)
		warning ("incomplete implementation of category `%s'",
			 IDENTIFIER_POINTER
			   (CLASS_SUPER_NAME (objc_implementation_context)));
	      first = 0;
	    }
	  warning ("method definition for `%c%s' not found",
		   mtype, IDENTIFIER_POINTER (METHOD_SEL_NAME (chain)));
	}

      chain = TREE_CHAIN (chain); /* next method...  */
    }
  return first;
}

/* Check whether the current interface (accessible via
   'objc_implementation_context') actually implements protocol P, along
   with any protocols that P inherits.  */
   
static void
check_protocol (p, type, name)
     tree p;
     const char *type;
     const char *name;
{
  if (TREE_CODE (p) == PROTOCOL_INTERFACE_TYPE)
    {
      int f1, f2;

      /* Ensure that all protocols have bodies!  */
      if (warn_protocol)
	{
	  f1 = check_methods (PROTOCOL_CLS_METHODS (p),
			      CLASS_CLS_METHODS (objc_implementation_context),
			      '+');
	  f2 = check_methods (PROTOCOL_NST_METHODS (p),
			      CLASS_NST_METHODS (objc_implementation_context),
			      '-');
	}
      else
	{
	  f1 = check_methods_accessible (PROTOCOL_CLS_METHODS (p),
					 objc_implementation_context,
					 '+');
	  f2 = check_methods_accessible (PROTOCOL_NST_METHODS (p),
					 objc_implementation_context,
					 '-');
	}

      if (!f1 || !f2)
	warning ("%s `%s' does not fully implement the `%s' protocol",
		 type, name, IDENTIFIER_POINTER (PROTOCOL_NAME (p)));
    }
    
  /* Check protocols recursively.  */
  if (PROTOCOL_LIST (p))
    {
      tree subs = PROTOCOL_LIST (p);
      tree super_class =
	lookup_interface (CLASS_SUPER_NAME (implementation_template));

      while (subs) 
	{
	  tree sub = TREE_VALUE (subs);

	  /* If the superclass does not conform to the protocols
	     inherited by P, then we must!  */
	  if (!super_class || !conforms_to_protocol (super_class, sub))
	    check_protocol (sub, type, name);
	  subs = TREE_CHAIN (subs);
	}
    }
}
	
/* Check whether the current interface (accessible via
   'objc_implementation_context') actually implements the protocols listed
   in PROTO_LIST.  */
   
static void
check_protocols (proto_list, type, name)
     tree proto_list;
     const char *type;
     const char *name;
{
  for ( ; proto_list; proto_list = TREE_CHAIN (proto_list))
    {
      tree p = TREE_VALUE (proto_list);

      check_protocol (p, type, name);
    }
}

/* Make sure that the class CLASS_NAME is defined
   CODE says which kind of thing CLASS_NAME ought to be.
   It can be CLASS_INTERFACE_TYPE, CLASS_IMPLEMENTATION_TYPE,
   CATEGORY_INTERFACE_TYPE, or CATEGORY_IMPLEMENTATION_TYPE.  */

tree
start_class (code, class_name, super_name, protocol_list)
     enum tree_code code;
     tree class_name;
     tree super_name;
     tree protocol_list;
{
  tree class, decl;

  /* APPLE LOCAL Objective-C++ */
#ifdef OBJCPLUS
  /* APPLE LOCAL begin indexing */
  /* Now processing Objective-C construct.  */
  set_index_lang (PB_INDEX_LANG_OBJC);
  /* APPLE LOCAL end indexing */

  if (current_namespace != global_namespace) {
    error ("Objective-C declarations may only appear in global scope");
  }
#endif /* OBJCPLUS */

  if (objc_implementation_context)
    {
      warning ("`@end' missing in implementation context");
      finish_class (objc_implementation_context);
      objc_ivar_chain = NULL_TREE;
      objc_implementation_context = NULL_TREE;
    }

  class = make_node (code);
  TYPE_BINFO (class) = make_tree_vec (6);

  CLASS_NAME (class) = class_name;
  CLASS_SUPER_NAME (class) = super_name;
  CLASS_CLS_METHODS (class) = NULL_TREE;

  /* APPLE LOCAL Objective-C++ */
  if (! is_class_name (class_name) 
      /* APPLE LOCAL Panther ObjC enhancements */
      && (decl = lookup_name (class_name)))
    {
      error ("`%s' redeclared as different kind of symbol",
	     IDENTIFIER_POINTER (class_name));
      error_with_decl (decl, "previous declaration of `%s'");
    }

  if (code == CLASS_IMPLEMENTATION_TYPE)
    {
      {
        tree chain;

        for (chain = implemented_classes; chain; chain = TREE_CHAIN (chain))
           if (TREE_VALUE (chain) == class_name)
	     {
	       error ("reimplementation of class `%s'",
		      IDENTIFIER_POINTER (class_name));
	       return error_mark_node;
	     }
        implemented_classes = tree_cons (NULL_TREE, class_name,
					 implemented_classes);
      }

      /* APPLE LOCAL Objective-C++ */
      /* Moved initializations to synth_module_prologue.  */
      
      /* Reset for multiple classes per file.  */
      method_slot = 0;

      objc_implementation_context = class;

      /* Lookup the interface for this implementation.  */

      if (!(implementation_template = lookup_interface (class_name)))
        {
	  warning ("cannot find interface declaration for `%s'",
		   IDENTIFIER_POINTER (class_name));
	  /* APPLE LOCAL objc speedup dpatel */
	  /* Add second parameter class_name */
	  add_class (implementation_template = objc_implementation_context, class_name);
        }

      /* If a super class has been specified in the implementation,
	 insure it conforms to the one specified in the interface.  */

      if (super_name
	  && (super_name != CLASS_SUPER_NAME (implementation_template)))
        {
	  tree previous_name = CLASS_SUPER_NAME (implementation_template);
          const char *const name =
	    previous_name ? IDENTIFIER_POINTER (previous_name) : "";
	  error ("conflicting super class name `%s'",
		 IDENTIFIER_POINTER (super_name));
	  error ("previous declaration of `%s'", name);
        }

      else if (! super_name)
	{
	  CLASS_SUPER_NAME (objc_implementation_context) 
	    = CLASS_SUPER_NAME (implementation_template);
	}
    }

  else if (code == CLASS_INTERFACE_TYPE)
    {
      if (lookup_interface (class_name))
	/* APPLE LOCAL begin Objective-C++ */
#ifdef OBJCPLUS
	error ("duplicate interface declaration for class `%s'",
#else
	warning ("duplicate interface declaration for class `%s'",
#endif	
	/* APPLE LOCAL end Objective-C++ */
        IDENTIFIER_POINTER (class_name));
	else
	/* APPLE LOCAL objc speedup dpatel */
	/* Add second parameter, class_name */
          add_class (class, class_name);

      if (protocol_list)
	CLASS_PROTOCOL_LIST (class)
	  = lookup_and_install_protocols (protocol_list);
    }

  else if (code == CATEGORY_INTERFACE_TYPE)
    {
      tree class_category_is_assoc_with;

      /* For a category, class_name is really the name of the class that
	 the following set of methods will be associated with. We must
	 find the interface so that can derive the objects template.  */

      if (!(class_category_is_assoc_with = lookup_interface (class_name)))
	{
	  error ("cannot find interface declaration for `%s'",
		 IDENTIFIER_POINTER (class_name));
	  exit (FATAL_EXIT_CODE);
	}
      else
        add_category (class_category_is_assoc_with, class);

      if (protocol_list)
	CLASS_PROTOCOL_LIST (class)
	  = lookup_and_install_protocols (protocol_list);
    }

  else if (code == CATEGORY_IMPLEMENTATION_TYPE)
    {
      /* APPLE LOCAL Objective-C++ */
      /* Moved initializations to synth_module_prologue.  */
      
      /* Reset for multiple classes per file.  */
      method_slot = 0;

      objc_implementation_context = class;

      /* For a category, class_name is really the name of the class that
	 the following set of methods will be associated with.  We must
	 find the interface so that can derive the objects template.  */

      if (!(implementation_template = lookup_interface (class_name)))
        {
	  error ("cannot find interface declaration for `%s'",
		 IDENTIFIER_POINTER (class_name));
	  exit (FATAL_EXIT_CODE);
        }
    }

  /* APPLE LOCAL begin indexing dpatel */
  if (flag_gen_index)
    {
      int info_tag;

      switch (code)
	{
	  case CLASS_INTERFACE_TYPE:
	    info_tag = INDEX_CLASS_DECL;
	    break;
	  case CLASS_IMPLEMENTATION_TYPE:
	    info_tag = INDEX_CLASS_BEGIN;
	    break;
	  case CATEGORY_INTERFACE_TYPE:
	    info_tag = INDEX_CATEGORY_DECL;
	    break;
	  case CATEGORY_IMPLEMENTATION_TYPE:
	    info_tag = INDEX_CATEGORY_BEGIN;
	    break;
	  default:
	    info_tag = INDEX_ERROR;
	    break;
	}
      if (info_tag == INDEX_CATEGORY_DECL || info_tag == INDEX_CATEGORY_BEGIN)
	{
	  /* In case of categories, super_name is the category name.  */
          gen_indexing_info (info_tag, IDENTIFIER_POINTER(super_name),
			     lineno);
          gen_indexing_info (INDEX_CLASS_INHERITANCE, IDENTIFIER_POINTER(class_name),
			     lineno);
	}
      else
	{
          gen_indexing_info (info_tag, IDENTIFIER_POINTER(class_name),
			     lineno);
          /* Add inherited classes, if applicable.  */
          if (super_name != NULL)
            gen_indexing_info (INDEX_CLASS_INHERITANCE, 
			       IDENTIFIER_POINTER(super_name),
			       lineno);
	}
      /* Add adopted protocols.  */
      if (protocol_list)
	{
	  tree pnode = protocol_list;
	  for (; pnode; pnode = TREE_CHAIN (pnode))
	    {
	      tree pdecl = TREE_VALUE (pnode);
	      gen_indexing_info (INDEX_PROTOCOL_INHERITANCE,
				 /* APPLE LOCAL type aliasing */
				 IDENTIFIER_POINTER (OBJC_TYPE_NAME (pdecl)),
				 0);
	    }
	}
    }
  /* APPLE LOCAL end indexing dpatel */
  return class;
}

tree
continue_class (class)
     tree class;
{
  if (TREE_CODE (class) == CLASS_IMPLEMENTATION_TYPE
      || TREE_CODE (class) == CATEGORY_IMPLEMENTATION_TYPE)
    {
      struct imp_entry *imp_entry;
      tree ivar_context;

      /* Check consistency of the instance variables.  */

      if (CLASS_IVARS (class))
	check_ivars (implementation_template, class);

      /* code generation */

      /* APPLE LOCAL Objective-C++ */
#ifdef OBJCPLUS
      push_lang_context (lang_name_c);
#endif

      ivar_context = build_private_template (implementation_template);

      if (!objc_class_template)
	build_class_template ();

      imp_entry = (struct imp_entry *) ggc_alloc (sizeof (struct imp_entry));

      imp_entry->next = imp_list;
      imp_entry->imp_context = class;
      imp_entry->imp_template = implementation_template;

      synth_forward_declarations ();
      imp_entry->class_decl = UOBJC_CLASS_decl;
      imp_entry->meta_decl = UOBJC_METACLASS_decl;

      /* Append to front and increment count.  */
      imp_list = imp_entry;
      if (TREE_CODE (class) == CLASS_IMPLEMENTATION_TYPE)
	imp_count++;
      else
	cat_count++;

      /* APPLE LOCAL Objective-C++  */
#ifdef OBJCPLUS
      pop_lang_context ();
#endif /* OBJCPLUS */

      return ivar_context;
    }

  else if (TREE_CODE (class) == CLASS_INTERFACE_TYPE)
    {
      /* APPLE LOCAL begin Objective-C++ */
      /* Some of this should go back to the FSF... */

#ifdef OBJCPLUS
      push_lang_context (lang_name_c);
#endif /* OBJCPLUS */
      if (!CLASS_STATIC_TEMPLATE (class))
	{
	  tree record = start_struct (RECORD_TYPE, CLASS_NAME (class));
	  finish_struct (record, get_class_ivars (class, 0), NULL_TREE);
	  /* APPLE LOCAL end Objective-C++ */
	  CLASS_STATIC_TEMPLATE (class) = record;

	  /* Mark this record as a class template for static typing.  */
	  TREE_STATIC_TEMPLATE (record) = 1;
	}

    /* APPLE LOCAL Objective-C++ */
#ifdef OBJCPLUS
      pop_lang_context ();
#endif /* OBJCPLUS */
      return NULL_TREE;
    }

  else
    return error_mark_node;
}

/* This is called once we see the "@end" in an interface/implementation.  */

void
finish_class (class)
     tree class;
{
  /* APPLE LOCAL begin indexing dpatel */
  if (flag_gen_index)
    {
      int info_tag;

      switch (TREE_CODE (class))
	{
	  case CLASS_INTERFACE_TYPE:
	    info_tag = INDEX_CLASS_DECL_END;
	    break;
	  case CLASS_IMPLEMENTATION_TYPE:
	    info_tag = INDEX_CLASS_END;
	    break;
	  case CATEGORY_INTERFACE_TYPE:
	    info_tag = INDEX_CATEGORY_DECL_END;
	    break;
	  case CATEGORY_IMPLEMENTATION_TYPE:
	    info_tag = INDEX_CATEGORY_END;
	    break;
	  default:
	    info_tag = INDEX_ERROR;
	    break;
	}
      gen_indexing_info (info_tag, NULL, lineno);
    }
  flag_suppress_builtin_indexing = 1;
  /* APPLE LOCAL end indexing dpatel */

  if (TREE_CODE (class) == CLASS_IMPLEMENTATION_TYPE)
    {
      /* All code generation is done in finish_objc.  */

      if (implementation_template != objc_implementation_context)
	{
	  /* Ensure that all method listed in the interface contain bodies.  */
	  check_methods (CLASS_CLS_METHODS (implementation_template),
			 CLASS_CLS_METHODS (objc_implementation_context), '+');
	  check_methods (CLASS_NST_METHODS (implementation_template),
			 CLASS_NST_METHODS (objc_implementation_context), '-');

	  if (CLASS_PROTOCOL_LIST (implementation_template))
	    check_protocols (CLASS_PROTOCOL_LIST (implementation_template),
			     "class",
			     IDENTIFIER_POINTER (CLASS_NAME (objc_implementation_context)));
	}
    }

  else if (TREE_CODE (class) == CATEGORY_IMPLEMENTATION_TYPE)
    {
      tree category = CLASS_CATEGORY_LIST (implementation_template);

      /* Find the category interface from the class it is associated with.  */
      while (category)
	{
	  if (CLASS_SUPER_NAME (class) == CLASS_SUPER_NAME (category))
	    break;
	  category = CLASS_CATEGORY_LIST (category);
	}

      if (category)
	{
	  /* Ensure all method listed in the interface contain bodies.  */
	  check_methods (CLASS_CLS_METHODS (category),
			 CLASS_CLS_METHODS (objc_implementation_context), '+');
	  check_methods (CLASS_NST_METHODS (category),
			 CLASS_NST_METHODS (objc_implementation_context), '-');

	  if (CLASS_PROTOCOL_LIST (category))
	    check_protocols (CLASS_PROTOCOL_LIST (category),
			     "category",
			     IDENTIFIER_POINTER (CLASS_SUPER_NAME (objc_implementation_context)));
	}
    }

  else if (TREE_CODE (class) == CLASS_INTERFACE_TYPE)
    {
      tree decl_specs;
      const char *class_name = IDENTIFIER_POINTER (CLASS_NAME (class));
      char *string = (char *) alloca (strlen (class_name) + 3);

      /* extern struct objc_object *_<my_name>; */

      sprintf (string, "_%s", class_name);

      decl_specs = build_tree_list (NULL_TREE, ridpointers[(int) RID_EXTERN]);
      decl_specs = tree_cons (NULL_TREE, objc_object_reference, decl_specs);
      define_decl (build1 (INDIRECT_REF, NULL_TREE, get_identifier (string)),
		   decl_specs);
    }
  /* APPLE LOCAL indexing dpatel */
  flag_suppress_builtin_indexing = 0;
}

static tree
add_protocol (protocol)
     tree protocol;
{
  /* Put protocol on list in reverse order.  */
  TREE_CHAIN (protocol) = protocol_chain;
  protocol_chain = protocol;
  return protocol_chain;
}

static tree
lookup_protocol (ident)
     tree ident;
{
  tree chain;

  for (chain = protocol_chain; chain; chain = TREE_CHAIN (chain))
    if (ident == PROTOCOL_NAME (chain))
      return chain;

  return NULL_TREE;
}

/* This function forward declares the protocols named by NAMES.  If
   they are already declared or defined, the function has no effect.  */

void
objc_declare_protocols (names)
     tree names;
{
  tree list;

  /* APPLE LOCAL Objective-C++ */
#ifdef OBJCPLUS
  if (current_namespace != global_namespace) {
    error ("Objective-C declarations may only appear in global scope");
  }
#endif /* OBJCPLUS */
  
  for (list = names; list; list = TREE_CHAIN (list))
    {
      tree name = TREE_VALUE (list);

      if (lookup_protocol (name) == NULL_TREE)
	{
	  tree protocol = make_node (PROTOCOL_INTERFACE_TYPE);

	  TYPE_BINFO (protocol) = make_tree_vec (2);
	  PROTOCOL_NAME (protocol) = name;
	  PROTOCOL_LIST (protocol) = NULL_TREE;
	  add_protocol (protocol);
	  PROTOCOL_DEFINED (protocol) = 0;
	  PROTOCOL_FORWARD_DECL (protocol) = NULL_TREE;
	}
    }
}

tree
start_protocol (code, name, list)
     enum tree_code code;
     tree name;
     tree list;
{
  tree protocol;

  /* APPLE LOCAL Objective-C++ */
#ifdef OBJCPLUS
  if (current_namespace != global_namespace) {
    error ("Objective-C declarations may only appear in global scope");
  }
#endif /* OBJCPLUS */
  
  /* This is as good a place as any.  Need to invoke
     push_tag_toplevel.  */
  if (!objc_protocol_template)
    objc_protocol_template = build_protocol_template ();

  protocol = lookup_protocol (name);

  if (!protocol)
    {
      protocol = make_node (code);
      TYPE_BINFO (protocol) = make_tree_vec (2);

      PROTOCOL_NAME (protocol) = name;
      PROTOCOL_LIST (protocol) = lookup_and_install_protocols (list);
      add_protocol (protocol);
      PROTOCOL_DEFINED (protocol) = 1;
      PROTOCOL_FORWARD_DECL (protocol) = NULL_TREE;

      check_protocol_recursively (protocol, list);
    }
  else if (! PROTOCOL_DEFINED (protocol))
    {
      PROTOCOL_DEFINED (protocol) = 1;
      PROTOCOL_LIST (protocol) = lookup_and_install_protocols (list);

      check_protocol_recursively (protocol, list);
    }
  else
    {
      warning ("duplicate declaration for protocol `%s'",
	       IDENTIFIER_POINTER (name));
    }

  /* APPLE LOCAL begin indexing dpatel */
  if (flag_gen_index)
    {
       tree pnode = PROTOCOL_LIST (protocol);
       gen_indexing_info (INDEX_PROTOCOL_BEGIN,
                          IDENTIFIER_POINTER (OBJC_TYPE_NAME (protocol)),
                          lineno);
      /* Add adopted protocols.  */
      if (pnode)
	{
	  for (; pnode; pnode = TREE_CHAIN (pnode))
	    {
	      tree pdecl = TREE_VALUE (pnode);
	      gen_indexing_info (INDEX_PROTOCOL_INHERITANCE,
				 IDENTIFIER_POINTER (OBJC_TYPE_NAME (pdecl)),
				 0);
	    }
	}
    }
  /* APPLE LOCAL end indexing dpatel */
  return protocol;
}

void
finish_protocol (protocol)
     /* APPLE LOCAL indexing dpatel */
     tree protocol;
{
  /* APPLE LOCAL begin indexing dpatel */
  /* Can use tree code PROTOCOL_INTERFACE_TYPE */
  if (flag_gen_index)
    gen_indexing_info (INDEX_PROTOCOL_END,
                       IDENTIFIER_POINTER (OBJC_TYPE_NAME (protocol)),
                       lineno);
  /* APPLE LOCAL end indexing dpatel */
}


/* "Encode" a data type into a string, which grows in util_obstack.
   ??? What is the FORMAT?  Someone please document this!  */

static void
encode_type_qualifiers (declspecs)
     tree declspecs;
{
  tree spec;

  for (spec = declspecs; spec; spec = TREE_CHAIN (spec))
    {
      if (ridpointers[(int) RID_CONST] == TREE_VALUE (spec))
	obstack_1grow (&util_obstack, 'r');
      else if (ridpointers[(int) RID_IN] == TREE_VALUE (spec))
	obstack_1grow (&util_obstack, 'n');
      else if (ridpointers[(int) RID_INOUT] == TREE_VALUE (spec))
	obstack_1grow (&util_obstack, 'N');
      else if (ridpointers[(int) RID_OUT] == TREE_VALUE (spec))
	obstack_1grow (&util_obstack, 'o');
      else if (ridpointers[(int) RID_BYCOPY] == TREE_VALUE (spec))
	obstack_1grow (&util_obstack, 'O');
      else if (ridpointers[(int) RID_BYREF] == TREE_VALUE (spec))
        obstack_1grow (&util_obstack, 'R');
      else if (ridpointers[(int) RID_ONEWAY] == TREE_VALUE (spec))
	obstack_1grow (&util_obstack, 'V');
    }
}

/* Encode a pointer type.  */

static void
encode_pointer (type, curtype, format)
     tree type;
     int curtype;
     int format;
{
  tree pointer_to = TREE_TYPE (type);

  if (TREE_CODE (pointer_to) == RECORD_TYPE)
    {
      /* APPLE LOCAL begin type aliasing */
      if (OBJC_TYPE_NAME (pointer_to)
	  && TREE_CODE (OBJC_TYPE_NAME (pointer_to)) == IDENTIFIER_NODE)
	{
	  const char *name = IDENTIFIER_POINTER (OBJC_TYPE_NAME (pointer_to));
	  /* APPLE LOCAL end type aliasing */

	  if (strcmp (name, TAG_OBJECT) == 0) /* '@' */
	    {
	      obstack_1grow (&util_obstack, '@');
	      return;
	    }
	  else if (TREE_STATIC_TEMPLATE (pointer_to))
	    {
              if (generating_instance_variables)
	        {
	          obstack_1grow (&util_obstack, '@');
	          obstack_1grow (&util_obstack, '"');
	          obstack_grow (&util_obstack, name, strlen (name));
	          obstack_1grow (&util_obstack, '"');
	          return;
		}
              else
	        {
	          obstack_1grow (&util_obstack, '@');
	          return;
		}
	    }
	  else if (strcmp (name, TAG_CLASS) == 0) /* '#' */
	    {
	      obstack_1grow (&util_obstack, '#');
	      return;
	    }
	  else if (strcmp (name, TAG_SELECTOR) == 0) /* ':' */
	    {
	      obstack_1grow (&util_obstack, ':');
	      return;
	    }
	}
    }
  else if (TREE_CODE (pointer_to) == INTEGER_TYPE
	   && TYPE_MODE (pointer_to) == QImode)
    {
      /* APPLE LOCAL begin bool encoding */
      tree pname = TREE_CODE (OBJC_TYPE_NAME (pointer_to)) == IDENTIFIER_NODE
	          ? OBJC_TYPE_NAME (pointer_to) 
	          : DECL_NAME (OBJC_TYPE_NAME (pointer_to));
      
      if (!!strcmp (IDENTIFIER_POINTER (pname), "BOOL"))
	{
	  obstack_1grow (&util_obstack, '*');
	  return;
	}
      /* APPLE LOCAL end bool encoding */
    }

  /* We have a type that does not get special treatment.  */

  /* NeXT extension */
  obstack_1grow (&util_obstack, '^');
  encode_type (pointer_to, curtype, format);
}

static void
encode_array (type, curtype, format)
     tree type;
     int curtype;
     int format;
{
  tree an_int_cst = TYPE_SIZE (type);
  tree array_of = TREE_TYPE (type);
  char buffer[40];

  /* An incomplete array is treated like a pointer.  */
  if (an_int_cst == NULL)
    {
      encode_pointer (type, curtype, format);
      return;
    }

  sprintf (buffer, "[%ld",
	   (long) (TREE_INT_CST_LOW (an_int_cst)
		   / TREE_INT_CST_LOW (TYPE_SIZE (array_of))));

  obstack_grow (&util_obstack, buffer, strlen (buffer));
  encode_type (array_of, curtype, format);
  obstack_1grow (&util_obstack, ']');
  return;
}

static void
encode_aggregate_within (type, curtype, format, left, right)
     tree type;
     int curtype;
     int format;
     int left;
     int right;
{
  /* APPLE LOCAL begin method encoding */
  tree name;
  /* NB: aggregates that are pointed to have slightly different encoding
     rules in that you never encode the names of instance variables.  */
  int pointed_to
   = (obstack_object_size (&util_obstack) > 0
      && *(obstack_next_free (&util_obstack) - 1) == '^');
  int inline_contents
   = ((format == OBJC_ENCODE_INLINE_DEFS || generating_instance_variables)
      && (!pointed_to || obstack_object_size (&util_obstack) - curtype == 1));
       
  /* Traverse struct aliases; it is important to get the
     original struct and its tag name (if any).  */
  type = TYPE_MAIN_VARIANT (type);
  name = OBJC_TYPE_NAME (type);
  /* Open parenth/bracket.  */  			 
  obstack_1grow (&util_obstack, left);

  /* Encode the struct/union tag name, or '?' if a tag was
     not provided.  Typedef aliases do not qualify.  */
  if (name && TREE_CODE (name) == IDENTIFIER_NODE
#ifdef OBJCPLUS
      /* Did this struct have a tag?  */
      && !TYPE_WAS_ANONYMOUS (type)
#endif
      )
    obstack_grow (&util_obstack,
		  IDENTIFIER_POINTER (name),
		  strlen (IDENTIFIER_POINTER (name)));
  else
    obstack_1grow (&util_obstack, '?');
    
  /* Encode the types (and possibly names) of the inner fields,
     if required.  */  
  if (inline_contents)
    {
      tree fields = TYPE_FIELDS (type);

      obstack_1grow (&util_obstack, '=');
      for (; fields; fields = TREE_CHAIN (fields))
	{
	  /* APPLE LOCAL begin Objective-C++ */
#ifdef OBJCPLUS
	  /* C++ static members, and things that are not fields at all,
	     should not appear in the encoding.  */
	  if (TREE_CODE (fields) != FIELD_DECL || TREE_STATIC (fields))
	    continue;
#endif
	  /* APPLE LOCAL end Objective-C++ */
	  if (generating_instance_variables && !pointed_to)
	    {
	      tree fname = DECL_NAME (fields);

	      obstack_1grow (&util_obstack, '"');
	      if (fname && TREE_CODE (fname) == IDENTIFIER_NODE)
		obstack_grow (&util_obstack,
			      IDENTIFIER_POINTER (fname),
			      strlen (IDENTIFIER_POINTER (fname)));
	      obstack_1grow (&util_obstack, '"');
	    }
	  encode_field_decl (fields, curtype, format);
	}
    }
  /* Close parenth/bracket.  */  			 
  obstack_1grow (&util_obstack, right);
  /* APPLE LOCAL end method encoding */
}

static void
encode_aggregate (type, curtype, format)
     tree type;
     int curtype;
     int format;
{
  enum tree_code code = TREE_CODE (type);

  switch (code)
    {
    case RECORD_TYPE:
      {
	encode_aggregate_within (type, curtype, format, '{', '}');
	break;
      }
    case UNION_TYPE:
      {
	encode_aggregate_within (type, curtype, format, '(', ')');
	break;
      }

    case ENUMERAL_TYPE:
      obstack_1grow (&util_obstack, 'i');
      break;

    default:
      break;
    }
}

/* Support bitfields.  The current version of Objective-C does not support
   them.  The string will consist of one or more "b:n"'s where n is an
   integer describing the width of the bitfield. Currently, classes in
   the kit implement a method "-(char *)describeBitfieldStruct:" that
   simulates this. If they do not implement this method, the archiver
   assumes the bitfield is 16 bits wide (padded if necessary) and packed
   according to the GNU compiler. After looking at the "kit", it appears
   that all classes currently rely on this default behavior, rather than
   hand generating this string (which is tedious).  */

/* APPLE LOCAL bitfield alignment */
/* TODO: The above comment is now outdated; revisit after madness subsides. */

static void
encode_bitfield (width)
     int width;
{
  char buffer[40];
  sprintf (buffer, "b%d", width);
  obstack_grow (&util_obstack, buffer, strlen (buffer));
}

/* FORMAT will be OBJC_ENCODE_INLINE_DEFS or OBJC_ENCODE_DONT_INLINE_DEFS.  */

static void
encode_type (type, curtype, format)
     tree type;
     int curtype;
     int format;
{
  enum tree_code code = TREE_CODE (type);

  if (code == INTEGER_TYPE)
    {
      if (integer_zerop (TYPE_MIN_VALUE (type)))
	{
	  /* Unsigned integer types.  */

	  if (TYPE_MODE (type) == QImode)
	    obstack_1grow (&util_obstack, 'C');
	  else if (TYPE_MODE (type) == HImode)
	    obstack_1grow (&util_obstack, 'S');
	  else if (TYPE_MODE (type) == SImode)
	    {
	      if (type == long_unsigned_type_node)
		obstack_1grow (&util_obstack, 'L');
	      else
		obstack_1grow (&util_obstack, 'I');
	    }
	  else if (TYPE_MODE (type) == DImode)
	    obstack_1grow (&util_obstack, 'Q');
	}

      else
	/* Signed integer types.  */
	{
	  if (TYPE_MODE (type) == QImode)
	    obstack_1grow (&util_obstack, 'c');
	  else if (TYPE_MODE (type) == HImode)
	    obstack_1grow (&util_obstack, 's');
	  else if (TYPE_MODE (type) == SImode)
	    {
	      if (type == long_integer_type_node)
		obstack_1grow (&util_obstack, 'l');
	      else
		obstack_1grow (&util_obstack, 'i');
	    }

	  else if (TYPE_MODE (type) == DImode)
	    obstack_1grow (&util_obstack, 'q');
	}
    }

  else if (code == REAL_TYPE)
    {
      /* Floating point types.  */

      if (TYPE_MODE (type) == SFmode)
	obstack_1grow (&util_obstack, 'f');
      else if (TYPE_MODE (type) == DFmode
	       || TYPE_MODE (type) == TFmode)
	obstack_1grow (&util_obstack, 'd');
    }

  else if (code == VOID_TYPE)
    obstack_1grow (&util_obstack, 'v');

  /* APPLE LOCAL begin Panther ObjC enhancements */
  else if (code == BOOLEAN_TYPE)
    obstack_1grow (&util_obstack, 'B');
  /* APPLE LOCAL end Panther ObjC enhancements */

  else if (code == ARRAY_TYPE)
    encode_array (type, curtype, format);

  else if (code == POINTER_TYPE)
    encode_pointer (type, curtype, format);

  else if (code == RECORD_TYPE || code == UNION_TYPE || code == ENUMERAL_TYPE)
    encode_aggregate (type, curtype, format);

  else if (code == FUNCTION_TYPE) /* '?' */
    obstack_1grow (&util_obstack, '?');
}

static void
encode_complete_bitfield (position, type, size)
     int position;
     tree type;
     int size;
{
  enum tree_code code = TREE_CODE (type);
  char buffer[40];
  char charType = '?';

  if (code == INTEGER_TYPE)
    {
      if (integer_zerop (TYPE_MIN_VALUE (type)))
	{
	  /* Unsigned integer types.  */

	  if (TYPE_MODE (type) == QImode)
	    charType = 'C';
	  else if (TYPE_MODE (type) == HImode)
	    charType = 'S';
	  else if (TYPE_MODE (type) == SImode)
	    {
	      if (type == long_unsigned_type_node)
		charType = 'L';
	      else
		charType = 'I';
	    }
	  else if (TYPE_MODE (type) == DImode)
	    charType = 'Q';
	}

      else
	/* Signed integer types.  */
	{
	  if (TYPE_MODE (type) == QImode)
	    charType = 'c';
	  else if (TYPE_MODE (type) == HImode)
	    charType = 's';
	  else if (TYPE_MODE (type) == SImode)
	    {
	      if (type == long_integer_type_node)
		charType = 'l';
	      else
		charType = 'i';
	    }

	  else if (TYPE_MODE (type) == DImode)
	    charType = 'q';
	}
    }
  else if (code == ENUMERAL_TYPE)
    charType = 'i';
  else
    abort ();

  sprintf (buffer, "b%d%c%d", position, charType, size);
  obstack_grow (&util_obstack, buffer, strlen (buffer));
}

static void
encode_field_decl (field_decl, curtype, format)
     tree field_decl;
     int curtype;
     int format;
{
  tree type;

  /* APPLE LOCAL begin Objective-C++ */
#ifdef OBJCPLUS
  /* C++ static members, and things that are not fields at all,
     should not appear in the encoding.  */
  if (TREE_CODE (field_decl) != FIELD_DECL || TREE_STATIC (field_decl))
    return;
#endif
  /* APPLE LOCAL end Objective-C++ */

  type = TREE_TYPE (field_decl);

  /* If this field is obviously a bitfield, or is a bitfield that has been
     clobbered to look like a ordinary integer mode, go ahead and generate
     the bitfield typing information.  */
  if (flag_next_runtime)
    {
      if (DECL_BIT_FIELD_TYPE (field_decl))
	encode_bitfield (tree_low_cst (DECL_SIZE (field_decl), 1));
      else
	encode_type (TREE_TYPE (field_decl), curtype, format);
    }
  else
    {
      if (DECL_BIT_FIELD_TYPE (field_decl))
	encode_complete_bitfield (int_bit_position (field_decl),
				  DECL_BIT_FIELD_TYPE (field_decl),
				  tree_low_cst (DECL_SIZE (field_decl), 1));
      else
	encode_type (TREE_TYPE (field_decl), curtype, format);
    }
}

static tree
expr_last (complex_expr)
     tree complex_expr;
{
  tree next;

  if (complex_expr)
    while ((next = TREE_OPERAND (complex_expr, 0)))
      complex_expr = next;

  return complex_expr;
}
/* APPLE LOCAL begin Objective-C++ */
static void
synth_self_and_ucmd_args ()
{
  tree decl_specs;
  
  if (objc_method_context 
      && TREE_CODE (objc_method_context) == INSTANCE_METHOD_DECL)
    decl_specs = build_tree_list (NULL_TREE, uprivate_record);
  else
    /* Really a `struct objc_class *'. However, we allow people to
       assign to self, which changes its type midstream.  */
    decl_specs = build_tree_list (NULL_TREE, objc_object_reference);

  push_parm_decl (build_tree_list
		  (build_tree_list (decl_specs,
				    build1 (INDIRECT_REF, NULL_TREE, self_id)),
		   unused_list));

  decl_specs = build_tree_list (NULL_TREE,
				xref_tag (RECORD_TYPE,
					  get_identifier (TAG_SELECTOR)));
  push_parm_decl (build_tree_list
		  (build_tree_list (decl_specs,
				    build1 (INDIRECT_REF, NULL_TREE, ucmd_id)),
		   unused_list));
}
/* APPLE LOCAL end Objective-C++ */

/* Transform a method definition into a function definition as follows:
   - synthesize the first two arguments, "self" and "_cmd".  */

void
start_method_def (method)
     tree method;
{
  /* APPLE LOCAL Objective-C++ */
  /* remove unused local var.  */

  /* Required to implement _msgSuper.  */
  objc_method_context = method;
  UOBJC_SUPER_decl = NULL_TREE;

  /* Must be called BEFORE start_function.  */
  pushlevel (0);

  /* Generate prototype declarations for arguments..."new-style".  */
  /* APPLE LOCAL Objective-C++ */
  synth_self_and_ucmd_args ();
  
  /* Generate argument declarations if a keyword_decl.  */
  if (METHOD_SEL_ARGS (method))
    {
      tree arglist = METHOD_SEL_ARGS (method);
      do
	{
	  tree arg_spec = TREE_PURPOSE (TREE_TYPE (arglist));
	  tree arg_decl = TREE_VALUE (TREE_TYPE (arglist));

	  if (arg_decl)
	    {
	      tree last_expr = expr_last (arg_decl);

	      /* Unite the abstract decl with its name.  */
	      TREE_OPERAND (last_expr, 0) = KEYWORD_ARG_NAME (arglist);
	      push_parm_decl (build_tree_list
			      (build_tree_list (arg_spec, arg_decl),
			       NULL_TREE));

	      /* APPLE LOCAL Objective-C++ */
#ifndef OBJCPLUS	      
	      /* Unhook: restore the abstract declarator.  */
	      TREE_OPERAND (last_expr, 0) = NULL_TREE;
#endif	      
	    }

	  else
	    push_parm_decl (build_tree_list
			    (build_tree_list (arg_spec,
					      KEYWORD_ARG_NAME (arglist)),
			     NULL_TREE));

	  arglist = TREE_CHAIN (arglist);
	}
      while (arglist);
    }

  if (METHOD_ADD_ARGS (method) != NULL_TREE
      && METHOD_ADD_ARGS (method) != objc_ellipsis_node)
    {
      /* We have a variable length selector - in "prototype" format.  */
      tree akey = TREE_PURPOSE (METHOD_ADD_ARGS (method));
      while (akey)
	{
	  /* This must be done prior to calling pushdecl.  pushdecl is
	     going to change our chain on us.  */
	  tree nextkey = TREE_CHAIN (akey);
	  pushdecl (akey);
	  akey = nextkey;
	}
    }
  /* APPLE LOCAL begin indexing dpatel */
  /* Can use tree code  INSTANCE/CLASS_METHOD_DECL */
  if (flag_gen_index && TREE_CODE (method) == INSTANCE_METHOD_DECL)
    gen_indexing_info (INDEX_INSTANCE_METHOD_BEGIN,
                       IDENTIFIER_POINTER (DECL_NAME (method)),
                       lineno);
  else if (flag_gen_index && TREE_CODE (method) == CLASS_METHOD_DECL)
    gen_indexing_info (INDEX_CLASS_METHOD_BEGIN,
                       IDENTIFIER_POINTER (DECL_NAME (method)),
                       lineno);
  /* APPLE LOCAL end indexing dpatel */
}

static void
warn_with_method (message, mtype, method)
     const char *message;
     int mtype;
     tree method;
{
  if (!diagnostic_count_diagnostic (global_dc, DK_WARNING))
    return;

  diagnostic_report_current_function (global_dc);

  /* Add a readable method name to the warning.  */
  warning_with_file_and_line (DECL_SOURCE_FILE (method),
			      DECL_SOURCE_LINE (method),
			      "%s `%c%s'",
			      message, mtype,
			      gen_method_decl (method, errbuf));
}

/* Return 1 if METHOD is consistent with PROTO.  */

static int
comp_method_with_proto (method, proto)
     tree method, proto;
{
  /* Create a function template node at most once.  */
  if (!function1_template)
    function1_template = make_node (FUNCTION_TYPE);

  /* Install argument types - normally set by build_function_type.  */
  TYPE_ARG_TYPES (function1_template) = get_arg_type_list (proto, METHOD_DEF, 0);

  /* install return type */
  TREE_TYPE (function1_template) = groktypename (TREE_TYPE (proto));

/* BEGIN APPLE LOCAL IMI */
  return comptypes (TREE_TYPE (METHOD_DEFINITION (method)), function1_template,
		    false);
/* END APPLE LOCAL IMI */
}

/* Return 1 if PROTO1 is consistent with PROTO2.  */

static int
comp_proto_with_proto (proto0, proto1)
     tree proto0, proto1;
{
  /* Create a couple of function_template nodes at most once.  */
  if (!function1_template)
    function1_template = make_node (FUNCTION_TYPE);
  if (!function2_template)
    function2_template = make_node (FUNCTION_TYPE);

  /* Install argument types; normally set by build_function_type.  */
  TYPE_ARG_TYPES (function1_template) = get_arg_type_list (proto0, METHOD_REF, 0);
  TYPE_ARG_TYPES (function2_template) = get_arg_type_list (proto1, METHOD_REF, 0);

  /* Install return type.  */
  TREE_TYPE (function1_template) = groktypename (TREE_TYPE (proto0));
  TREE_TYPE (function2_template) = groktypename (TREE_TYPE (proto1));

/* BEGIN APPLE LOCAL IMI */
  return comptypes (function1_template, function2_template, false);
/* END APPLE LOCAL IMI */
}

/* - Generate an identifier for the function. the format is "_n_cls",
     where 1 <= n <= nMethods, and cls is the name the implementation we
     are processing.
   - Install the return type from the method declaration.
   - If we have a prototype, check for type consistency.  */

static void
really_start_method (method, parmlist)
     tree method, parmlist;
{
  tree sc_spec, ret_spec, ret_decl, decl_specs;
  tree method_decl, method_id;
  const char *sel_name, *class_name, *cat_name;
  char *buf;

  /* Synth the storage class & assemble the return type.  */
  sc_spec = tree_cons (NULL_TREE, ridpointers[(int) RID_STATIC], NULL_TREE);
  ret_spec = TREE_PURPOSE (TREE_TYPE (method));
  decl_specs = chainon (sc_spec, ret_spec);

  sel_name = IDENTIFIER_POINTER (METHOD_SEL_NAME (method));
  class_name = IDENTIFIER_POINTER (CLASS_NAME (objc_implementation_context));
  cat_name = ((TREE_CODE (objc_implementation_context)
	       == CLASS_IMPLEMENTATION_TYPE)
	      ? NULL
	      : IDENTIFIER_POINTER (CLASS_SUPER_NAME (objc_implementation_context)));
  method_slot++;

  /* Make sure this is big enough for any plausible method label.  */
  buf = (char *) alloca (50 + strlen (sel_name) + strlen (class_name)
			 + (cat_name ? strlen (cat_name) : 0));

  OBJC_GEN_METHOD_LABEL (buf, TREE_CODE (method) == INSTANCE_METHOD_DECL,
			 class_name, cat_name, sel_name, method_slot);

  method_id = get_identifier (buf);

  /* APPLE LOCAL Objective-C++ */
#ifdef OBJCPLUS
  /* Objective-C methods cannot be overloaded, so we don't need
     the type encoding appended.  It looks bad anyway... */
  push_lang_context (lang_name_c);
#endif

  method_decl = build_nt (CALL_EXPR, method_id, parmlist, NULL_TREE);

  /* Check the declarator portion of the return type for the method.  */
  if ((ret_decl = TREE_VALUE (TREE_TYPE (method))))
    {
      /* Unite the complex decl (specified in the abstract decl) with the
	 function decl just synthesized..(int *), (int (*)()), (int (*)[]).  */
      tree save_expr = expr_last (ret_decl);

      TREE_OPERAND (save_expr, 0) = method_decl;
      method_decl = ret_decl;

      /* Fool the parser into thinking it is starting a function.  */
      start_function (decl_specs, method_decl, NULL_TREE);

      /* Unhook: this has the effect of restoring the abstract declarator.  */
      TREE_OPERAND (save_expr, 0) = NULL_TREE;
    }

  else
    {
      TREE_VALUE (TREE_TYPE (method)) = method_decl;

      /* Fool the parser into thinking it is starting a function.  */
      start_function (decl_specs, method_decl, NULL_TREE);

      /* Unhook: this has the effect of restoring the abstract declarator.  */
      TREE_VALUE (TREE_TYPE (method)) = NULL_TREE;
    }

  /* APPLE LOCAL begin Objective-C++ */
#ifdef OBJCPLUS
  /* set self_decl from the first argument...this global is used by 
   * build_ivar_reference().build_indirect_ref().
   */
  self_decl = DECL_ARGUMENTS (current_function_decl);

  /* snaroff (3/28/96): when compiling with -Wall, this suppresses
   * the following: warning:unused parameter `struct objc_selector * _cmd'
   */
  TREE_USED (self_decl) = 1;
  TREE_USED (TREE_CHAIN (self_decl)) = 1;
  /* Ditto for the underlying (static) C function.  */
  TREE_USED (current_function_decl) = 1;
  pop_lang_context ();
#endif
  /* APPLE LOCAL end Objective-C++ */

  METHOD_DEFINITION (method) = current_function_decl;

  /* Check consistency...start_function, pushdecl, duplicate_decls.  */

  if (implementation_template != objc_implementation_context)
    {
      tree proto;

      if (TREE_CODE (method) == INSTANCE_METHOD_DECL)
	proto = lookup_instance_method_static (implementation_template,
					       METHOD_SEL_NAME (method));
      else
	proto = lookup_class_method_static (implementation_template,
					    METHOD_SEL_NAME (method));

      if (proto && ! comp_method_with_proto (method, proto))
	{
	  char type = (TREE_CODE (method) == INSTANCE_METHOD_DECL ? '-' : '+');

	  warn_with_method ("conflicting types for", type, method);
	  warn_with_method ("previous declaration of", type, proto);
	}
    }
}

/* The following routine is always called...this "architecture" is to
   accommodate "old-style" variable length selectors.
 
   - a:a b:b // prototype  ; id c; id d; // old-style.  */

void
continue_method_def ()
{
  tree parmlist;

  if (METHOD_ADD_ARGS (objc_method_context) == objc_ellipsis_node)
    /* We have a `, ...' immediately following the selector.  */
    parmlist = get_parm_info (0);
  else
    parmlist = get_parm_info (1); /* place a `void_at_end' */

  /* APPLE LOCAL Objective-C++  */
#ifndef OBJCPLUS
  /* Set self_decl from the first argument...this global is used by
     build_ivar_reference calling build_indirect_ref.  */
  self_decl = TREE_PURPOSE (parmlist);
#endif /* !OBJCPLUS */

  poplevel (0, 0, 0);
  really_start_method (objc_method_context, parmlist);
  store_parm_decls ();
}

/* APPLE LOCAL msg send super  */
static void *UOBJC_SUPER_binding_level = 0;
/* The 'add_objc_decls' routine is no more.  */

/* _n_Method (id self, SEL sel, ...)
     {
       struct objc_super _S;
       _msgSuper ((_S.self = self, _S.class = _cls, &_S), ...);
     }  */

tree
get_super_receiver ()
{
  if (objc_method_context)
    {
      tree super_expr, super_expr_list;

      /* APPLE LOCAL begin msg send super */
      if (!UOBJC_SUPER_decl)
      {
	/* APPLE LOCAL indexing dpatel */
	flag_suppress_builtin_indexing = 1;
	
	UOBJC_SUPER_decl = start_decl (get_identifier (TAG_SUPER),
				       build_tree_list (NULL_TREE,
				       objc_super_template),
				       0, NULL_TREE);
	
	finish_decl (UOBJC_SUPER_decl, NULL_TREE, NULL_TREE);

	/* This prevents `unused variable' warnings when compiling with -Wall.  */
	TREE_USED (UOBJC_SUPER_decl) = 1;
	DECL_ARTIFICIAL (UOBJC_SUPER_decl) = 1;

	/* APPLE LOCAL msg send super */
	UOBJC_SUPER_binding_level = get_current_binding_level ();

	/* APPLE LOCAL indexing dpatel */
	flag_suppress_builtin_indexing = 0;
      }
      /* APPLE LOCAL end msg send super */

      /* Set receiver to self.  */
      super_expr = build_component_ref (UOBJC_SUPER_decl, self_id);
      super_expr = build_modify_expr (super_expr, NOP_EXPR, self_decl);
      super_expr_list = build_tree_list (NULL_TREE, super_expr);

      /* Set class to begin searching.  */
      super_expr = build_component_ref (UOBJC_SUPER_decl,
					get_identifier ("class"));

      if (TREE_CODE (objc_implementation_context) == CLASS_IMPLEMENTATION_TYPE)
	{
	  /* [_cls, __cls]Super are "pre-built" in
	     synth_forward_declarations.  */

	  super_expr = build_modify_expr (super_expr, NOP_EXPR,
					  ((TREE_CODE (objc_method_context)
					    == INSTANCE_METHOD_DECL)
					   ? ucls_super_ref
					   : uucls_super_ref));
	}

      else
	/* We have a category.  */
	{
	  tree super_name = CLASS_SUPER_NAME (implementation_template);
	  tree super_class;

	  /* Barf if super used in a category of Object.  */
	  if (!super_name)
	    {
	      error ("no super class declared in interface for `%s'",
		    IDENTIFIER_POINTER (CLASS_NAME (implementation_template)));
	      return error_mark_node;
	    }

	  /* APPLE LOCAL Panther ObjC enhancements */
	  if (flag_next_runtime && !flag_zero_link)
	    {
	      super_class = get_class_reference (super_name);
	      if (TREE_CODE (objc_method_context) == CLASS_METHOD_DECL)
		/* APPLE LOCAL begin msg send super */
		/* If we are in a class method, we must retrieve the
		   _metaclass_ for the current class, pointed at by
		   the class's "isa" pointer.  The following assumes that
		   "isa" is the first ivar in a class (which it must be).  */
		super_class
		  = build_indirect_ref 
		    (build_c_cast (build_pointer_type (objc_class_type),
				   super_class), "unary *");
	    }
	  else
	    {
	      add_class_reference (super_name);
	      super_class = (TREE_CODE (objc_method_context) == INSTANCE_METHOD_DECL
			     ? objc_get_class_decl : objc_get_meta_class_decl);
	      assemble_external (super_class);
	      super_class
		= build_function_call
		  (super_class,
		   build_tree_list
		   (NULL_TREE,
		    my_build_string (IDENTIFIER_LENGTH (super_name) + 1,
				     IDENTIFIER_POINTER (super_name))));
	    }

	  /* APPLE LOCAL begin msg send super */
	  super_expr 
	    = build_modify_expr (super_expr, NOP_EXPR, 
				 build_c_cast (TREE_TYPE (super_expr),
					       super_class));
	  /* APPLE LOCAL end msg send super */
	}

      chainon (super_expr_list, build_tree_list (NULL_TREE, super_expr));

      super_expr = build_unary_op (ADDR_EXPR, UOBJC_SUPER_decl, 0);
      chainon (super_expr_list, build_tree_list (NULL_TREE, super_expr));

      return build_compound_expr (super_expr_list);
    }
  else
    {
      error ("[super ...] must appear in a method context");
      return error_mark_node;
    }
}


/* APPLE LOCAL begin msg send super */
void
objc_clear_super_receiver ()
{
  if (objc_method_context 
      && UOBJC_SUPER_binding_level == get_current_binding_level ()) {
    UOBJC_SUPER_decl = 0;
    UOBJC_SUPER_binding_level = 0;
  }  
}
/* APPLE LOCAL end msg send super */


/* APPLE LOCAL method encoding */
/* The encode_method_def routine is no more;
   encode_method_prototype does a much better job.  */

static void
objc_expand_function_end ()
{
  /* APPLE LOCAL begin method encoding */
  /* This routine may also get called for C functions, including those
     nested within ObjC methods.  In such cases, method encoding is
     meaningless.  */
  if (objc_method_context == NULL_TREE
      || DECL_INITIAL (objc_method_context) != current_function_decl)
    return;
  /* APPLE LOCAL end method encoding */
    
  METHOD_ENCODING (objc_method_context) 
    /* APPLE LOCAL method encoding */
    = encode_method_prototype (objc_method_context);
}

void
finish_method_def ()
{
  lang_expand_function_end = objc_expand_function_end;
  /* APPLE LOCAL begin indexing dpatel */
  if (flag_gen_index)
    gen_indexing_info (INDEX_INSTANCE_METHOD_END, NULL, lineno);
  flag_suppress_builtin_indexing = 1;
  /* APPLE LOCAL end indexing dpatel */
  /* APPLE LOCAL begin suppress method inlining */
  /* We can't validly inline methods, at least not without a language
     extension to declare that a method need not be dynamically
     dispatched, so suppress all thoughts of doing so.  */
  DECL_INLINE (current_function_decl) = 0;
  DECL_UNINLINABLE (current_function_decl) = 1;
  current_function_cannot_inline = "methods cannot be inlined";
  /* APPLE LOCAL end suppress method inlining */

  finish_function (0, 1);
  lang_expand_function_end = NULL;

  /* Required to implement _msgSuper. This must be done AFTER finish_function,
     since the optimizer may find "may be used before set" errors.  */
  objc_method_context = NULL_TREE;
  /* APPLE LOCAL indexing dpatel */
  flag_suppress_builtin_indexing = 0;
}

#if 0
int
lang_report_error_function (decl)
      tree decl;
{
  if (objc_method_context)
    {
      fprintf (stderr, "In method `%s'\n",
	       IDENTIFIER_POINTER (METHOD_SEL_NAME (objc_method_context)));
      return 1;
    }

  else
    return 0;
}
#endif

static int
is_complex_decl (type)
     tree type;
{
  return (TREE_CODE (type) == ARRAY_TYPE
	  || TREE_CODE (type) == FUNCTION_TYPE
	  || (TREE_CODE (type) == POINTER_TYPE && ! IS_ID (type)));
}


/* Code to convert a decl node into text for a declaration in C.  */

static char tmpbuf[256];

static void
adorn_decl (decl, str)
     tree decl;
     char *str;
{
  enum tree_code code = TREE_CODE (decl);

  if (code == ARRAY_REF)
    {
      tree an_int_cst = TREE_OPERAND (decl, 1);

      if (an_int_cst && TREE_CODE (an_int_cst) == INTEGER_CST)
	sprintf (str + strlen (str), "[%ld]",
		 (long) TREE_INT_CST_LOW (an_int_cst));
      else
	strcat (str, "[]");
    }

  else if (code == ARRAY_TYPE)
    {
      tree an_int_cst = TYPE_SIZE (decl);
      tree array_of = TREE_TYPE (decl);

      if (an_int_cst && TREE_CODE (an_int_cst) == INTEGER_TYPE)
	sprintf (str + strlen (str), "[%ld]",
		 (long) (TREE_INT_CST_LOW (an_int_cst)
			 / TREE_INT_CST_LOW (TYPE_SIZE (array_of))));
      else
	strcat (str, "[]");
    }

  else if (code == CALL_EXPR)
    {
      tree chain = TREE_PURPOSE (TREE_OPERAND (decl, 1));

      strcat (str, "(");
      while (chain)
	{
	  gen_declaration_1 (chain, str);
	  chain = TREE_CHAIN (chain);
	  if (chain)
	    strcat (str, ", ");
	}
      strcat (str, ")");
    }

  else if (code == FUNCTION_TYPE)
    {
      tree chain  = TYPE_ARG_TYPES (decl);

      strcat (str, "(");
      while (chain && TREE_VALUE (chain) != void_type_node)
	{
	  gen_declaration_1 (TREE_VALUE (chain), str);
	  chain = TREE_CHAIN (chain);
	  if (chain && TREE_VALUE (chain) != void_type_node)
	    strcat (str, ", ");
	}
      strcat (str, ")");
    }

  else if (code == INDIRECT_REF)
    {
      strcpy (tmpbuf, "*");
      if (TREE_TYPE (decl) && TREE_CODE (TREE_TYPE (decl)) == TREE_LIST)
	{
	  tree chain;

	  for (chain = nreverse (copy_list (TREE_TYPE (decl)));
	       chain;
	       chain = TREE_CHAIN (chain))
	    {
	      if (TREE_CODE (TREE_VALUE (chain)) == IDENTIFIER_NODE)
		{
		  strcat (tmpbuf, " ");
		  strcat (tmpbuf, IDENTIFIER_POINTER (TREE_VALUE (chain)));
		}
	    }
	  if (str[0])
	    strcat (tmpbuf, " ");
	}
      strcat (tmpbuf, str);
      strcpy (str, tmpbuf);
    }

  else if (code == POINTER_TYPE)
    {
      strcpy (tmpbuf, "*");
      if (TREE_READONLY (decl) || TYPE_VOLATILE (decl))
	{
	  if (TREE_READONLY (decl))
	    strcat (tmpbuf, " const");
	  if (TYPE_VOLATILE (decl))
	    strcat (tmpbuf, " volatile");
	  if (str[0])
	    strcat (tmpbuf, " ");
	}
      strcat (tmpbuf, str);
      strcpy (str, tmpbuf);
    }
}

static char *
gen_declarator (decl, buf, name)
     tree decl;
     char *buf;
     const char *name;
{
  if (decl)
    {
      enum tree_code code = TREE_CODE (decl);
      char *str;
      tree op;
      int wrap = 0;

      switch (code)
	{
	case ARRAY_REF:
	case INDIRECT_REF:
	case CALL_EXPR:
	  op = TREE_OPERAND (decl, 0);

	  /* We have a pointer to a function or array...(*)(), (*)[] */
	  if ((code == ARRAY_REF || code == CALL_EXPR)
	      && op && TREE_CODE (op) == INDIRECT_REF)
	    wrap = 1;

	  str = gen_declarator (op, buf, name);

	  if (wrap)
	    {
	      strcpy (tmpbuf, "(");
	      strcat (tmpbuf, str);
	      strcat (tmpbuf, ")");
	      strcpy (str, tmpbuf);
	    }

	  adorn_decl (decl, str);
	  break;

	case ARRAY_TYPE:
	case FUNCTION_TYPE:
	case POINTER_TYPE:
	  strcpy (buf, name);
	  str = buf;

	  /* This clause is done iteratively rather than recursively.  */
	  do
	    {
	      op = (is_complex_decl (TREE_TYPE (decl))
		    ? TREE_TYPE (decl) : NULL_TREE);

	      adorn_decl (decl, str);

	      /* We have a pointer to a function or array...(*)(), (*)[] */
	      if (code == POINTER_TYPE
		  && op && (TREE_CODE (op) == FUNCTION_TYPE
			    || TREE_CODE (op) == ARRAY_TYPE))
		{
		  strcpy (tmpbuf, "(");
		  strcat (tmpbuf, str);
		  strcat (tmpbuf, ")");
		  strcpy (str, tmpbuf);
		}

	      decl = (is_complex_decl (TREE_TYPE (decl))
		      ? TREE_TYPE (decl) : NULL_TREE);
	    }

	  while (decl && (code = TREE_CODE (decl)))
	    ;

	  break;

	case IDENTIFIER_NODE:
	  /* Will only happen if we are processing a "raw" expr-decl.  */
	  strcpy (buf, IDENTIFIER_POINTER (decl));
	  return buf;

	default:
	  abort ();
	}

      return str;
    }

  else
    /* We have an abstract declarator or a _DECL node.  */
    {
      strcpy (buf, name);
      return buf;
    }
}

static void
gen_declspecs (declspecs, buf, raw)
     tree declspecs;
     char *buf;
     int raw;
{
  if (raw)
    {
      tree chain;

      for (chain = nreverse (copy_list (declspecs));
	   chain; chain = TREE_CHAIN (chain))
	{
	  tree aspec = TREE_VALUE (chain);

	  if (TREE_CODE (aspec) == IDENTIFIER_NODE)
	    strcat (buf, IDENTIFIER_POINTER (aspec));
	  else if (TREE_CODE (aspec) == RECORD_TYPE)
	    {
	      /* APPLE LOCAL type aliasing */
	      if (OBJC_TYPE_NAME (aspec))
		{
		  /* APPLE LOCAL SPEC */
		  tree protocol_list = type_protocol_list (aspec);

		  if (! TREE_STATIC_TEMPLATE (aspec))
		    strcat (buf, "struct ");
		  /* APPLE LOCAL type aliasing */
		  strcat (buf, IDENTIFIER_POINTER (OBJC_TYPE_NAME (aspec)));

		  /* NEW!!! */
		  if (protocol_list)
		    {
		      tree chain = protocol_list;

		      strcat (buf, " <");
		      while (chain)
			{
			  strcat (buf,
				  IDENTIFIER_POINTER
				  (PROTOCOL_NAME (TREE_VALUE (chain))));
			  chain = TREE_CHAIN (chain);
			  if (chain)
			    strcat (buf, ", ");
			}
		      strcat (buf, ">");
		    }
		}

	      else
		strcat (buf, "untagged struct");
	    }

	  else if (TREE_CODE (aspec) == UNION_TYPE)
	    {
	      /* APPLE LOCAL type aliasing */
	      if (OBJC_TYPE_NAME (aspec))
		{
		  if (! TREE_STATIC_TEMPLATE (aspec))
		    strcat (buf, "union ");
		  /* APPLE LOCAL type aliasing */
		  strcat (buf, IDENTIFIER_POINTER (OBJC_TYPE_NAME (aspec)));
		}
	      else
		strcat (buf, "untagged union");
	    }

	  else if (TREE_CODE (aspec) == ENUMERAL_TYPE)
	    {
	      /* APPLE LOCAL type aliasing */
	      if (OBJC_TYPE_NAME (aspec))
		{
		  if (! TREE_STATIC_TEMPLATE (aspec))
		    strcat (buf, "enum ");
		  /* APPLE LOCAL type aliasing */
		  strcat (buf, IDENTIFIER_POINTER (OBJC_TYPE_NAME (aspec)));
		}
	      else
		strcat (buf, "untagged enum");
	    }

	  else if (TREE_CODE (aspec) == TYPE_DECL && DECL_NAME (aspec))
	    strcat (buf, IDENTIFIER_POINTER (DECL_NAME (aspec)));

	  else if (IS_ID (aspec))
	    {
	      /* APPLE LOCAL SPEC */
	      tree protocol_list = type_protocol_list (aspec);

	      strcat (buf, "id");
	      if (protocol_list)
		{
		  tree chain = protocol_list;

		  strcat (buf, " <");
		  while (chain)
		    {
		      strcat (buf,
			      IDENTIFIER_POINTER
			      (PROTOCOL_NAME (TREE_VALUE (chain))));
		      chain = TREE_CHAIN (chain);
		      if (chain)
			strcat (buf, ", ");
		    }
		  strcat (buf, ">");
		}
	    }
	  if (TREE_CHAIN (chain))
	    strcat (buf, " ");
	}
    }
  else
    {
      /* Type qualifiers.  */
      if (TREE_READONLY (declspecs))
	strcat (buf, "const ");
      if (TYPE_VOLATILE (declspecs))
	strcat (buf, "volatile ");

      switch (TREE_CODE (declspecs))
	{
	  /* Type specifiers.  */

	case INTEGER_TYPE:
	  declspecs = TYPE_MAIN_VARIANT (declspecs);

	  /* Signed integer types.  */

	  if (declspecs == short_integer_type_node)
	    strcat (buf, "short int ");
	  else if (declspecs == integer_type_node)
	    strcat (buf, "int ");
	  else if (declspecs == long_integer_type_node)
	    strcat (buf, "long int ");
	  else if (declspecs == long_long_integer_type_node)
	    strcat (buf, "long long int ");
	  else if (declspecs == signed_char_type_node
		   || declspecs == char_type_node)
	    strcat (buf, "char ");

	  /* Unsigned integer types.  */

	  else if (declspecs == short_unsigned_type_node)
	    strcat (buf, "unsigned short ");
	  else if (declspecs == unsigned_type_node)
	    strcat (buf, "unsigned int ");
	  else if (declspecs == long_unsigned_type_node)
	    strcat (buf, "unsigned long ");
	  else if (declspecs == long_long_unsigned_type_node)
	    strcat (buf, "unsigned long long ");
	  else if (declspecs == unsigned_char_type_node)
	    strcat (buf, "unsigned char ");
	  break;

	case REAL_TYPE:
	  declspecs = TYPE_MAIN_VARIANT (declspecs);

	  if (declspecs == float_type_node)
	    strcat (buf, "float ");
	  else if (declspecs == double_type_node)
	    strcat (buf, "double ");
	  else if (declspecs == long_double_type_node)
	    strcat (buf, "long double ");
	  break;

	case RECORD_TYPE:
	  /* APPLE LOCAL begin type aliasing */
	  if (OBJC_TYPE_NAME (declspecs)
	      && TREE_CODE (OBJC_TYPE_NAME (declspecs)) == IDENTIFIER_NODE)
	    {
	      /* APPLE LOCAL SPEC */
	      tree protocol_list = type_protocol_list (declspecs);

	      if (! TREE_STATIC_TEMPLATE (declspecs))
		strcat (buf, "struct ");
	      strcat (buf, IDENTIFIER_POINTER (OBJC_TYPE_NAME (declspecs)));
	      /* APPLE LOCAL end type aliasing */

	      if (protocol_list)
		{
		  tree chain = protocol_list;

		  strcat (buf, " <");
		  while (chain)
		    {
		      strcat (buf,
			      IDENTIFIER_POINTER
			      (PROTOCOL_NAME (TREE_VALUE (chain))));
		      chain = TREE_CHAIN (chain);
		      if (chain)
			strcat (buf, ", ");
		    }
		  strcat (buf, ">");
		}
	    }

	  else
	    strcat (buf, "untagged struct");

	  strcat (buf, " ");
	  break;

	case UNION_TYPE:
	  /* APPLE LOCAL begin type aliasing */
	  if (OBJC_TYPE_NAME (declspecs)
	      && TREE_CODE (OBJC_TYPE_NAME (declspecs)) == IDENTIFIER_NODE)
	    {
	      strcat (buf, "union ");
	      strcat (buf, IDENTIFIER_POINTER (OBJC_TYPE_NAME (declspecs)));
	      /* APPLE LOCAL end type aliasing */
	      strcat (buf, " ");
	    }

	  else
	    strcat (buf, "untagged union ");
	  break;

	case ENUMERAL_TYPE:
	  /* APPLE LOCAL begin type aliasing */
	  if (OBJC_TYPE_NAME (declspecs)
	      && TREE_CODE (OBJC_TYPE_NAME (declspecs)) == IDENTIFIER_NODE)
	    {
	      strcat (buf, "enum ");
	      strcat (buf, IDENTIFIER_POINTER (OBJC_TYPE_NAME (declspecs)));
	      /* APPLE LOCAL end type aliasing */
	      strcat (buf, " ");
	    }

	  else
	    strcat (buf, "untagged enum ");
	  break;

	case VOID_TYPE:
	  strcat (buf, "void ");
	  break;

	case POINTER_TYPE:
	  {
	    /* APPLE LOCAL SPEC */
	    tree protocol_list = type_protocol_list (declspecs);

	    strcat (buf, "id");
	    if (protocol_list)
	      {
		tree chain = protocol_list;

		strcat (buf, " <");
		while (chain)
		  {
		    strcat (buf,
			    IDENTIFIER_POINTER
			    (PROTOCOL_NAME (TREE_VALUE (chain))));
		    chain = TREE_CHAIN (chain);
		    if (chain)
		      strcat (buf, ", ");
		  }

		strcat (buf, ">");
	      }
	  }
	  break;
	  
	default:
	  break;
	}
    }
}

/* Given a tree node, produce a printable description of it in the given
   buffer, overwriting the buffer.  */

static char *
gen_declaration (atype_or_adecl, buf)
     tree atype_or_adecl;
     char *buf;
{
  buf[0] = '\0';
  gen_declaration_1 (atype_or_adecl, buf);
  return buf;
}

/* Given a tree node, append a printable description to the end of the
   given buffer.  */

static void
gen_declaration_1 (atype_or_adecl, buf)
     tree atype_or_adecl;
     char *buf;
{
  char declbuf[256];

  if (TREE_CODE (atype_or_adecl) == TREE_LIST)
    {
      tree declspecs;	/* "identifier_node", "record_type" */
      tree declarator;	/* "array_ref", "indirect_ref", "call_expr"...  */
      /* APPLE LOCAL begin bitfields */
      tree width = NULL_TREE;	/* for bitfields */

      /* We have a "raw", abstract declarator (typename).  */
      declarator = TREE_VALUE (atype_or_adecl);
      /* In the case of raw ivars, the declarator itself is a list,
	 and contains bitfield widths.  */
      if (declarator && TREE_CODE (declarator) == TREE_LIST)
	{
	  width = TREE_VALUE (declarator);
	  declarator = TREE_PURPOSE (declarator);
	}
      /* APPLE LOCAL end bitfields */
      declspecs  = TREE_PURPOSE (atype_or_adecl);

      gen_declspecs (declspecs, buf, 1);
      if (declarator)
	{
	  strcat (buf, " ");
	  strcat (buf, gen_declarator (declarator, declbuf, ""));
	}
      /* APPLE LOCAL begin bitfields */
      if (width)
	sprintf (buf + strlen (buf), ": %d", TREE_INT_CST_LOW (width));
      /* APPLE LOCAL end bitfields */  
    }

  else
    {
      tree atype;
      tree declspecs;	/* "integer_type", "real_type", "record_type"...  */
      tree declarator;	/* "array_type", "function_type", "pointer_type".  */

      if (TREE_CODE (atype_or_adecl) == FIELD_DECL
	  || TREE_CODE (atype_or_adecl) == PARM_DECL
	  || TREE_CODE (atype_or_adecl) == FUNCTION_DECL)
	atype = TREE_TYPE (atype_or_adecl);
      else
	/* Assume we have a *_type node.  */
	atype = atype_or_adecl;

      if (is_complex_decl (atype))
	{
	  tree chain;

	  /* Get the declaration specifier; it is at the end of the list.  */
	  declarator = chain = atype;
	  do
	    chain = TREE_TYPE (chain); /* not TREE_CHAIN (chain); */
	  while (is_complex_decl (chain));
	  declspecs = chain;
	}

      else
	{
	  declspecs = atype;
	  declarator = NULL_TREE;
	}

      gen_declspecs (declspecs, buf, 0);

      if (TREE_CODE (atype_or_adecl) == FIELD_DECL
	  || TREE_CODE (atype_or_adecl) == PARM_DECL
	  || TREE_CODE (atype_or_adecl) == FUNCTION_DECL)
	{
	  const char *const decl_name =
	    (DECL_NAME (atype_or_adecl)
	     ? IDENTIFIER_POINTER (DECL_NAME (atype_or_adecl)) : "");

	  if (declarator)
	    {
	      strcat (buf, " ");
	      strcat (buf, gen_declarator (declarator, declbuf, decl_name));
	    }

	  else if (decl_name[0])
	    {
	      strcat (buf, " ");
	      strcat (buf, decl_name);
	    }
	}
      else if (declarator)
	{
	  strcat (buf, " ");
	  strcat (buf, gen_declarator (declarator, declbuf, ""));
	}
    }
}

#define RAW_TYPESPEC(meth) (TREE_VALUE (TREE_PURPOSE (TREE_TYPE (meth))))

/* Given a method tree, put a printable description into the given
   buffer (overwriting) and return a pointer to the buffer.  */

static char *
gen_method_decl (method, buf)
     tree method;
     char *buf;
{
  tree chain;

  buf[0] = '\0';
  if (RAW_TYPESPEC (method) != objc_object_reference)
    {
      strcat (buf, "(");
      gen_declaration_1 (TREE_TYPE (method), buf);
      strcat (buf, ")");
    }

  chain = METHOD_SEL_ARGS (method);
  if (chain)
    {
      /* We have a chain of keyword_decls.  */
      do
        {
	  if (KEYWORD_KEY_NAME (chain))
	    strcat (buf, IDENTIFIER_POINTER (KEYWORD_KEY_NAME (chain)));

	  strcat (buf, ":");
	  if (RAW_TYPESPEC (chain) != objc_object_reference)
	    {
	      strcat (buf, "(");
	      gen_declaration_1 (TREE_TYPE (chain), buf);
	      strcat (buf, ")");
	    }

	  strcat (buf, IDENTIFIER_POINTER (KEYWORD_ARG_NAME (chain)));
	  if ((chain = TREE_CHAIN (chain)))
	    strcat (buf, " ");
        }
      while (chain);

      if (METHOD_ADD_ARGS (method) == objc_ellipsis_node)
        strcat (buf, ", ...");
      else if (METHOD_ADD_ARGS (method))
        {
	  /* We have a tree list node as generate by get_parm_info.  */
	  chain  = TREE_PURPOSE (METHOD_ADD_ARGS (method));

          /* Know we have a chain of parm_decls.  */
          while (chain)
            {
	      strcat (buf, ", ");
	      gen_declaration_1 (chain, buf);
	      chain = TREE_CHAIN (chain);
            }
	}
    }

  else
    /* We have a unary selector.  */
    strcat (buf, IDENTIFIER_POINTER (METHOD_SEL_NAME (method)));

  return buf;
}

/* Debug info.  */


/* Dump an @interface declaration of the supplied class CHAIN to the
   supplied file FP.  Used to implement the -gen-decls option (which
   prints out an @interface declaration of all classes compiled in
   this run); potentially useful for debugging the compiler too.  */
static void
dump_interface (fp, chain)
     FILE *fp;
     tree chain;
{
  /* FIXME: A heap overflow here whenever a method (or ivar)
     declaration is so long that it doesn't fit in the buffer.  The
     code and all the related functions should be rewritten to avoid
     using fixed size buffers.  */
  char *buf = (char *) xmalloc (1024 * 10);
  const char *my_name = IDENTIFIER_POINTER (CLASS_NAME (chain));
  tree ivar_decls = CLASS_RAW_IVARS (chain);
  tree nst_methods = CLASS_NST_METHODS (chain);
  tree cls_methods = CLASS_CLS_METHODS (chain);

  fprintf (fp, "\n@interface %s", my_name);

  /* CLASS_SUPER_NAME is used to store the superclass name for 
     classes, and the category name for categories.  */
  if (CLASS_SUPER_NAME (chain))
    {
      const char *name = IDENTIFIER_POINTER (CLASS_SUPER_NAME (chain));
      
      if (TREE_CODE (chain) == CATEGORY_IMPLEMENTATION_TYPE 
	  || TREE_CODE (chain) == CATEGORY_INTERFACE_TYPE)
	{
	  fprintf (fp, " (%s)\n", name);
	}
      else
	{
	  fprintf (fp, " : %s\n", name);
	}
    }
  else
    fprintf (fp, "\n");

  /* FIXME - the following doesn't seem to work at the moment.  */
  if (ivar_decls)
    {
      fprintf (fp, "{\n");
      do
	{
	  fprintf (fp, "\t%s;\n", gen_declaration (ivar_decls, buf));
	  ivar_decls = TREE_CHAIN (ivar_decls);
	}
      while (ivar_decls);
      fprintf (fp, "}\n");
    }

  while (nst_methods)
    {
      fprintf (fp, "- %s;\n", gen_method_decl (nst_methods, buf));
      nst_methods = TREE_CHAIN (nst_methods);
    }

  while (cls_methods)
    {
      fprintf (fp, "+ %s;\n", gen_method_decl (cls_methods, buf));
      cls_methods = TREE_CHAIN (cls_methods);
    }

  fprintf (fp, "@end\n");
}

/* Demangle function for Objective-C */
static const char *
objc_demangle (mangled)
     const char *mangled;
{
  char *demangled, *cp;

  if (mangled[0] == '_' &&
      (mangled[1] == 'i' || mangled[1] == 'c') &&
      mangled[2] == '_')
    {
      cp = demangled = xmalloc(strlen(mangled) + 2);
      if (mangled[1] == 'i')
	*cp++ = '-';            /* for instance method */
      else
	*cp++ = '+';            /* for class method */
      *cp++ = '[';              /* opening left brace */
      strcpy(cp, mangled+3);    /* tack on the rest of the mangled name */
      while (*cp && *cp == '_')
	cp++;                   /* skip any initial underbars in class name */
      cp = strchr(cp, '_');     /* find first non-initial underbar */
      if (cp == NULL)
	{
	  free(demangled);      /* not mangled name */
	  return mangled;
	}
      if (cp[1] == '_')  /* easy case: no category name */
	{
	  *cp++ = ' ';            /* replace two '_' with one ' ' */
	  strcpy(cp, mangled + (cp - demangled) + 2);
	}
      else
	{
	  *cp++ = '(';            /* less easy case: category name */
	  cp = strchr(cp, '_');
	  if (cp == 0)
	    {
	      free(demangled);    /* not mangled name */
	      return mangled;
	    }
	  *cp++ = ')';
	  *cp++ = ' ';            /* overwriting 1st char of method name... */
	  strcpy(cp, mangled + (cp - demangled)); /* get it back */
	}
      while (*cp && *cp == '_')
	cp++;                   /* skip any initial underbars in method name */
      for (; *cp; cp++)
	if (*cp == '_')
	  *cp = ':';            /* replace remaining '_' with ':' */
      *cp++ = ']';              /* closing right brace */
      *cp++ = 0;                /* string terminator */
      return demangled;
    }
  else
    return mangled;             /* not an objc mangled name */
}

const char *
objc_printable_name (decl, kind)
     tree decl;
     int kind ATTRIBUTE_UNUSED;
{
  return objc_demangle (IDENTIFIER_POINTER (DECL_NAME (decl)));
}

static void
init_objc ()
{
  /* APPLE LOCAL indexing dpatel */
  flag_suppress_builtin_indexing = 1;

  gcc_obstack_init (&util_obstack);
  util_firstobj = (char *) obstack_finish (&util_obstack);

  errbuf = (char *) xmalloc (BUFSIZE);
  hash_init ();
  synth_module_prologue ();
  /* APPLE LOCAL indexing dpatel */
  flag_suppress_builtin_indexing = 0;
}

static void
finish_objc ()
{
  struct imp_entry *impent;
  tree chain;
  /* The internally generated initializers appear to have missing braces.
     Don't warn about this.  */
  int save_warn_missing_braces = warn_missing_braces;
  warn_missing_braces = 0;

  /* APPLE LOCAL indexing dpatel */
  flag_suppress_builtin_indexing = 1;

  /* A missing @end may not be detected by the parser.  */
  if (objc_implementation_context)
    {
      warning ("`@end' missing in implementation context");
      finish_class (objc_implementation_context);
      objc_ivar_chain = NULL_TREE;
      objc_implementation_context = NULL_TREE;
    }

  generate_forward_declaration_to_string_table ();

#ifdef OBJC_PROLOGUE
  OBJC_PROLOGUE;
#endif

  /* Process the static instances here because initialization of objc_symtab
     depends on them.  */
  if (objc_static_instances)
    generate_static_references ();

  if (imp_list || class_names_chain
      || meth_var_names_chain || meth_var_types_chain || sel_ref_chain)
    generate_objc_symtab_decl ();

  for (impent = imp_list; impent; impent = impent->next)
    {
      objc_implementation_context = impent->imp_context;
      implementation_template = impent->imp_template;

      UOBJC_CLASS_decl = impent->class_decl;
      UOBJC_METACLASS_decl = impent->meta_decl;
      
      /* Dump the @interface of each class as we compile it, if the
	 -gen-decls option is in use.  TODO: Dump the classes in the
         order they were found, rather than in reverse order as we
         are doing now.  */
      if (flag_gen_declaration)
	{
	  dump_interface (gen_declaration_file, objc_implementation_context);
	}
      
      if (TREE_CODE (objc_implementation_context) == CLASS_IMPLEMENTATION_TYPE)
	{
	  /* all of the following reference the string pool...  */
	  generate_ivar_lists ();
	  generate_dispatch_tables ();
	  generate_shared_structures ();
	}
      else
	{
	  generate_dispatch_tables ();
	  generate_category (objc_implementation_context);
	}
    }

  /* If we are using an array of selectors, we must always
     finish up the array decl even if no selectors were used.  */
  if (! flag_next_runtime || sel_ref_chain)
    build_selector_translation_table ();

  if (protocol_chain)
    generate_protocols ();

  /* APPLE LOCAL begin fix and continue */
  if (flag_replace_objc_classes && imp_list)
    generate_objc_image_info ();
  /* APPLE LOCAL end fix and continue */
  
  if (objc_implementation_context || class_names_chain || objc_static_instances
      || meth_var_names_chain || meth_var_types_chain || sel_ref_chain)
    {
      /* Arrange for ObjC data structures to be initialized at run time.  */
      rtx init_sym = build_module_descriptor ();
      if (init_sym && targetm.have_ctors_dtors)
	(* targetm.asm_out.constructor) (init_sym, DEFAULT_INIT_PRIORITY);
    }

  /* Dump the class references.  This forces the appropriate classes
     to be linked into the executable image, preserving unix archive
     semantics.  This can be removed when we move to a more dynamically
     linked environment.  */

  for (chain = cls_ref_chain; chain; chain = TREE_CHAIN (chain))
    {
      handle_class_ref (chain);
      if (TREE_PURPOSE (chain))
	generate_classref_translation_entry (chain);
    }

  for (impent = imp_list; impent; impent = impent->next)
    handle_impent (impent);

  /* Dump the string table last.  */

  generate_strings ();

  if (warn_selector)
    {
      int slot;
      hash hsh;

      /* Run through the selector hash tables and print a warning for any
         selector which has multiple methods.  */

      for (slot = 0; slot < SIZEHASHTABLE; slot++)
	for (hsh = cls_method_hash_list[slot]; hsh; hsh = hsh->next)
	  if (hsh->list)
	    {
	      tree meth = hsh->key;
	      char type = (TREE_CODE (meth) == INSTANCE_METHOD_DECL
			   ? '-' : '+');
	      attr loop;

	      warning ("potential selector conflict for method `%s'",
		       IDENTIFIER_POINTER (METHOD_SEL_NAME (meth)));
	      warn_with_method ("found", type, meth);
	      for (loop = hsh->list; loop; loop = loop->next)
		warn_with_method ("found", type, loop->value);
	    }

      for (slot = 0; slot < SIZEHASHTABLE; slot++)
	for (hsh = nst_method_hash_list[slot]; hsh; hsh = hsh->next)
	  if (hsh->list)
	    {
	      tree meth = hsh->key;
	      char type = (TREE_CODE (meth) == INSTANCE_METHOD_DECL
			   ? '-' : '+');
	      attr loop;

	      warning ("potential selector conflict for method `%s'",
		       IDENTIFIER_POINTER (METHOD_SEL_NAME (meth)));
	      warn_with_method ("found", type, meth);
	      for (loop = hsh->list; loop; loop = loop->next)
		warn_with_method ("found", type, loop->value);
	    }
    }

  warn_missing_braces = save_warn_missing_braces;

  /* APPLE LOCAL indexing dpatel */
  flag_suppress_builtin_indexing = 0;
}

/* Subroutines of finish_objc.  */

static void
generate_classref_translation_entry (chain)
     tree chain;
{
  tree expr, name, decl_specs, decl, sc_spec;
  tree type;

  type = TREE_TYPE (TREE_PURPOSE (chain));

  expr = add_objc_string (TREE_VALUE (chain), class_names);
  expr = build_c_cast (type, expr); /* cast! */

  name = DECL_NAME (TREE_PURPOSE (chain));

  sc_spec = build_tree_list (NULL_TREE, ridpointers[(int) RID_STATIC]);

  /* static struct objc_class * _OBJC_CLASS_REFERENCES_n = ...; */
  decl_specs = tree_cons (NULL_TREE, type, sc_spec);

  /* The decl that is returned from start_decl is the one that we
     forward declared in build_class_reference.  */
  decl = start_decl (name, decl_specs, 1, NULL_TREE);
  DECL_CONTEXT (decl) = NULL_TREE;
  finish_decl (decl, expr, NULL_TREE);
  return;
}

static void
handle_class_ref (chain)
     tree chain;
{
  const char *name = IDENTIFIER_POINTER (TREE_VALUE (chain));
  char *string = (char *) alloca (strlen (name) + 30);
  tree decl;
  tree exp;

  sprintf (string, "%sobjc_class_name_%s",
	   (flag_next_runtime ? "." : "__"), name);

#ifdef ASM_DECLARE_UNRESOLVED_REFERENCE
  if (flag_next_runtime)
    {
      ASM_DECLARE_UNRESOLVED_REFERENCE (asm_out_file, string);
      return;
    }
#endif

  /* Make a decl for this name, so we can use its address in a tree.  */
  decl = build_decl (VAR_DECL, get_identifier (string), char_type_node);
  DECL_EXTERNAL (decl) = 1;
  TREE_PUBLIC (decl) = 1;

  pushdecl (decl);
  rest_of_decl_compilation (decl, 0, 0, 0);

  /* Make a decl for the address.  */
  sprintf (string, "%sobjc_class_ref_%s",
	   (flag_next_runtime ? "." : "__"), name);
  exp = build1 (ADDR_EXPR, string_type_node, decl);
  decl = build_decl (VAR_DECL, get_identifier (string), string_type_node);
  DECL_INITIAL (decl) = exp;
  TREE_STATIC (decl) = 1;
  TREE_USED (decl) = 1;

  pushdecl (decl);
  rest_of_decl_compilation (decl, 0, 0, 0);
}

static void
handle_impent (impent)
     struct imp_entry *impent;
{
  char *string;

  objc_implementation_context = impent->imp_context;
  implementation_template = impent->imp_template;

  if (TREE_CODE (impent->imp_context) == CLASS_IMPLEMENTATION_TYPE)
    {
      const char *const class_name =
	IDENTIFIER_POINTER (CLASS_NAME (impent->imp_context));

      string = (char *) alloca (strlen (class_name) + 30);

      sprintf (string, "%sobjc_class_name_%s",
               (flag_next_runtime ? "." : "__"), class_name);
    }
  else if (TREE_CODE (impent->imp_context) == CATEGORY_IMPLEMENTATION_TYPE)
    {
      const char *const class_name =
	IDENTIFIER_POINTER (CLASS_NAME (impent->imp_context));
      const char *const class_super_name =
        IDENTIFIER_POINTER (CLASS_SUPER_NAME (impent->imp_context));

      string = (char *) alloca (strlen (class_name)
				+ strlen (class_super_name) + 30);

      /* Do the same for categories.  Even though no references to
         these symbols are generated automatically by the compiler, it
         gives you a handle to pull them into an archive by hand.  */
      sprintf (string, "*%sobjc_category_name_%s_%s",
               (flag_next_runtime ? "." : "__"), class_name, class_super_name);
    }
  else
    return;

#ifdef ASM_DECLARE_CLASS_REFERENCE
  if (flag_next_runtime)
    {
      ASM_DECLARE_CLASS_REFERENCE (asm_out_file, string);
      return;
    }
  else
#endif
    {
      tree decl, init;

      init = build_int_2 (0, 0);
      TREE_TYPE (init) = c_common_type_for_size (BITS_PER_WORD, 1);
      decl = build_decl (VAR_DECL, get_identifier (string), TREE_TYPE (init));
      TREE_PUBLIC (decl) = 1;
      TREE_READONLY (decl) = 1;
      TREE_USED (decl) = 1;
      TREE_CONSTANT (decl) = 1;
      DECL_CONTEXT (decl) = 0;
      DECL_ARTIFICIAL (decl) = 1;
      DECL_INITIAL (decl) = init;
      assemble_variable (decl, 1, 0, 0);
    }
}

/* APPLE LOCAL begin fix and continue */

/* static int _OBJC_IMAGE_INFO[2] = { 0, 1 }; */

static void
generate_objc_image_info ()
{
  tree sc_spec, decl, initlist;

  /* APPLE LOCAL indexing */
  flag_suppress_builtin_indexing = 1;
  
  sc_spec = build_tree_list (NULL_TREE, ridpointers[(int) RID_STATIC]);
  decl 
   = start_decl (get_identifier ("_OBJC_IMAGE_INFO"),
		 tree_cons (NULL_TREE, 
			    build_array_type 
			    (integer_type_node, 
			     build_index_type (build_int_2 (1, 0))), 
			    sc_spec),
		 1,
		 NULL_TREE);

  initlist = build_tree_list (NULL_TREE, build_int_2 (0, 0));
  initlist = tree_cons (NULL_TREE, build_int_2 (1, 0), initlist);
  initlist = build_constructor (TREE_TYPE (decl), nreverse (initlist));

  TREE_USED (decl) = DECL_IGNORED_P (decl) = DECL_ARTIFICIAL (decl) = 1;
  finish_decl (decl, initlist, NULL_TREE);

  /* APPLE LOCAL indexing */
  flag_suppress_builtin_indexing = 0;
}
/* APPLE LOCAL end fix and continue */


/* Look up ID as an instance variable.  */
tree
lookup_objc_ivar (id)
     tree id;
{
  tree decl;

  if (objc_method_context && !strcmp (IDENTIFIER_POINTER (id), "super"))
    /* We have a message to super.  */
    return get_super_receiver ();
  else if (objc_method_context && (decl = is_ivar (objc_ivar_chain, id)))
    {
      if (is_private (decl))
	return error_mark_node;
      else
        return build_ivar_reference (id);
    }
  else
    return 0;
}

#include "gt-objc-objc-act.h"
/* APPLE LOCAL begin Objective-C++ */
#include "gt-objc-objc-act-h.h"
#ifndef OBJCPLUS
tree objc_dummy = 0;
#include "gtype-objc.h"
#endif
/* APPLE LOCAL end Objective-C++ */
