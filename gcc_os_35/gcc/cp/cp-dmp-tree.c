/* APPLE LOCAL file new tree dump */
/* Common condensed c++ tree display routines. Based on dmp-tree.c
   Copyright (C) 2001  Free Software Foundation, Inc.
   Contributed by Devang Patel (dpatel@apple.com)
   and Ira L. Ruben (ira@apple.com)

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

/* Both C and C++ node handling is required for C++.  The C handling is
   done in c-dmp-tree.c.  But that is a C language specific file, i.e.,
   only built for C.  Thus we need to #include it here to get the stuff
   we need defined.  But we need to tell c-dmp-tree.c that we are doing
   this so it doesn't define stuff we don't want defined.  That's the
   purpose of the CP_DMP_TREE switch.  
   
   Note that c-dmp-tree.c does all the main #includes so we don't need
   them here.  */
   
#define CP_DMP_TREE
#include "c-dmp-tree.c"

#include "cp-tree.h"

int cp_dump_tree_p (FILE *, const char *, tree, int);
lang_dump_tree_p_t cp_prev_lang_dump_tree_p = NULL;

#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) \
static void print_ ## SYM (FILE *file, const char *annotation, tree node, int indent);
#include "cp-tree.def"
#undef DEFTREECODE

static void print_RECORD_TYPE (FILE *, const char *, tree, int);
static void print_NAMESPACE_DECL (FILE *, const char *, tree, int);
static void print_ADDR_EXPR (FILE *, const char *, tree, int);

/*-------------------------------------------------------------------*/

/* Called twice for dmp-tree() for an IDENTIFIER_NODE.  The first call
   is after the common info for the node is generated but before
   displaying the identifer (before_id==0) which is always assumed
   to be the last thing on the line.
   
   The second call is done after the id is displayed (before_id!=0).
   This is for displaying any language-specific node information that
   should be preceded by an newline_and_indent() call or a recursive
   call to dump_tree() for nodes which are language specific operands
   to a IDENTIFIER_NODE.  */
  
void 
cxx_dump_identifier (FILE *file,
		      tree node,
		      int indent ATTRIBUTE_UNUSED,
		      int after_id)
{
  if (!after_id)
    {
      if (C_IS_RESERVED_WORD (node))
        fputs (" reserved", file);
      if (IDENTIFIER_CTOR_OR_DTOR_P (node))
        fputs (" ctor/dtor", file);
      if (IDENTIFIER_NAMESPACE_BINDINGS (node))
	{
	  fprintf (file, " ns-bindings=");
	  fprintf (file, HOST_PTR_PRINTF, IDENTIFIER_NAMESPACE_BINDINGS (node));
	}
      if (IDENTIFIER_CLASS_VALUE (node))
	{
	  fprintf (file, " binding=");
	  fprintf (file, HOST_PTR_PRINTF, IDENTIFIER_CLASS_VALUE (node));
	}
      if (IDENTIFIER_BINDING (node))
	{
	  fprintf (file, " lcl-bindings=");
	  fprintf (file, HOST_PTR_PRINTF, IDENTIFIER_BINDING (node));
	}
      if (IDENTIFIER_LABEL_VALUE (node))
	{
	  fprintf (file, " gbl=");
	  fprintf (file, HOST_PTR_PRINTF, IDENTIFIER_LABEL_VALUE (node));
	}
      if (IDENTIFIER_TEMPLATE (node))
	{
	  fprintf (file, " tmpl=");
	  fprintf (file, HOST_PTR_PRINTF, IDENTIFIER_TEMPLATE (node));
	}
      if (IDENTIFIER_IMPLICIT_DECL (node))
	{
	  fprintf (file, " impl=");
	  fprintf (file, HOST_PTR_PRINTF, IDENTIFIER_IMPLICIT_DECL (node));
	}
      if (IDENTIFIER_ERROR_LOCUS (node))
	{
	  fprintf (file, " err-locus=");
	  fprintf (file, HOST_PTR_PRINTF, IDENTIFIER_ERROR_LOCUS (node));
	}
    }
  else
    {
#if 0
      dump_binding (file, "(bindings)", IDENTIFIER_NAMESPACE_BINDINGS (node), indent + INDENT);
#endif
      dump_tree (file, "(class)", IDENTIFIER_CLASS_VALUE (node), indent + INDENT);
#if 0
      dump_binding (file, "(lcl-bindings)", IDENTIFIER_BINDING (node), indent + INDENT);
#endif
      dump_tree (file, "(lbl)", IDENTIFIER_LABEL_VALUE (node), indent + INDENT);
      dump_tree (file, "(tmpl)", IDENTIFIER_TEMPLATE (node), indent + INDENT);
      dump_tree (file, "(impl)", IDENTIFIER_IMPLICIT_DECL (node), indent + INDENT);
      dump_tree (file, "(err-locus)", IDENTIFIER_ERROR_LOCUS (node), indent + INDENT);
    }
}

/* Called twice for dmp_tree() for a ..._DECL node.  The first call
   after the common info for the node is generated but before
   displaying the identifier (before_id==0) which is always assumed
   to be the last thing on the line.
   
   The second call is done after the id is displayed (before_id!=0).
   This is for displaying any language-specific node information that
   should be preceded by an newline_and_indent() call or a recursive
   call to dump_tree() for nodes which are language specific operands
   to a ..._DECL node.  */
    
