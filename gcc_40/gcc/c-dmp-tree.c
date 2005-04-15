/* APPLE LOCAL new tree dump */
/* Common condensed tree display routines specific for C.
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

#include "config.h"

#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "c-common.h"
#include <string.h>
#include <ctype.h>

#define DMP_TREE
#include "dmp-tree.h"

int c_dump_tree_p (FILE *, const char *, tree, int);
lang_dump_tree_p_t c_prev_lang_dump_tree_p = NULL;

#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) \
static void print_ ## SYM (FILE *file, const char *annotation, tree node, int indent);
#include "c-common.def"
#undef DEFTREECODE

/*-------------------------------------------------------------------*/

/* If CP_DMP_TREE is defined then this file is being #include'ed from
   cp/cp-dmp-tree.c.  It needs to handle both C and C++ nodes.  The C++
   nodes are handled from cp/cp-dmp-tree.c and it use c-dmp-tree.c to
   handle the C nodes.  */
   
#ifndef CP_DMP_TREE
#include "c-tree.h"

/* Called twice for dmp_tree() for an IDENTIFIER_NODE.  The first call
   is after the common info for the node is generated but before
   displaying the identifier (before_id==0) which is always assumed
   to be the last thing on the line.
   
   The second call is done after the id is displayed (before_id!=0).
   This is for displaying any language-specific node information that
   should be preceded by an indent_to() call or a recursive call to
   dump_tree() for nodes which are language specific operands to a
   IDENTIFIER_NODE.  */
  
void 
c_dump_identifier (file, node, indent, after_id)
     FILE *file;
     tree node;
     int indent;
     int after_id;
{
  if (!after_id)
    {
      if (C_IS_RESERVED_WORD (node))
	{
	  tree rid = ridpointers[C_RID_CODE (node)];
	  fprintf (file, " rid=");
	  fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE((void *)rid));
	  fprintf (file, "(%s)", IDENTIFIER_POINTER (rid));
	}
      if (IDENTIFIER_LABEL_VALUE (node))
	{
	  fprintf (file, " lbl=");
	  fprintf (file, HOST_PTR_PRINTF, IDENTIFIER_LABEL_VALUE (node));
	}
    }
  else
    {
      dump_tree (file, "(lbl)", IDENTIFIER_LABEL_VALUE (node), indent + INDENT);
    }
}

/* Called twice for dmp_tree() for a ..._DECL node.  The first call
   after the common info for the node is generated but before
   displaying the identifer (before_id==0) which is always assumed
   to be the last thing on the line.
   
   The second call is done after the id is displayed (before_id!=0).
   This is for displaying any language-specific node information that
   should be preceded by an indent_to() call or a recursive call to
   dump_tree() for nodes which are language specific operands to a
   a ..._DECL node.  */
    
void 
c_dump_decl (file, node, indent, after_id)
     FILE *file ATTRIBUTE_UNUSED;
     tree node ATTRIBUTE_UNUSED;
     int indent ATTRIBUTE_UNUSED;
     int after_id ATTRIBUTE_UNUSED;
{
}

/* Called twice for dmp_tree() for a ..._TYPE node.  The first call
   after the common info for the node is generated but before
   displaying the identifier (before_id==0) which is always assumed
   to be the last thing on the line.
   
   The second call is done after the id is displayed (before_id!=0).
   This is for displaying any language-specific node information that
   should be preceded by an indent_to() call or a recursive call to
   dump_tree() for nodes which are language specific operands to a
   a ..._TYPE node.  */

void 
c_dump_type (file, node, indent, after_id)
     FILE *file ATTRIBUTE_UNUSED;
     tree node ATTRIBUTE_UNUSED;
     int indent ATTRIBUTE_UNUSED;
     int after_id ATTRIBUTE_UNUSED;
{
}

