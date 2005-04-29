/* APPLE LOCAL file new tree dump */
/* Common condensed tree display routines specific for objc and objc++.
   Copyright (C) 2001  Free Software Foundation, Inc.
   Contributed by Ira L. Ruben (ira@apple.com)

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

/* The Objective-C++ and Objective-C tree dump handling is piggybacked
   on either C++ or C tree dump handling.  */
   
#ifdef OBJCPLUS
#include "cp-dmp-tree.c"
#else
#include "c-dmp-tree.c"
#endif

#include "objc-act.h"

int objc_dump_tree_p (FILE *, const char *, tree, int);
lang_dump_tree_p_t objc_prev_lang_dump_tree_p = NULL;

#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) \
static void print_ ## SYM (FILE *file, const char *annotation, tree node, int indent);
#include "objc-tree.def"
#undef DEFTREECODE

/*-------------------------------------------------------------------*/

static void
print_CLASS_INTERFACE_TYPE (file, annotation, node, indent)
     FILE *file ATTRIBUTE_UNUSED;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node ATTRIBUTE_UNUSED;
     int indent ATTRIBUTE_UNUSED;
{
  tree n;
  
  if (CLASS_SUPER_NAME (node))		
    fprintf (file, " super-name=%s",
  		  IDENTIFIER_POINTER (CLASS_SUPER_NAME (node)));
  /* The ivars for the class are accessible either via
     CLASS_IVARS(node), or as fields of the
     underlying C struct (itself accessible via
     CLASS_STATIC_TEMPLATE(node).  */
  fprintf (file, " ivars=");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (CLASS_IVARS (node)));
  /* zlaski 2001-Jul-26:  not sure what the raw ivars are for...  */		
  fprintf (file, " raw_ivars=");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (CLASS_RAW_IVARS (node)));
  /* The "own ivars" list contains just the ivars defined by this
     class (i.e., not inherited).  */		
  fprintf (file, " own_ivars=");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (CLASS_OWN_IVARS (node)));

  print_type (file, annotation, node, indent);
  (void)node_seen (node, TRUE);

  /* print out names of protocols, categories.  */
  for (n = CLASS_PROTOCOL_LIST (node); n; n = TREE_CHAIN (n))
    print_type (file, NULL, TREE_VALUE (n), indent + INDENT);
  for (n = CLASS_CATEGORY_LIST (node); n; n = TREE_CHAIN (n))
    print_type (file, NULL, TREE_VALUE (n), indent + INDENT);
    
  /* Print the underlying RECORD_TYPE node, with fields.  */
  dump_tree (file, annotation, CLASS_STATIC_TEMPLATE (node),
	     indent + INDENT);
  /* Print out names of class and instance methods.  */	     
  for (n = CLASS_CLS_METHODS (node); n; n = TREE_CHAIN (n))
    dump_tree (file, NULL, n, indent + INDENT);
  for (n = CLASS_NST_METHODS (node); n; n = TREE_CHAIN (n))
    dump_tree (file, NULL, n, indent + INDENT);
}

static void
print_CLASS_IMPLEMENTATION_TYPE (file, annotation, node, indent)
     FILE *file ATTRIBUTE_UNUSED;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node ATTRIBUTE_UNUSED;
     int indent ATTRIBUTE_UNUSED;
{
  print_CLASS_INTERFACE_TYPE (file, annotation, node, indent);
}

static void
print_CATEGORY_INTERFACE_TYPE (file, annotation, node, indent)
     FILE *file ATTRIBUTE_UNUSED;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node ATTRIBUTE_UNUSED;
     int indent ATTRIBUTE_UNUSED;
{
  tree n;
  
  fprintf (file, " cat-name=%s",
  		  IDENTIFIER_POINTER (CLASS_SUPER_NAME (node)));
  print_type (file, annotation, node, indent);
  (void)node_seen (node, TRUE);
  
  for (n = CLASS_PROTOCOL_LIST (node); n; n = TREE_CHAIN (n))
    dump_tree (file, NULL, TREE_VALUE (n), indent + INDENT);
  /* Print out names of class and instance methods.  */	     
  for (n = CLASS_CLS_METHODS (node); n; n = TREE_CHAIN (n))
    dump_tree (file, NULL, n, indent + INDENT);
  for (n = CLASS_NST_METHODS (node); n; n = TREE_CHAIN (n))
    dump_tree (file, NULL, n, indent + INDENT);
}

static void
print_CATEGORY_IMPLEMENTATION_TYPE (file, annotation, node, indent)
     FILE *file ATTRIBUTE_UNUSED;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node ATTRIBUTE_UNUSED;
     int indent ATTRIBUTE_UNUSED;
{
  print_CATEGORY_INTERFACE_TYPE (file, annotation, node, indent);
}