void 
cxx_dump_decl (FILE *file, tree node, int indent ATTRIBUTE_UNUSED, int after_id)
{
  switch (TREE_CODE (node))
  {
    case FUNCTION_DECL:
      if (!after_id)
        {
	  if (DECL_STATIC_FUNCTION_P (node))
	    fputs (" static", file);
	  if (DECL_FRIEND_P (node))
	    fputs (" frnd", file);
	  if (DECL_CONSTRUCTOR_P (node))
	      fprintf (file, " %sctor", 
	      		DECL_COPY_CONSTRUCTOR_P (node) ? "cpy-" : "");
	  if (DECL_DESTRUCTOR_P (node))
	    fputs (" dtor", file);
	  if (DECL_PURE_VIRTUAL_P (node))
	    fputs (" pure-virt", file);
	  if (DECL_CONST_MEMFUNC_P (node))
	    fputs (" const", file);
	  if (DECL_VOLATILE_MEMFUNC_P (node))
	    fputs (" volatile", file);
	  if (DECL_MUTABLE_P (node))
	    fputs (" mutable", file);
	  if (DECL_THUNK_P (node))
	    fputs (" thnk", file);
	  if (DECL_LANG_SPECIFIC (node))
	    {
	      if (DECL_PENDING_INLINE_INFO (node))
		{
	          fprintf (file, " pending-inline-info=");
		  fprintf (file, HOST_PTR_PRINTF,
	        	   HOST_PTR_PRINTF_VALUE (DECL_PENDING_INLINE_INFO (node)));
		}
	      if (DECL_TEMPLATE_INFO (node))
		{
	          fprintf (file, " tmpl-info=");
		  fprintf (file, HOST_PTR_PRINTF,
	        	   HOST_PTR_PRINTF_VALUE (DECL_TEMPLATE_INFO (node)));
		}
	    }
        }
      break;
    
    case FIELD_DECL:
      if (!after_id)
        {
          if (DECL_MUTABLE_P (node))
	    fputs (" mutable", file);
        }
      break;
    
    case TYPE_DECL:
      if (!after_id)
        {
	  if (DECL_LANG_SPECIFIC (node))
	    {
	      if (DECL_TEMPLATE_INFO (node))
		{
	          fprintf (file, " tmpl-info=");
		  fprintf (file, HOST_PTR_PRINTF,
	        	   HOST_PTR_PRINTF_VALUE (DECL_TEMPLATE_INFO (node)));
		}
	      if (DECL_SORTED_FIELDS (node))
		{
	          fprintf (file, " sorted-fields=");
		  fprintf (file, HOST_PTR_PRINTF,
	        	   HOST_PTR_PRINTF_VALUE (DECL_SORTED_FIELDS (node)));
		}
	    }
        }
       break;
   
    case VAR_DECL:
      if (!after_id)
        {
	  if (DECL_LANG_SPECIFIC (node))
	    {
	      if (DECL_TEMPLATE_INFO (node))
		{
	          fprintf (file, " tmpl-info=");
		  fprintf (file, HOST_PTR_PRINTF,
	        	   HOST_PTR_PRINTF_VALUE (DECL_TEMPLATE_INFO (node)));
		}
	    }
	  if (DECL_SHADOWED_FOR_VAR (node))
	    fputs (" shadowed", file);
        }
       break;
    
    default:
      break;
  }
}

/* Called twice for dmp_tree() for a ..._TYPE node.  The first call
   after the common info for the node is generated but before
   displaying the identifier (before_id==0) which is always assumed
   to be the last thing on the line.
   
   The second call is done after the id is displayed (before_id!=0).
   This is for displaying any language-specific node information that
   should be preceded by an newline_and_indent() call or a recursive
   call to dump_tree() for nodes which are language specific operands
   to a ..._TYPE node.  */