/* Normally a blank line is inserted before each statement node (a 
   statement node is determined by calling statement_code_p()).  This
   makes the display easier to read by keeping each statement grouped
   like a paragraph.  There may, however, be some kinds of statements
   where a blank line isn't desired (e.g., a begin SCOPE_STMT in C).
   Thus dump_lang_blank_line() is called to ask if a particular 
   statement should be preceded by a blank line dependent upon the
   node that preceded it.
   
   dump_lang_blank_line_p() is called for each statement passing the
   previous node (not necessarily a statement) and current node (a
   statement node by definition).  It should return 1 if a blank
   line is to be inserted and 0 otherwise.  */

int
c_dump_blank_line_p (previous_node, current_node)
     tree previous_node;
     tree current_node;
{
  return (TREE_CODE (current_node) != SCOPE_STMT
	  && !(TREE_CODE (previous_node) == SCOPE_STMT 
	       && SCOPE_BEGIN_P (previous_node)));
}

/* This is called for each node to display file and/or line number
   information for those nodes that have such information.  If it
   is displayed the function should return 1.  If not, 0.
   
   The function generally does not have to handle ..._DECL nodes
   unless there some special handling is reequired.  They are
   handled by print_lineno() (dump_lang_lineno_p()'s caller).
   It is defined to not repeat the filename if it does not
   change from what's in dump_tree_state.curr_file and then
   it only displays the basename (using lbasename()).  The
   format of the display is " line=nbr(basename)" where the
   leading space is included as usual in these displays and
   the parenthesized basename omitted if not needed or is
   the same as before.  */
   
int
c_dump_lineno_p (file, node)
     FILE *file ATTRIBUTE_UNUSED;
     tree node ATTRIBUTE_UNUSED;
{
  return 0;
}

/* Called only by tree-dump.c when doing a full compilation tree dump
   under one of the -fdmp-xxxx options.  This makes tree_dump.c, which
   is common to all languages, independent of dmp_tree, which currently
   only supports the c languages.  */
int 
c_dmp_tree3 (file, node, flags)
     FILE *file;
     tree node;
     int flags;
{
  dmp_tree3 (file, node, flags);
  return 1;
}

#endif /* !CP_DMP_TREE */

/*-------------------------------------------------------------------*/

static void
print_SIZEOF_EXPR (file, annotation, node, indent)
     FILE *file ATTRIBUTE_UNUSED;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node ATTRIBUTE_UNUSED;
     int indent ATTRIBUTE_UNUSED;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_ARROW_EXPR (file, annotation, node, indent)
     FILE *file ATTRIBUTE_UNUSED;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node ATTRIBUTE_UNUSED;
     int indent ATTRIBUTE_UNUSED;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_ALIGNOF_EXPR (file, annotation, node, indent)
     FILE *file ATTRIBUTE_UNUSED;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node ATTRIBUTE_UNUSED;
     int indent ATTRIBUTE_UNUSED;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_EXPR_STMT (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_COMPOUND_STMT (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, FALSE, NULL);
  
  for (node = COMPOUND_BODY (node); node; node = TREE_CHAIN (node))
    {
      if (TREE_CODE (node) == SCOPE_STMT && SCOPE_END_P (node) && indent >= INDENT)
      	indent -= INDENT;
      	
      dump_tree (file, NULL, node, indent + INDENT);
      
      if (TREE_CODE (node) == SCOPE_STMT && SCOPE_BEGIN_P (node))
      	indent += INDENT;
    }
}