static void
print_PROTOCOL_INTERFACE_TYPE (file, annotation, node, indent)
     FILE *file ATTRIBUTE_UNUSED;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node ATTRIBUTE_UNUSED;
     int indent ATTRIBUTE_UNUSED;
{
  tree n;
  
  fprintf (file, " fwd-decl=");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (PROTOCOL_FORWARD_DECL (node)));
  fprintf (file, " defined=%d", PROTOCOL_DEFINED (node));

  print_type (file, annotation, node, indent);
  (void)node_seen (node, TRUE);
  
  /* print superprotocols, if any.  */
  for (n = PROTOCOL_LIST (node); n; n = TREE_CHAIN (n))
    dump_tree (file, NULL, TREE_VALUE (n), indent + INDENT);
    
  /* Print out names of class and instance methods.  */	     
  for (n = PROTOCOL_CLS_METHODS (node); n; n = TREE_CHAIN (n))
    dump_tree (file, NULL, n, indent + INDENT);
  for (n = PROTOCOL_NST_METHODS (node); n; n = TREE_CHAIN (n))
    dump_tree (file, NULL, n, indent + INDENT);
}

static void
print_KEYWORD_DECL (file, annotation, node, indent)
     FILE *file ATTRIBUTE_UNUSED;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node ATTRIBUTE_UNUSED;
     int indent ATTRIBUTE_UNUSED;
{
  fprintf (file, " key=%s",
  		IDENTIFIER_POINTER (KEYWORD_KEY_NAME (node)));
  fprintf (file, " arg=%s",
  		IDENTIFIER_POINTER (KEYWORD_ARG_NAME (node)));
		
  print_type (file, annotation, node, indent);
  (void)node_seen (node, TRUE);
}

static void
print_INSTANCE_METHOD_DECL (file, annotation, node, indent)
     FILE *file ATTRIBUTE_UNUSED;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node ATTRIBUTE_UNUSED;
     int indent ATTRIBUTE_UNUSED;
{
  fprintf (file, " args=");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (METHOD_SEL_ARGS (node)));
  fprintf (file, " addl=");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (METHOD_ADD_ARGS (node)));
  if (METHOD_ENCODING (node))		
    fprintf (file, " encode=%s",
  		  IDENTIFIER_POINTER (METHOD_ENCODING (node)));

  print_decl (file, annotation, node, indent);
  
  /* Print the underlying FUNCTION_DECL node.  */
  if (METHOD_DEFINITION (node))
    dump_tree (file, annotation, METHOD_DEFINITION (node),
  	       indent + INDENT);
  
  (void)node_seen (node, TRUE);
}

static void
print_CLASS_METHOD_DECL (file, annotation, node, indent)
     FILE *file ATTRIBUTE_UNUSED;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node ATTRIBUTE_UNUSED;
     int indent ATTRIBUTE_UNUSED;
{
  print_INSTANCE_METHOD_DECL (file, annotation, node, indent);
}

static void
print_CLASS_REFERENCE_EXPR (file, annotation, node, indent)
     FILE *file ATTRIBUTE_UNUSED;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node ATTRIBUTE_UNUSED;
     int indent ATTRIBUTE_UNUSED;
{
  fprintf (file, " ident=");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (TREE_OPERAND (node, 0)));
  (void)node_seen (node, TRUE);
}

static void
print_MESSAGE_SEND_EXPR (file, annotation, node, indent)
     FILE *file ATTRIBUTE_UNUSED;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node ATTRIBUTE_UNUSED;
     int indent ATTRIBUTE_UNUSED;
{
  fprintf (file, " receiver=");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (TREE_OPERAND (node, 0)));
  fprintf (file, " sel_name=%s",
  		IDENTIFIER_POINTER (TREE_OPERAND (node, 1)));
  fprintf (file, " args=");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (TREE_OPERAND (node, 2)));
  (void)node_seen (node, TRUE);
}

/*-------------------------------------------------------------------*/

int
objc_dump_tree_p (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
   switch (TREE_CODE (node)) 
   {
#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) \
     	   case SYM: print_ ## SYM (file, annotation, node, indent); break;
#include "objc-tree.def"
#undef DEFTREECODE
   default:
     return objc_prev_lang_dump_tree_p (file, annotation, node, indent);
   }
   
   return 1;
}


/*-------------------------------------------------------------------*/

#if 0

cd $gcc3/gcc; \
cc -no-cpp-precomp -c  -DIN_GCC  -g \
  -W -Wall -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wtraditional -pedantic -Wno-long-long \
  -DHAVE_CONFIG_H \
  -I$gcc3obj \
  -I. \
  -Iobjc \
  -Iconfig \
  -I../include \
  objc/objc-dmp-tree.c -o ~/tmp.o -w 

#endif