void 
cxx_dump_type (FILE *file, tree node, int indent, int after_id)
{
  if (!after_id)
    {
      if (CLASS_TYPE_P (node))	/* RECORD_TYPE, UNION_TYPE only */
        {
	  if (TYPE_NEEDS_CONSTRUCTING (node))
	    fputs (" needs-ctor", file);
	  if (TYPE_HAS_NONTRIVIAL_DESTRUCTOR (node))
	    fputs (" needs-dtor", file);
	  if (TYPE_HAS_DESTRUCTOR (node))
	    fputs (" ~X()", file);
	  if (TYPE_HAS_DEFAULT_CONSTRUCTOR (node))
	    fputs (" X()", file);
	  if (TYPE_HAS_CONVERSION (node))
	    fputs (" has-conv", file);
	  if (TYPE_HAS_INIT_REF (node))
	    {
	      if (TYPE_HAS_CONST_INIT_REF (node))
		fputs (" X(constX&)", file);
	      else
		fputs (" X(X&)", file);
	    }
	  if (TYPE_HAS_NEW_OPERATOR (node))
	    fputs (" new", file);
	  if (TYPE_HAS_ARRAY_NEW_OPERATOR (node))
	    fputs (" new[]", file);
	  if (TYPE_GETS_DELETE (node) & 1)
	    fputs (" delete", file);
	  if (TYPE_GETS_DELETE (node) & 2)
	    fputs (" delete[]", file);
	  if (TYPE_HAS_ASSIGN_REF (node))
	    fputs (" this=(X&)", file);
        }
    }
    
  switch (TREE_CODE (node))
    {
    case FUNCTION_TYPE:
    case METHOD_TYPE:
      if (!after_id)
        {
          if (TYPE_RAISES_EXCEPTIONS (node))
	    {
              fprintf (file, " throws=");
	      fprintf (file, HOST_PTR_PRINTF,
            	HOST_PTR_PRINTF_VALUE (TYPE_RAISES_EXCEPTIONS (node)));
	    }
        }
      else
        {
          if (TYPE_RAISES_EXCEPTIONS (node))
	    dump_tree (file, "(throws)", TYPE_RAISES_EXCEPTIONS (node),
	    		indent + INDENT);
        }
      break;
          
    default:
      break;
    }
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
cxx_dump_blank_line_p (tree previous_node ATTRIBUTE_UNUSED,
		       tree current_node ATTRIBUTE_UNUSED)
{
  return 1;
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
cxx_dump_lineno_p (FILE *file ATTRIBUTE_UNUSED, tree node ATTRIBUTE_UNUSED)
{
  return 0;
}

/* Called only by tree-dump.c when doing a full compilation tree dump
   under one of the -fdmp-xxxx options.  This makes tree_dump.c, which
   is common to all languages, independent of dmp_tree, which currently
   only supports the c languages.  */
int 
cxx_dmp_tree3 (file, node, flags)
     FILE *file;
     tree node;
     int flags;
{
  dmp_tree3 (file, node, flags);
  return 1;
}

/*-------------------------------------------------------------------*/

static void
print_OFFSET_REF (FILE *file, const char *annotation, tree node, int indent)
{
  if (PTRMEM_OK_P (node))
    fputs (" ptr-to-mbr-ok", file);
  print_ref (file, annotation, node, indent);
  
  print_operands (file, node, indent, TRUE, "(obj)", "(offset)", NULL);
}

static void
print_NON_DEPENDENT_EXPR (FILE *file, const char *annotation, tree node,
			  int indent)
{
}

static void
print_PTRMEM_CST (FILE *file,
		  const char *annotation ATTRIBUTE_UNUSED,
		  tree node,
		  int indent ATTRIBUTE_UNUSED)
{
  fprintf (file, " rec-type::mbr-decl=");
  fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE (PTRMEM_CST_CLASS (node)));
  fprintf (file, "::");
  fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE (PTRMEM_CST_MEMBER (node)));
  /* not sure I want to follow these nodes here */
}

static void
print_NEW_EXPR (FILE *file,
		const char *annotation ATTRIBUTE_UNUSED,
		tree node,
		int indent)
{
  if (NEW_EXPR_USE_GLOBAL (node))
    fputs ("use-glbl", file);
    
  print_operands (file, node, indent, TRUE, "(placement)", "(new)", "(init)", NULL);
}

static void
print_VEC_NEW_EXPR (FILE *file,
		    const char *annotation ATTRIBUTE_UNUSED,
		    tree node,
		    int indent)
{
  print_operands (file, node, indent, TRUE, "(placement)", "(new)", "(init)", NULL);
}

static void
print_DELETE_EXPR (FILE *file,
		   const char *annotation ATTRIBUTE_UNUSED,
		   tree node,
		   int indent)
{
  if (DELETE_EXPR_USE_GLOBAL (node))
    fputs ("use-glbl", file);
  if (DELETE_EXPR_USE_VEC (node))
    fputs ("use-vec", file);
    
  print_operands (file, node, indent, TRUE, "(store)", "(how)", NULL);
}

static void
print_VEC_DELETE_EXPR (FILE *file,
		       const char *annotation ATTRIBUTE_UNUSED,
		       tree node,
		       int indent)
{
  print_operands (file, node, indent, TRUE, "(store)", "(how)", NULL);
}

static void
print_SCOPE_REF (FILE *file,
		 const char *annotation ATTRIBUTE_UNUSED,
		 tree node,
		 int indent)
{
  print_ref (file, annotation, node, indent);
  fprintf (file, " complexity=%d", TREE_COMPLEXITY (node));
  
  print_operands (file, node, indent, TRUE, "(class)", "(field)", NULL);
}

static void
print_MEMBER_REF (FILE *file,
		  const char *annotation ATTRIBUTE_UNUSED,
		  tree node,
		  int indent)
{
  print_ref (file, annotation, node, indent);
  
  print_operands (file, node, indent, FALSE, "(obj)", "(mbr)", NULL);
  /* not sure I want to follow these nodes here */
}

static void
print_TYPE_EXPR (FILE *file,
		 const char *annotation ATTRIBUTE_UNUSED,
		 tree node,
		 int indent)
{
  fprintf (file, " type=");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (TREE_TYPE (node)));
  
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_AGGR_INIT_EXPR (FILE *file,
		      const char *annotation ATTRIBUTE_UNUSED,
		      tree node,
		      int indent)
{
  if (AGGR_INIT_VIA_CTOR_P(node))
    fputs (" ctor", file);
    
  print_operands (file, node, indent, TRUE, "(init-funct)", "(args)", "(slot)", NULL);
}