static void
print_DECL_STMT (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  tree type;
  
  fprintf (file, " %s=", tree_code_name[(int) TREE_CODE (DECL_STMT_DECL (node))]);
  fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE (DECL_STMT_DECL (node)));
  
  type = TREE_TYPE (DECL_STMT_DECL (node));
  if (type && TREE_CODE_CLASS (TREE_CODE (type)) == 't') 
    {
      if (TYPE_NAME (type))
        {
	  if (TREE_CODE (TYPE_NAME (type)) == IDENTIFIER_NODE)
	    fprintf (file, " {%s}", IDENTIFIER_POINTER (TYPE_NAME (type)));
	  else if (TREE_CODE (TYPE_NAME (type)) == TYPE_DECL
		   && DECL_NAME (TYPE_NAME (type)))
	    fprintf (file, " {%s}",
		     IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (type))));
        }
      else
      	fprintf (file, " {%s}", tree_code_name[(int) TREE_CODE (type)]);
    }
   
  if (DECL_NAME ( DECL_STMT_DECL (node)))
    fprintf (file, " %s", IDENTIFIER_POINTER (DECL_NAME ( DECL_STMT_DECL (node))));
 
  if (!node_seen (DECL_STMT_DECL (node), FALSE)
      || TREE_CODE (DECL_STMT_DECL (node)) != VAR_DECL)
    dump_tree (file, NULL, DECL_STMT_DECL (node), indent + INDENT);
}

static void
print_IF_STMT (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, "(if)", "(then)", "(else)", NULL);
}

static void
print_FOR_STMT (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  if (NEW_FOR_SCOPE_P (node))
    fputs (" new-scope", file);
    
  print_operands (file, node, indent, TRUE, "(init)", "(cond)",
  			"(expr)", "(body)", NULL);
}

static void
print_WHILE_STMT (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, "(cond)", "(body)", NULL);
}

static void
print_DO_STMT (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, "(cond)", "(body)", NULL);
}

static void
print_RETURN_STMT (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_BREAK_STMT (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_CONTINUE_STMT (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_SWITCH_STMT (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, "(cond)", "(body)", NULL);
}

static void
print_GOTO_STMT (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_LABEL_STMT (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_ASM_STMT (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  if (ASM_VOLATILE_P (node))
    fputs (" volatile", file);
    
  print_operands (file, node, indent, TRUE, "(cv-qual)", "(string)",
  		 	"(outputs)", "(inputs)", "(clobbers)", NULL);
}

static void
print_SCOPE_STMT (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  if (SCOPE_BEGIN_P (node))
    fputs (" BEGIN", file);
  if (SCOPE_END_P (node))
    fputs (" END", file);
  if (SCOPE_NULLIFIED_P (node))
    fputs (" no-vars", file);
  if (SCOPE_NO_CLEANUPS_P (node))
    fputs (" no-cleanups", file);
  if (SCOPE_PARTIAL_P (node))
    fputs (" partial", file);
  fprintf (file, " block=");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (SCOPE_STMT_BLOCK (node)));
  
  if (SCOPE_BEGIN_P (node) || !node_seen (SCOPE_STMT_BLOCK (node), FALSE))
    dump_tree (file, NULL, SCOPE_STMT_BLOCK (node), indent + INDENT);
  
  (void)node_seen (node, TRUE);
  
  for (node = TREE_CHAIN (node); node; node = TREE_CHAIN (node))
    dump_tree (file, annotation, node, indent + INDENT);
}

static void
print_FILE_STMT (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_CASE_LABEL (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, "(lo)", "(hi)", "(lbl)", NULL);
}

static void
print_STMT_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_COMPOUND_LITERAL_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_CLEANUP_STMT (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, "(decl)", "(expr)", NULL);
}

/*-------------------------------------------------------------------*/

/* Return 1 if tree node is a C++ specific tree node from cp-tree.def
   or a tree node specific to whatever cp_prev_lang_dump_tree_p
   calls.  Otherwise return 0.
*/

int
c_dump_tree_p (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
   switch (TREE_CODE (node)) 
   {
#   define DEFTREECODE(SYM, NAME, TYPE, LENGTH) \
     	   case SYM: print_ ## SYM (file, annotation, node, indent); break;
#   include "c-common.def"
#   undef DEFTREECODE
   default:
     return c_prev_lang_dump_tree_p (file, annotation, node, indent);
   }
   
   return 1;
}