static void
print_THROW_EXPR (FILE *file,
		  const char *annotation ATTRIBUTE_UNUSED,
		  tree node,
		  int indent)
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_EMPTY_CLASS_EXPR (FILE *file, 
                        const char *annotation ATTRIBUTE_UNUSED, 
			tree node, 
			int indent ATTRIBUTE_UNUSED)
{
  if (TREE_TYPE (node))
    {
      fprintf (file, "class=");
      fprintf (file, HOST_PTR_PRINTF,
    		HOST_PTR_PRINTF_VALUE (TREE_TYPE (node)));
    }
}

static void
print_BASELINK (FILE *file,
		const char *annotation ATTRIBUTE_UNUSED,
		tree node,
		int indent)
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_TEMPLATE_DECL (FILE *file,
		     const char *annotation ATTRIBUTE_UNUSED,
		     tree node,
		     int indent)
{
  dump_tree_state.line_cnt = 0;
  
  fprintf (file, " args=");
  fprintf (file, HOST_PTR_PRINTF,
		HOST_PTR_PRINTF_VALUE (DECL_ARGUMENTS (node)));
  if (DECL_LANG_SPECIFIC (node) && DECL_TEMPLATE_INFO (node))
    {
      fprintf (file, " tmpl-info=");
      fprintf (file, HOST_PTR_PRINTF,
		HOST_PTR_PRINTF_VALUE (DECL_TEMPLATE_INFO (node)));
    }
  if (DECL_VINDEX (node))
    {
      fprintf (file, " inst=");
      fprintf (file, HOST_PTR_PRINTF,
		HOST_PTR_PRINTF_VALUE (DECL_VINDEX (node)));
    }
  if (TREE_TYPE (node))
    {
      fprintf (file, " obj-type=");
      fprintf (file, HOST_PTR_PRINTF,
		HOST_PTR_PRINTF_VALUE (TREE_TYPE (node)));
    }
  if (DECL_TEMPLATE_RESULT (node))
    {
      fprintf (file, " obj-decl=");
      fprintf (file, HOST_PTR_PRINTF,
		HOST_PTR_PRINTF_VALUE (DECL_TEMPLATE_RESULT (node)));
    }
  if (DECL_INITIAL (node))
    {
      fprintf (file, " assoc-tmpls=");
      fprintf (file, HOST_PTR_PRINTF,
		HOST_PTR_PRINTF_VALUE (DECL_INITIAL (node)));
    }		
  print_decl (file, annotation, node, indent);
  (void)node_seen (node, TRUE);
  
  if (DECL_ARGUMENTS (node))
    {
      if (dump_tree_state.line_cnt > 1)
      	newline_and_indent (file, 0);
      dump_tree (file, "(args)", DECL_ARGUMENTS (node), indent + INDENT);
    }
  
  if (DECL_VINDEX (node))
    {
      if (dump_tree_state.line_cnt > 1)
      	newline_and_indent (file, 0);
      dump_tree (file, "(inst)", DECL_VINDEX (node), indent + INDENT);
    }
    
  /* tsubst_decl() in cp/pt.c looks interesting */
  if (TREE_TYPE (node))
    {
      if (dump_tree_state.line_cnt > 1)
      	newline_and_indent (file, 0);
      dump_tree (file, "(obj-type)", TREE_TYPE (node), indent + INDENT);
    }
    
  if (DECL_TEMPLATE_RESULT (node))
    {
      if (dump_tree_state.line_cnt > 1)
      	newline_and_indent (file, 0);
      dump_tree (file, "(obj-decl)", DECL_TEMPLATE_RESULT (node), indent + INDENT);
    }
  
  if (DECL_INITIAL (node))
    {
      if (dump_tree_state.line_cnt > 1)
      	newline_and_indent (file, 0);
      dump_tree (file, "(assoc-tmpl)", DECL_INITIAL (node), indent + INDENT);
    }
}

static void
print_TEMPLATE_PARM_INDEX (FILE *file,
			   const char *annotation ATTRIBUTE_UNUSED,
			   tree node,
			   int indent)
{
  fprintf (file, " idx/lvl=("HOST_WIDE_INT_PRINT_DEC","HOST_WIDE_INT_PRINT_DEC")"
  		 " orig-lvl="HOST_WIDE_INT_PRINT_DEC
		 " dcndnts=",
		 TEMPLATE_PARM_IDX (node), TEMPLATE_PARM_LEVEL(node),
		 TEMPLATE_PARM_ORIG_LEVEL (node));
  fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE (TEMPLATE_PARM_DESCENDANTS (node)));
		 

  print_decl (file, annotation, TEMPLATE_PARM_DECL(node), indent + INDENT);
  dump_tree (file, "(dcndnt)", TEMPLATE_PARM_DESCENDANTS (node), indent + INDENT);
}

static void
print_TEMPLATE_TYPE_PARM (FILE *file,
			  const char *annotation,
			  tree node,
			  int indent)
{
  fprintf (file, " parms=");
  fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE (TEMPLATE_TYPE_IDX (node)));
  fprintf (file, " idx/lvl=("HOST_WIDE_INT_PRINT_DEC","HOST_WIDE_INT_PRINT_DEC")"
  		 " orig-lvl="HOST_WIDE_INT_PRINT_DEC,
        	 TEMPLATE_TYPE_IDX (node), TEMPLATE_TYPE_LEVEL (node),
        	 TEMPLATE_TYPE_ORIG_LEVEL (node));
  print_type (file, annotation, node, indent);
  
  dump_tree (file, "(parm)", TEMPLATE_TYPE_PARM_INDEX (node), indent + INDENT);
}

static void
print_TEMPLATE_TEMPLATE_PARM (FILE *file,
			      const char *annotation,
			      tree node,
			      int indent)
{
  fprintf (file, " tmpl-decl=");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (TYPE_NAME (node)));
  
  print_TEMPLATE_TYPE_PARM (file, annotation, node, indent);
  
  dump_tree (file, "(tmpl-decl)", TYPE_NAME (node), indent + INDENT);
}

static void
print_BOUND_TEMPLATE_TEMPLATE_PARM (FILE *file,
				    const char *annotation,
				    tree node,
				    int indent)
{
  fprintf (file, " name=");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (TEMPLATE_TEMPLATE_PARM_TEMPLATE_INFO (node)));
  fprintf (file, " type-decl=");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (TYPE_NAME (node)));
  fprintf (file, " tmpl-decl=");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (TYPE_TI_TEMPLATE (node)));
  		  
  print_TEMPLATE_TYPE_PARM (file, annotation, node, indent);
  
  dump_tree (file, "(name)", TEMPLATE_TEMPLATE_PARM_TEMPLATE_INFO (node), indent + INDENT);
  dump_tree (file, "(type-decl)", TYPE_NAME (node), indent + INDENT);
  dump_tree (file, "(tmpl-decl)", TYPE_TI_TEMPLATE (node), indent + INDENT);
}

static void
print_TYPENAME_TYPE (FILE *file,
		     const char *annotation,
		     tree node,
		     int indent)
{
  fprintf (file, " cntxt::id=");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (TYPE_CONTEXT (node)));
  fprintf (file, "::");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (TYPE_NAME (node)));
		
  if (TYPENAME_TYPE_FULLNAME (node))
    {
      fprintf (file, " fullname=");
      fprintf (file, HOST_PTR_PRINTF,
    		HOST_PTR_PRINTF_VALUE (TYPENAME_TYPE_FULLNAME (node)));
    }
  if (TREE_TYPE (node))
    {
      fprintf (file, " impl-type=");
      fprintf (file, HOST_PTR_PRINTF,
    		HOST_PTR_PRINTF_VALUE (TREE_TYPE (node)));
    }
  print_type (file, annotation, node, indent);
  
  dump_tree (file, "(cntxt)", TYPE_CONTEXT (node), indent + INDENT);
  dump_tree (file, "(id)", TYPE_NAME (node), indent + INDENT);
  dump_tree (file, "(fullname)", TYPENAME_TYPE_FULLNAME (node), indent + INDENT);
  dump_tree (file, "(impl-type)", TREE_TYPE (node), indent + INDENT);
}

static void
print_UNBOUND_CLASS_TEMPLATE (FILE *file,
			      const char *annotation,
			      tree node,
			      int indent)
{
  fprintf (file, " cntxt::id=");
  fprintf (file, HOST_PTR_PRINTF,
	   HOST_PTR_PRINTF_VALUE (TYPE_CONTEXT (node)));
  fprintf (file, "::");
  fprintf (file, HOST_PTR_PRINTF,
	   HOST_PTR_PRINTF_VALUE (TYPE_NAME (node)));
  print_type (file, annotation, node, indent); 
  
  dump_tree (file, "(cntxt)", TYPE_CONTEXT (node), indent + INDENT);
  dump_tree (file, "(id)", TYPE_NAME (node), indent + INDENT);
}

static void
print_TYPEOF_TYPE (FILE *file,
		   const char *annotation,
		   tree node,
		   int indent)
{
  fprintf (file, " ");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (TYPE_FIELDS (node)));
  print_type (file, annotation, node, indent); 
  
  dump_tree (file, NULL, TYPE_FIELDS (node), indent + INDENT);
}

static void
print_USING_DECL (FILE *file,
		  const char *annotation,
		  tree node,
		  int indent)
{
  fprintf (file, " scope=");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (DECL_INITIAL (node)));
  print_decl (file, annotation, node, indent);

  dump_tree (file, "(scope)", DECL_INITIAL (node), indent + INDENT);
}

static void
print_USING_STMT (FILE *file,
		  const char *annotation ATTRIBUTE_UNUSED,
		  tree node,
		  int indent)
{
  print_operands (file, node, indent, TRUE, "(using)", NULL);
}

static void
print_DEFAULT_ARG (FILE *file, 
                   const char *annotation ATTRIBUTE_UNUSED, 
		   tree node, 
		   int indent ATTRIBUTE_UNUSED)
{
#if 0
  /* TO DO */
  fprintf (file, " def-arg=");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (DEFARG_POINTER (node)));
#endif
  fprintf (file, " (struct unparsed_text * in cp/spew.c)");

  if (TREE_PURPOSE (node))
    {
      fprintf (file, " purpose=");
      fprintf (file, HOST_PTR_PRINTF,
      	        HOST_PTR_PRINTF_VALUE (TREE_PURPOSE (node)));
      dump_tree (file, "(purpose)", TREE_PURPOSE (node), indent + INDENT);
    }
}

static void
print_TEMPLATE_ID_EXPR (FILE *file,
			const char *annotation ATTRIBUTE_UNUSED,
			tree node,
			int indent)
{
  print_operands (file, node, indent, TRUE, "(tmpl)", "(args)", NULL);
}

#if 0

static void
print_CPLUS_BINDING (FILE *file, 
                     const char *annotation ATTRIBUTE_UNUSED, 
		     tree node, 
		     int indent)
{
  tree n;
  
  #define BINDING_LEVEL(NODE) \
     (((struct tree_binding*)NODE)->scope.level)
  
  if (LOCAL_BINDING_P (node))
    fputs (" local", file);
  if (INHERITED_VALUE_BINDING_P (node))
    fputs (" inherited", file);
  
  fprintf (file, " value=");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (BINDING_VALUE (node)));
    
  if (BINDING_HAS_LEVEL_P (node))
    {
      fprintf (file, " level=");
      fprintf (file, HOST_PTR_PRINTF,
      			HOST_PTR_PRINTF_VALUE ( (node)));
      fprintf (file, " (struct binding_level * in cp/decl.c)");
    }
  else
    {
      fprintf (file, " scope=");
      fprintf (file, HOST_PTR_PRINTF,
      			HOST_PTR_PRINTF_VALUE (BINDING_LEVEL (node)));
    }
  
  if (TREE_CHAIN (node))
    {
      fprintf (file, " chain=");
      fprintf (file, HOST_PTR_PRINTF,
		HOST_PTR_PRINTF_VALUE (TREE_CHAIN (node)));
    }
  
  dump_tree (file, "(value)", BINDING_VALUE (node), indent + INDENT);
  if (!BINDING_HAS_LEVEL_P (node))
    dump_tree (file, "(scope)", BINDING_SCOPE (node), indent + INDENT);
  
  for (n = TREE_CHAIN (node); n; n = TREE_CHAIN (n))
    dump_tree (file, "(chain)", n, indent + INDENT);
}

#endif

static void
print_OVERLOAD (FILE *file,
	        const char *annotation ATTRIBUTE_UNUSED,
	        tree node,
	        int indent)
{
  tree n;
  
  if (OVL_FUNCTION (node))
    {
      fprintf (file, " ovld=");
      fprintf (file, HOST_PTR_PRINTF,
    	HOST_PTR_PRINTF_VALUE (OVL_FUNCTION (node)));
    }
  if (OVL_CHAIN (node))
    {
      fprintf (file, " next-ovld=");
      fprintf (file, HOST_PTR_PRINTF,
    	HOST_PTR_PRINTF_VALUE (OVL_CHAIN (node)));
    }
  
  if ((TREE_CODE (OVL_FUNCTION (node)) == FUNCTION_DECL
       || TREE_CODE (OVL_FUNCTION (node)) == TEMPLATE_DECL)
      && DECL_NAME (OVL_FUNCTION (node)))
    fprintf (file, " %s",
    	IDENTIFIER_POINTER (DECL_NAME (OVL_FUNCTION (node))));
  
  if (DECL_CONSTRUCTOR_P (OVL_FUNCTION (node)))
    dump_tree (file, NULL, OVL_FUNCTION (node), indent + INDENT);
  else
    dump_tree (file, "(ovld)", OVL_FUNCTION (node), indent + INDENT);

  for (n = OVL_CHAIN (node); n; n = OVL_CHAIN (n))
    dump_tree (file, NULL, n, indent + INDENT);
}

static void
print_WRAPPER (FILE *file,
               const char *annotation ATTRIBUTE_UNUSED,
	       tree node,
	       int indent ATTRIBUTE_UNUSED)
{
  /* TODO: Print out tree_common.  */
  fprintf (file, " ptr=");
  fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE (WRAPPER_ZC (node)));
}

static void
print_MODOP_EXPR (FILE *file, 
                  const char *annotation ATTRIBUTE_UNUSED, 
		  tree node, 
		  int indent)
{
  print_operands (file, node, indent, TRUE, "(lhs)", "(modifycode)", "(rhs)", NULL);
}

static void
print_CAST_EXPR (FILE *file, 
                 const char *annotation ATTRIBUTE_UNUSED, 
		 tree node, 
		 int indent)
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_REINTERPRET_CAST_EXPR (FILE *file, 
                             const char *annotation ATTRIBUTE_UNUSED, 
			     tree node, 
			     int indent)
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_CONST_CAST_EXPR (FILE *file, 
                       const char *annotation ATTRIBUTE_UNUSED, 
		       tree node, 
		       int indent)
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_STATIC_CAST_EXPR (FILE *file, 
                        const char *annotation ATTRIBUTE_UNUSED, 
			tree node, 
			int indent)
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_DYNAMIC_CAST_EXPR (FILE *file, 
                         const char *annotation ATTRIBUTE_UNUSED, 
			 tree node ATTRIBUTE_UNUSED, 
			 int indent ATTRIBUTE_UNUSED)
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_DOTSTAR_EXPR (FILE *file, 
                    const char *annotation ATTRIBUTE_UNUSED, 
		    tree node, 
		    int indent)
{
  print_operands (file, node, indent, TRUE, "(datum)", "(cmpnt)", NULL);
}

static void
print_TYPEID_EXPR (FILE *file, 
                   const char *annotation ATTRIBUTE_UNUSED, 
		   tree node, 
		   int indent)
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_PSEUDO_DTOR_EXPR (FILE *file, 
                        const char *annotation ATTRIBUTE_UNUSED, 
			tree node, 
			int indent)
{
  print_operands (file, node, indent, TRUE, "(obj)", "(scope)", "(dtor)", NULL);
}

static void
print_CTOR_INITIALIZER (FILE *file,
			const char *annotation ATTRIBUTE_UNUSED,
			tree node,
			int indent)
{
  print_operands (file, node, indent, TRUE, "(mbr-init)", "(base-init)", NULL);
}

#if 0
static void
print_RETURN_INIT (FILE *file,
		   const char *annotation ATTRIBUTE_UNUSED,
		   tree node,
		   int indent)
{
  print_operands (file, node, indent, TRUE, "(id)", "(init)", NULL);
}
#endif

static void
print_TRY_BLOCK (FILE *file,
		 const char *annotation ATTRIBUTE_UNUSED,
		 tree node,
		 int indent)
{
  if (FN_TRY_BLOCK_P (node))
    fputs (" func-try-blk", file);
  if (CLEANUP_P (node))
    fputs (" clnup", file);
    
  print_operands (file, node, indent, TRUE, "(body)", "(hndlrs)", NULL);
}

static void
print_EH_SPEC_BLOCK (FILE *file,
		     const char *annotation ATTRIBUTE_UNUSED,
		     tree node,
		     int indent)
{
  print_operands (file, node, indent, TRUE, "(body)", "(raises)", NULL);
}

static void
print_HANDLER (FILE *file,
	       const char *annotation ATTRIBUTE_UNUSED,
	       tree node,
	       int indent)
{
  fprintf (file, " hdnlr-type=");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (HANDLER_TYPE (node)));
  		
  print_operands (file, node, indent, TRUE, "(parms)", "(body)", NULL);
}

static void
print_MUST_NOT_THROW_EXPR (FILE *file,
			   const char *annotation ATTRIBUTE_UNUSED,
			   tree node,
			   int indent)
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_TAG_DEFN (FILE *file ATTRIBUTE_UNUSED, 
                const char *annotation ATTRIBUTE_UNUSED, 
		tree node ATTRIBUTE_UNUSED, 
		int indent ATTRIBUTE_UNUSED)
{
}

static void
print_IDENTITY_CONV (FILE *file,
		     const char *annotation ATTRIBUTE_UNUSED,
		     tree node,
		     int indent)
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_LVALUE_CONV (FILE *file,
		   const char *annotation ATTRIBUTE_UNUSED,
		   tree node,
		   int indent)
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_QUAL_CONV (FILE *file,
		 const char *annotation ATTRIBUTE_UNUSED,
		 tree node,
		 int indent)
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_STD_CONV (FILE *file,
		const char *annotation ATTRIBUTE_UNUSED,
		tree node,
		int indent)
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_PTR_CONV (FILE *file,
		const char *annotation ATTRIBUTE_UNUSED,
		tree node,
		int indent)
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_PMEM_CONV (FILE *file,
		 const char *annotation ATTRIBUTE_UNUSED,
		 tree node,
		 int indent)
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_BASE_CONV (FILE *file,
		 const char *annotation ATTRIBUTE_UNUSED,
		 tree node,
		 int indent)
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_REF_BIND (FILE *file,
		const char *annotation ATTRIBUTE_UNUSED,
		tree node,
		int indent)
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_USER_CONV (FILE *file,
		 const char *annotation ATTRIBUTE_UNUSED,
		 tree node,
		 int indent)
{
  fprintf (file, " from=");
  fprintf (file, HOST_PTR_PRINTF,
  	HOST_PTR_PRINTF_VALUE (TREE_OPERAND (node, 0)));
  fprintf (file, " cand=");
  fprintf (file, HOST_PTR_PRINTF,
  	HOST_PTR_PRINTF_VALUE (TREE_OPERAND (node, 1)));
  
  print_operands (file, node, indent, TRUE, "(from)", "(cand)", NULL);
}

static void
print_AMBIG_CONV (FILE *file,
		  const char *annotation ATTRIBUTE_UNUSED,
		  tree node,
		  int indent)
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_RVALUE_CONV (FILE *file,
		   const char *annotation ATTRIBUTE_UNUSED,
		   tree node,
		   int indent)
{
  print_operands (file, node, indent, TRUE, NULL);
}

/*-------------------------------------------------------------------*/

/* Override to routine in dmp-tree.c print Method vector Record Type.  */
static void
print_RECORD_TYPE (FILE *file,
		   const char *annotation,
		   tree node,
		   int indent)
{
  tree n;
  
  fprintf (file, " fields=");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (TYPE_FIELDS (node)));
  fprintf (file, " mbrs=");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (CLASSTYPE_METHOD_VEC (node)));
  if (TYPE_NO_FORCE_BLK (node))
    fputs (" no-force-blk", file);
  fprintf (file, " #parents=%d", CLASSTYPE_N_BASECLASSES (node));
  if (CLASSTYPE_USE_TEMPLATE (node))
    fprintf (file, " use-tmpl=%d", CLASSTYPE_USE_TEMPLATE (node));
  if (TYPE_PTRMEMFUNC_P (node))
    {
      fprintf (file, " ptrmemfunc-fn-type=");
      fprintf (file, HOST_PTR_PRINTF,
	HOST_PTR_PRINTF_VALUE (TYPE_PTRMEMFUNC_FN_TYPE (node)));
    }
  print_type (file, annotation, node, indent);
  (void)node_seen (node, TRUE);
  
  for (n = TYPE_FIELDS (node); n; n = TREE_CHAIN (n))
    {
      if (TREE_CODE (n) == TYPE_DECL
          && TREE_TYPE (n) == DECL_CONTEXT (n)
          && TREE_TYPE (n) == node)
        dump_tree (file, "(self-reference)", n, indent + INDENT);
      else
        dump_tree (file, NULL, n, indent + INDENT);
    }

  dump_tree (file, "(mbrs)", CLASSTYPE_METHOD_VEC (node), indent + INDENT);   
  
  if (TYPE_PTRMEMFUNC_P (node))
    {
      newline_and_indent (file, 0);
      dump_tree (file, "(ptrmemfunc-fn-type)",
		  TYPE_PTRMEMFUNC_FN_TYPE (node), indent + INDENT);
    }
}

/* Override to routine in dmp-tree.c to print namespace.  */
static void
print_NAMESPACE_DECL (FILE *file,
		      const char *annotation,
		      tree node,
		      int indent)
{
  if (NAMESPACE_LEVEL (node))
    {
      fprintf (file, " binding_lvl=");
      fprintf (file, HOST_PTR_PRINTF,
    		HOST_PTR_PRINTF_VALUE (NAMESPACE_LEVEL (node)));
    }
  if (DECL_NAMESPACE_ALIAS (node))
    {
      fprintf (file, " alias=");
      fprintf (file, HOST_PTR_PRINTF,
    		HOST_PTR_PRINTF_VALUE (DECL_NAMESPACE_ALIAS (node)));
    }
  if (DECL_NAMESPACE_USING (node))
    {
      fprintf (file, " using=");
      fprintf (file, HOST_PTR_PRINTF,
    		HOST_PTR_PRINTF_VALUE (DECL_NAMESPACE_USING (node)));
    }
  if (DECL_NAMESPACE_USERS (node))
    {
      fprintf (file, " usrs=");
      fprintf (file, HOST_PTR_PRINTF,
    		HOST_PTR_PRINTF_VALUE (DECL_NAMESPACE_USERS (node)));
    }

  print_decl (file, annotation, node, indent);
    
  dump_tree (file, "(alias)", DECL_NAMESPACE_ALIAS (node), indent + INDENT);
  dump_tree (file, "(using)", DECL_NAMESPACE_USING (node), indent + INDENT);
  dump_tree (file, "(usrs)",  DECL_NAMESPACE_USERS (node), indent + INDENT);
  
  if (dump_tree_state.visit_only_once == DMP_TREE_VISIT_ONCE2)
    {
      for (node = cp_namespace_decls (node); node; node = TREE_CHAIN (node))
	dump_tree (file, NULL, node, indent + INDENT);
    }
}

static void
print_ADDR_EXPR (FILE *file,
	         const char *annotation ATTRIBUTE_UNUSED,
	         tree node,
	         int indent)
{
  if (PTRMEM_OK_P (node))
    fputs (" ptr-to-mbr-ok", file);
    
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_ALIAS_DECL (FILE *file ATTRIBUTE_UNUSED,
	         const char *annotation ATTRIBUTE_UNUSED,
	         tree node ATTRIBUTE_UNUSED,
	         int indent ATTRIBUTE_UNUSED)
{
  /* TO DO */
}

/*-------------------------------------------------------------------*/

/* Return 1 if tree node is a C++ specific tree node from cp-tree.def
   or a tree node specific to whatever cp_prev_lang_dump_tree_p
   calls.  Otherwise return 0.
*/

int
cp_dump_tree_p (FILE *file, const char *annotation, tree node, int indent)
{
   switch (TREE_CODE (node)) 
     {
     #define DEFTREECODE(SYM, NAME, TYPE, LENGTH) \
     case SYM: print_ ## SYM (file, annotation, node, indent); break;
     #include "cp-tree.def"
     #undef DEFTREECODE
     
     case RECORD_TYPE:
       print_RECORD_TYPE (file, annotation, node, indent);
       break;
    
     case NAMESPACE_DECL:
       print_NAMESPACE_DECL (file, annotation, node, indent);
       break;
     
     case ADDR_EXPR:
       print_ADDR_EXPR (file, annotation, node, indent);
       break;
     
     default:
       return cp_prev_lang_dump_tree_p (file, annotation, node, indent);
       break;
     }
   
   return 1;
}

/*-------------------------------------------------------------------*/

#if 0

cd $gcc3/gcc; \
cc -no-cpp-precomp -c -DIN_GCC  -g \
  -W -Wall -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes  \
  -DHAVE_CONFIG_H \
  -I$gcc3obj \
  -Icp \
  -I. \
  -Iconfig \
  -I../include \
  cp/cp-dmp-tree.c -o ~/tmp.o -w 

#endif
