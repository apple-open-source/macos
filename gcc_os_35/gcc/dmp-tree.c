/* APPLE LOCAL new tree dump */
/* Common condensed tree display routines.
   Copyright (C) 2001, 2002  Free Software Foundation, Inc.
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

/* This dumper is intended for used with the debugger.  You call
   dump_tree(x), where x is the node of interest.  The tree display
   produced here is much more condensed than what is produced by,
   say, debug_tree().  Here each node is displayed as a single
   (possibly line wrapped) line.  The emphasis here is mainly on
   the parse (front-end) trees as opposed to RTL-related info.
 
   Here's an example of some of it's output:
   
   compound_stmt:0xED4D0C line=74 0xED4D20
    scope_stmt:0xED4D20 line=75 BEGIN block=0xED0300
     block:0xED0300 vars=0xECD620 abs-orig=0x0 super=0xED0340 sub=0x0
      var_decl:0xECD620 t=0xEA5310 {int} initial=0xED0260 line=75 comm
                        cntxt=0xECD3F0 sz=32(4) zzz
       integer_cst:0xED0260 t=0xEA5310 {int} 0xFFFFFF9C = -100

     decl_stmt:0xED4D84 line=75 var_decl=0xECD620 {int} zzz

     expr_stmt:0xED4E24 line=76 0xED02E0
      modify_expr:0xED02E0 t=0xEA5310 {int} 0xECD540 0xED4DFC
       var_decl:0xECD540 t=0xEA5310 {int} cntxt=0xECD3F0 i
       nop_expr:0xED4DFC t=0xEA5310 {int} 0xED02C0
        plus_expr:0xED02C0 t=0xEA8540 {long unsigned int} 0xED4DE8 0xED02A0
         nop_expr:0xED4DE8 t=0xEA8540 {long unsigned int} 0xED0280
          trunc_div_expr:0xED0280 t=0xEA5310 {int} 0xECD540 0xECD620
           var_decl:0xECD540 t=0xEA5310 {int} cntxt=0xECD3F0 i
           var_decl:0xECD620 t=0xEA5310 {int} cntxt=0xECD3F0 zzz
         call_expr:0xED02A0 t=0xEA8540 {long unsigned int} func=0xED4DAC args=0xED4DD4
          addr_expr:0xED4DAC (func) t=0xECD690 {pointer_type} 0xEB0850
           function_decl:0xEB0850 t=0xEABD90 {long unsigned int} BUILT_IN_NORMAL strlen
          tree_list:0xED4DD4 (args) purpose=0x0 value=0xED4DC0 chain=0x0
           nop_expr:0xED4DC0 (value) t=0xEAB1C0 {pointer_type} 0xECD4D0
            var_decl:0xECD4D0 t=0xEA8CB0 {pointer_type} cntxt=0xECD3F0 lcl
    scope_stmt:0xED4E38 line=77 END block=0xED0300
  
  Each node is limited to a single line (unless it needs to be wrapped).
  All nodes at the same nesting level are displayed at the same indenting
  level.  Each node is handled individually with code appropriate to what
  it needs to display.
  
  Note that unlike debug_tree() the details of the type info are not shown
  in these trees when (as in this example) the initial node passed to
  dmp_tree() is not a type.  It is assumed that you are mainly interested
  in the parse trees and their relationship to one another and don't need
  the display cluttered up with each node's type info (other than it's
  kind and address).
  
  On the other hand you could be interested in the details of a particular
  type.  In that case call dmp_tree() specifying that type's address as
  the initial node.  When that is done a similarly formatted tree is
  output, but now the details of that type and the types it uses are
  shown.  In the above example, 3 lines up from the bottom, there's a
  pointer (which happens to be for a char*).  Here's what
  dmp_tree(0xEAB1C0) produces:
  
  pointer_type:0xEAB1C0 t=0xEAB150 align=32 prec=32 sz=32(4)
   integer_type:0xEAB150 uns const align=8 *this=0xEAB1C0 prec=8 sz=8(1)
                         min/max=0/255 char
 
  Note, the type is fully followed (except for *this which is overkill).
  Also illustrated is how lines are wrapped if too long for the display.
  Here, it's illustrated assuming about a 72-character wrap point to 
  fit in these comments.  In reality, the wrap point is normally the
  terminal width.  But the DMP_TREE_WRAP environment variable can be
  set to the desired width.  If neither the terminal width with nor the
  environment variable can be accessed, or the value is outside the
  range 72 to 512, the width is set to 130.
  
  There are five language-specific routines that which MUST be uniquely
  defined (via the language hooks) for that language to handle language-
  specific attributes for certain kinds of tree nodes:
     
  1. void dump_identifier(FILE *file, tree node, int indent, int before_id);
  
     Called twice for dmp_tree() for an IDENTIFIER_NODE.  The first
     call is after the common info for the node is generated but before
     displaying the identifier (before_id==0) which is always assumed to
     be the last thing on the line.
     
     The second call is done after the id is displayed (before_id!=0).
     This is for displaying any language-specific node information that
     should be preceded by an newline_and_indent() call or a recursive
     call to dump_tree() for nodes which are language specific operands
     to a IDENTIFIER_NODE.
     
  2. void dump_decl(FILE *file, tree node, int indent, int before_id);
     
     Called twice for dmp_tree() for a ..._DECL node.  The first call
     after the common info for the node is generated but before
     displaying the identifier (before_id==0) which is always assumed
     to be the last thing on the line.
     
     The second call is done after the id is displayed (before_id!=0).
     This is for displaying any language-specific node information that
     should be preceded by an newline_and_indent() call or a recursive
     call to dump_tree() for nodes which are language specific operands
     to a ..._DECL node.
    
  3. void dump_type(FILE *file, tree node, int indent, int before_id);
     
     Called twice for dmp_tree() for a ..._TYPE node.  The first call
     after the common info for the node is generated but before
     displaying the identifier (before_id==0) which is always assumed
     to be the last thing on the line.
     
     The second call is done after the id is displayed (before_id!=0).
     This is for displaying any language-specific node information that
     should be preceded by an newline_and_indent() call or a recursive
     call to dump_tree() for nodes which are language specific operands
     to a ..._TYPE node.
  
  4. int dump_blank_line_p(tree previous_node, tree current_node);
   
     Normally a blank line is inserted before each statement node (a 
     statement node is determined by calling statement_code_p()).  This
     makes the display easier to read by keeping each statement grouped
     like a paragraph.  There may, however, be some kinds of statements
     where a blank line isn't desired (e.g., a begin SCOPE_STMT in C).
     Thus dump_blank_line_p() is called to ask if a particular 
     statement should be preceded by a blank line dependent upon the
     node that preceded it.
     
     dump_blank_line_p() is called for each statement passing the
     previous node (not necessarily a statement) and current node (a
     statement node by definition).  It should return 1 if a blank
     line is to be inserted and 0 otherwise.
  
  5. int dump_lineno_p(FILE *file, tree node);
  
     This is called for each node to display file and/or line number
     information for those nodes that have such information.  If it
     is displayed the function should return 1.  If not, 0.
     
     The function generally does not have to handle ..._DECL nodes
     unless there some special handling is reequired.  They are
     handled by print_lineno() (dump_lineno_p()'s caller).
     It is defined to not repeat the filename if it does not
     change from what's in dump_tree_state.curr_file and then
     it only displays the basename (using lbasename()).  The
     format of the display is " line=nbr(basename)" where the
     leading space is included as usual in these displays and
     the parenthesized basename omitted if not needed or is
     the same as before.
*/
  
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tree.h"
#include "real.h"
#include "c-common.h"
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include "langhooks.h"

/* c-common.h defines a macro called RETURN_EXPR.  But we need to use it
   as the enum defined by tree.def.  */
#undef RETURN_EXPR

#define DMP_TREE
#include "dmp-tree.h"

#if defined(HAVE_UNISTD_H) && DMP_TREE_WRAPPED_OUTPUT
#include <sys/ioctl.h>
#endif

#define MIN_LINE_WRAP 	    72		   /* min wrapping column  */
#define MAX_LINE_WRAP 	   512		   /* max wrapping column  */
#define DEFAULT_LINE_WRAP  130		   /* default              */
#define DMP_TREE_LINE_WRAP "DMP_TREE_WRAP" /* environment variable */

/* Notes to myself:

   c-common.c, verify_tree(), c-decl.c, and cp/ptree.c look intersting!
*/

void dump_tree (FILE *file, const char *annotation, tree node, int indent);
void dmp_tree (tree node);

#define HASH_SIZE 37

struct bucket
{
  tree node;
  struct bucket *next;
};

static struct bucket **table;

static tree *type_array = NULL;
#define TYPE_ARRAY_INITIAL_SIZE 20000
#define TYPE_ARRAY_INCR         10000
static int type_array_size = 0;
static int type_array_next = 0;
static int type_array_incr = TYPE_ARRAY_INITIAL_SIZE;

#if DMP_TREE_WRAPPED_OUTPUT
static char curr_line[2 * MAX_LINE_WRAP];
static int prefix_len;
static int line_index;
static int prev_line_was_null;
static int prev_line_was_wrapped;
static int wrap_column;
#ifdef TIOCGWINSZ
static struct winsize winsize;
#endif
#if defined(TIOCGSIZE) && !defined(TIOCGWINSZ)
static struct ttysize winsize;
#endif
#endif /* DMP_TREE_WRAPPED_OUTPUT */

dump_tree_state_t dump_tree_state = {
  (int)LAST_AND_UNUSED_TREE_CODE,	/* c */
  -1,					/* nesting_depth */
  0, 					/* dump_full_type */
  0, 					/* really_follow */
  0, 					/* doing_parm_decl */
  0, 					/* doing_call_expr */
  NULL, 				/* curr_file */
  0, 					/* no_new_line */
  0, 					/* line_cnt */
  0,					/* doing_tree_list */
  INT_MAX,				/* max_depth */
  DMP_TREE_VISIT_ANY			/* visit_only_once */
};

#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) \
static void print_ ## SYM (FILE *file, const char *annotation, tree node, int indent);
#include "tree.def"
#undef DEFTREECODE

static void init_dump_state PARAMS ((tree node));
static int no_dump_tree_p   PARAMS ((FILE *file, const char *annotation, tree node, int indent));
static void free_hash_table PARAMS ((void));

static lang_dump_tree_p_t lang_dump_tree_p = no_dump_tree_p;

#define CST_VALUE(node, ok)						\
  ((TREE_INT_CST_HIGH (node) == 0) 					\
    ? (ok &= 1, TREE_INT_CST_LOW (node)) 				\
    : ((TREE_INT_CST_HIGH (node) == -1 && TREE_INT_CST_LOW (node) != 0) \
       ? (ok &= 1, -TREE_INT_CST_LOW (node))				\
       : (ok = 0)))



/*-------------------------------------------------------------------*/

static void
init_dump_state (node)
     tree node;
{
  dump_tree_state.dump_full_type  = TYPE_P (node);
  /*dump_tree_state.nesting_depth = 0; */
  dump_tree_state.really_follow   = 1;
  dump_tree_state.doing_parm_decl = 0;
  dump_tree_state.doing_call_expr = 0;
  dump_tree_state.no_new_line	  = 0;
  dump_tree_state.line_cnt	  = 0;
  dump_tree_state.curr_file	  = NULL;
  dump_tree_state.doing_tree_list = 0;
  /*dump_tree_state.max_depth	  = INT_MAX;*/
  /*dump_tree_state.visit_only_once = DMP_TREE_VISIT_ANY;*/
}

/* Always end the current line by writing a '\n'.
   If column > 0 then output column blanks to the next line.
   Increment dump_tree_state.line_cnt to allow callers to
   determine how many lines are output between any pair of
   points.  */
   
void 
newline_and_indent (file, column)
     FILE *file;
     int column;
{
  fputc ('\n', file);
  ++dump_tree_state.line_cnt;
  
  if (column > 0)
    fprintf (file, "%*c", column, ' ');
}

/* Return 1 if node has been previously seen and 0 otherwise.
   If record_it is non-zero then record that the node was
   seen.  */
int 
node_seen (node, record_it)
     tree node;
     int record_it;
{
  int hash;
  struct bucket *b;
  
  hash = ((unsigned long) node) % HASH_SIZE;

  /* If node is in the table, just mention its address.  */

  for (b = table[hash]; b; b = b->next)
    if (b->node == node)
	return 1;
  
  if (record_it)
    {
      b = (struct bucket *) xmalloc (sizeof (struct bucket));
      b->node = node;
      b->next = table[hash];
      table[hash] = b;
    }
    
  return 0;
}

/* Free the hash table used to record visited nodes.  */
static void
free_hash_table()
{
  int hash;
  struct bucket *b, *next;
  
  if (!table)
    return;
  
  for (hash = 0; hash < HASH_SIZE; ++hash)
    {
      b = table[hash];
      while (b)
	{
	  next = b->next;
	  free (b);
	  b = next;
	}
    }
    
  free (table);
  table = NULL;
}


/*-------------------------------------------------------------------*/

void
print_type (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  int newline = 0;
  
#   define PRINT_TYPE_NAME(node) 					\
    if (!newline && TYPE_NAME (node))					\
      {									\
	if (TREE_CODE (TYPE_NAME (node)) == IDENTIFIER_NODE)		\
	  fprintf (file, " %s", IDENTIFIER_POINTER (TYPE_NAME (node)));	\
	else if (TREE_CODE (TYPE_NAME (node)) == TYPE_DECL		\
		 && DECL_NAME (TYPE_NAME (node)))			\
	  fprintf (file, " %s",						\
		   IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (node))));	\
        newline = 1;							\
      }
  
  if (TYPE_CONTEXT (node))
    {
      fprintf (file, " cntxt=");
      fprintf (file, HOST_PTR_PRINTF, 
      			HOST_PTR_PRINTF_VALUE (TYPE_CONTEXT (node)));
    }
      
  if (TYPE_NEEDS_CONSTRUCTING (node))
    fputs (" need-ctor", file);
  if (TYPE_PACKED (node))
    fputs (" packed", file);
  if (TREE_THIS_VOLATILE (node))
    fputs (" volatile", file);
  if (TYPE_READONLY (node))
    fputs (" const", file);
  if (TYPE_RESTRICT (node))
    fputs (" restrict", file);
  if (TYPE_UNSIGNED (node))
    fputs (" uns", file);
    
  if (TYPE_LANG_FLAG_0 (node)
      || TYPE_LANG_FLAG_1 (node)
      || TYPE_LANG_FLAG_2 (node)
      || TYPE_LANG_FLAG_3 (node)
      || TYPE_LANG_FLAG_4 (node)
      || TYPE_LANG_FLAG_5 (node)
      || TYPE_LANG_FLAG_6 (node))
    {
      fputs (" type-flags=#", file);
      if (TYPE_LANG_FLAG_0 (node))
	fputc ('0', file);
      if (TYPE_LANG_FLAG_1 (node))
	fputc ('1', file);
      if (TYPE_LANG_FLAG_2 (node))
	fputc ('2', file);
      if (TYPE_LANG_FLAG_3 (node))
	fputc ('3', file);
      if (TYPE_LANG_FLAG_4 (node))
	fputc ('4', file);
      if (TYPE_LANG_FLAG_5 (node))
	fputc ('5', file);
      if (TYPE_LANG_FLAG_6 (node))
	fputc ('6', file);
    }
  
  if (TYPE_USER_ALIGN (node))
    fprintf (file, " usr-algn");
  
  fprintf (file, " align=%d", TYPE_ALIGN (node));
  
  if (TYPE_SYMTAB_ADDRESS (node))
    fprintf (file, " symtab=%d", TYPE_SYMTAB_ADDRESS (node));
  
  if (TYPE_ALIAS_SET (node) != -1)
    {
      fprintf (file, " alias-set=");
      fprintf (file, HOST_WIDE_INT_PRINT_DEC, TYPE_ALIAS_SET (node));
    }
  
  if (TYPE_POINTER_TO (node))
    {
      fprintf (file, " *this=");
      fprintf (file, HOST_PTR_PRINTF,
    		HOST_PTR_PRINTF_VALUE (TYPE_POINTER_TO (node)));
    }
  
  if (TYPE_REFERENCE_TO (node))
    {
      fprintf (file, " &this=");
      fprintf (file, HOST_PTR_PRINTF,
    		HOST_PTR_PRINTF_VALUE (TYPE_REFERENCE_TO (node)));
    }
  
  if (TREE_CODE (node) == ARRAY_TYPE || TREE_CODE (node) == SET_TYPE)
    {
      if (TYPE_DOMAIN (node))
	{
	  fprintf (file, " domain=");
	  fprintf (file, HOST_PTR_PRINTF,
			HOST_PTR_PRINTF_VALUE (TYPE_DOMAIN (node)));
	}
    }
  else if (TREE_CODE (node) == ENUMERAL_TYPE)
    {
      if (TYPE_VALUES (node))
	{
	  fprintf (file, " values=");
	  fprintf (file, HOST_PTR_PRINTF,
			HOST_PTR_PRINTF_VALUE (TYPE_VALUES (node)));
	}
    }
  else if (TREE_CODE (node) == VECTOR_TYPE)
    {
      if (TYPE_DEBUG_REPRESENTATION_TYPE (node))
	{
	  fprintf (file, " values=");
	  fprintf (file, HOST_PTR_PRINTF,
			HOST_PTR_PRINTF_VALUE (TYPE_DEBUG_REPRESENTATION_TYPE (node)));
	}
    }
  
  if (TYPE_ATTRIBUTES (node))
    {
      fprintf (file, " attr=");
      fprintf (file, HOST_PTR_PRINTF,
  			HOST_PTR_PRINTF_VALUE (TYPE_ATTRIBUTES (node)));
    }
  
  if (TYPE_PRECISION (node))
    fprintf (file, " prec=%d", TYPE_PRECISION (node));
    
  if (TYPE_MAIN_VARIANT (node) != node)
    {
      fprintf (file, " main-variant=");
      fprintf (file, HOST_PTR_PRINTF,
		    HOST_PTR_PRINTF_VALUE (TYPE_MAIN_VARIANT (node)));
    }
  if (TYPE_NEXT_VARIANT (node))
    {
      fprintf (file, " next-variant=");
      fprintf (file, HOST_PTR_PRINTF,
		    HOST_PTR_PRINTF_VALUE (TYPE_NEXT_VARIANT (node)));
    }
  
  if (TYPE_NAME (node) && TREE_CODE (TYPE_NAME (node)) == TYPE_DECL
      && DECL_ORIGINAL_TYPE (TYPE_NAME (node)))
    { 
      fprintf (file, " orig-type=");
      fprintf (file, HOST_PTR_PRINTF,
		      HOST_PTR_PRINTF_VALUE (DECL_ORIGINAL_TYPE (TYPE_NAME (node))));
    }

  (void)node_seen (node, TRUE);		/* prevent recursion on this node */
  
  (*lang_hooks.dump_type) (file, node, indent, 0);
  
  if (TYPE_SIZE (node) 
      && TREE_CODE (TYPE_SIZE (node)) == INTEGER_CST
      && TYPE_SIZE_UNIT (node)
      && TREE_CODE (TYPE_SIZE_UNIT (node)) == INTEGER_CST)
    {
      fputs (" sz=", file);
      print_integer_constant (file, TYPE_SIZE (node), 0);
      fputs ("(", file);
      print_integer_constant (file, TYPE_SIZE_UNIT (node), 0);
      fputs (")", file);
    }
  else
    {
      PRINT_TYPE_NAME(node);
      dump_tree (file, "(size)", TYPE_SIZE (node), indent + INDENT);
      dump_tree (file, "(unit size)", TYPE_SIZE_UNIT (node), indent + INDENT);
    }
  
  if (INTEGRAL_TYPE_P (node))
    {
      if (!newline &&
          TREE_CODE (TYPE_MIN_VALUE (node)) == INTEGER_CST
      	  && TREE_CODE (TYPE_MAX_VALUE (node)) == INTEGER_CST)
        {
      	  fputs (" min/max=", file);
      	  print_integer_constant (file, TYPE_MIN_VALUE (node), 0);
      	  fputc ('/', file);
      	  print_integer_constant (file, TYPE_MAX_VALUE (node), 0);
        }
      else
        {
	  PRINT_TYPE_NAME(node);
	  dump_tree (file, "(min)", TYPE_MIN_VALUE (node), indent + INDENT);
	  dump_tree (file, "(max)", TYPE_MAX_VALUE (node), indent + INDENT);
        }
    }
    
  PRINT_TYPE_NAME(node);
  
  if (TYPE_ATTRIBUTES (node))
    dump_tree (file, NULL, TYPE_ATTRIBUTES (node), indent + INDENT);
    
  (*lang_hooks.dump_type) (file, node, indent, 1);
  
  if (TREE_CODE (node) == ARRAY_TYPE || TREE_CODE (node) == SET_TYPE)
    {
      if (TYPE_DOMAIN (node))
	dump_tree (file, "(domain)", TYPE_DOMAIN (node), indent + INDENT);
    }
  else if (TREE_CODE (node) == ENUMERAL_TYPE)
    {
      if (TYPE_VALUES (node))
  	dump_tree (file, "(values)", TYPE_VALUES (node), indent + INDENT);
    }
  
#if 0
  if (TYPE_MAIN_VARIANT (node) == node)
    {
      tree n = TYPE_MAIN_VARIANT (node);
      dump_tree (file, "(main-variant)", n, indent + INDENT);
      for (n = TYPE_NEXT_VARIANT (n); n; n = TYPE_NEXT_VARIANT (n))
        dump_tree (file, "(next-variant)", n, indent + INDENT);
    }
#endif
  
#if 0
  if (TYPE_POINTER_TO (node))
    dump_tree (file, "(ptr-to-this)", TYPE_POINTER_TO (node), indent + INDENT);
  
  if (TYPE_REFERENCE_TO (node))
    dump_tree (file, "(ref-to-this)", TYPE_REFERENCE_TO (node), indent + INDENT);
#endif
  
  if (TREE_TYPE (node))
    dump_tree (file, NULL, TREE_TYPE (node), indent + INDENT);
    
  /* I THINK SOME BINFO-RELATED MACROS NEED TO BE INVESTIGATED AND PUT HERE...  */
}

void
print_decl (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent ATTRIBUTE_UNUSED;
{
  int newline = 0;
  
  if (!node_seen (node, FALSE))
    {
      if (TREE_SIDE_EFFECTS (node) || TREE_THIS_VOLATILE (node))
      	fputs (" volatile", file);
      if (DECL_IGNORED_P (node))
	fputs (" ignored", file);
      if (DECL_ABSTRACT (node))
	fputs (" abst", file);
      if (DECL_IN_SYSTEM_HEADER (node))
	fputs (" in-sys-hdr", file);
      if (DECL_COMMON (node))
	fputs (" comm", file);
      if (DECL_NONLOCAL (node))
	fputs (" nonlcl", file);
#if 0
      if (DECL_ARTIFICIAL (node))
        fputs (" artifical", file);
#endif
      if (DECL_WEAK (node))
        fputs (" weak", file);
      
      if (DECL_LANG_FLAG_0 (node)
          || DECL_LANG_FLAG_1 (node)
          || DECL_LANG_FLAG_2 (node)
          || DECL_LANG_FLAG_3 (node)
          || DECL_LANG_FLAG_4 (node)
          || DECL_LANG_FLAG_5 (node)
          || DECL_LANG_FLAG_6 (node)
          || DECL_LANG_FLAG_7 (node))
        {
      	  fputs (" lang-flags=#", file);
	  if (DECL_LANG_FLAG_0 (node))
	    fputc ('0', file);
	  if (DECL_LANG_FLAG_1 (node))
	    fputc ('1', file);
	  if (DECL_LANG_FLAG_2 (node))
	    fputc ('2', file);
	  if (DECL_LANG_FLAG_3 (node))
	    fputc ('3', file);
	  if (DECL_LANG_FLAG_4 (node))
	    fputc ('4', file);
	  if (DECL_LANG_FLAG_5 (node))
	    fputc ('5', file);
	  if (DECL_LANG_FLAG_6 (node))
	    fputc ('6', file);
	  if (DECL_LANG_FLAG_7 (node))
	    fputc ('7', file);
        }
  }
  
  if (DECL_ATTRIBUTES (node))
    {
      fprintf (file, " attr=");
      fprintf (file, HOST_PTR_PRINTF,
  			HOST_PTR_PRINTF_VALUE (DECL_ATTRIBUTES (node)));
    }
  
  if (DECL_CONTEXT (node))
    {
      fprintf (file, " cntxt=");
      fprintf (file, HOST_PTR_PRINTF,
		      HOST_PTR_PRINTF_VALUE (DECL_CONTEXT (node)));
    }
  
  if (DECL_RTL_SET_P (node))
    {
      fprintf (file, " rtl=");
      fprintf (file, HOST_PTR_PRINTF,
		      HOST_PTR_PRINTF_VALUE (DECL_RTL (node)));
    }
    
  (*lang_hooks.dump_decl) (file, node, indent, 0);
 
  if (!node_seen (node, TRUE))	/* mark ..._DECL node seen b4 recursing */
    {
      if (TREE_CODE (node) == FIELD_DECL)
        {
          if (DECL_BIT_FIELD_TYPE (node))
            {
              tree bf_type = DECL_BIT_FIELD_TYPE (node);
	      fprintf (file, " bf-type=");
	      fprintf (file, HOST_PTR_PRINTF,
		       HOST_PTR_PRINTF_VALUE (bf_type));
	      if (TYPE_NAME (bf_type))
		{
		  if (TREE_CODE (TYPE_NAME (bf_type)) == IDENTIFIER_NODE)
		    {
		      if (IDENTIFIER_POINTER (TYPE_NAME (bf_type))
			  && *IDENTIFIER_POINTER (TYPE_NAME (bf_type)))
		      fprintf (file, " {%s}", IDENTIFIER_POINTER (TYPE_NAME (bf_type)));
		    }
		  else if (TREE_CODE (TYPE_NAME (bf_type)) == TYPE_DECL
			   && DECL_NAME (TYPE_NAME (bf_type))
			   && IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (bf_type)))
			   && *IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (bf_type))))
		    fprintf (file, " {%s}",
			     IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (bf_type))));
		}
	      else
		fprintf (file, " {%s}", tree_code_name[(int)TREE_CODE (bf_type)]);
	    }
	    
	  if (DECL_FIELD_OFFSET (node) && DECL_FIELD_BIT_OFFSET (node))
	    {
	      if (TREE_CODE (DECL_FIELD_OFFSET (node)) == INTEGER_CST
		  && TREE_CODE (DECL_FIELD_BIT_OFFSET (node)) == INTEGER_CST)
		{
		  fputs (" off=", file);
		  print_integer_constant (file, DECL_FIELD_BIT_OFFSET (node), 0);
		  fputs ("(", file);
		  print_integer_constant (file, DECL_FIELD_OFFSET (node), 0);
		  fputs (")", file);
		}
	      else
		{
		  if (DECL_NAME (node))
		    fprintf (file, " %s", IDENTIFIER_POINTER (DECL_NAME (node)));
		  dump_tree (file, "(offset)", DECL_FIELD_OFFSET (node),
			     indent + INDENT);
		  dump_tree (file, "(bit offset)", DECL_FIELD_BIT_OFFSET (node),
			     indent + INDENT);
		  newline = 1;
		}
	    }
	  else 
	    {
	      if (DECL_FIELD_OFFSET (node))
	        {
	          if (TREE_CODE (DECL_FIELD_OFFSET (node)) == INTEGER_CST)
	            {
		      fputs (" off=", file);
		      print_integer_constant (file, DECL_FIELD_BIT_OFFSET (node), 0);
		    }
		  else
		    {
		      if (DECL_NAME (node))
			fprintf (file, " %s", IDENTIFIER_POINTER (DECL_NAME (node)));
		      dump_tree (file, "(offset)", DECL_FIELD_OFFSET (node),
				 indent + INDENT);
		      newline = 1;
		    }
	        }
	      if (DECL_FIELD_BIT_OFFSET (node))
	        {
	          if (!newline
	              && TREE_CODE (DECL_FIELD_BIT_OFFSET (node)) == INTEGER_CST)
	            {
		      fputs (" bit-off=", file);
		      print_integer_constant (file, DECL_FIELD_BIT_OFFSET (node), 0);
		    }
		  else
		    {
		      if (!newline && DECL_NAME (node))
			fprintf (file, " %s", IDENTIFIER_POINTER (DECL_NAME (node)));
		      dump_tree (file, "(bit offset)", DECL_FIELD_BIT_OFFSET (node),
				 indent + INDENT);
		      newline = 1;
		    }
	        }
	    }
        }
      
      if (!newline && DECL_SIZE (node) && DECL_SIZE_UNIT (node)
          && TREE_CODE (DECL_SIZE (node)) == INTEGER_CST
          && TREE_CODE (DECL_SIZE_UNIT (node)) == INTEGER_CST)
	{
	  fputs (" sz=", file);
	  print_integer_constant (file, DECL_SIZE (node), 0);
	  fputs ("(", file);
	  print_integer_constant (file, DECL_SIZE_UNIT (node), 0);
	  fputs (")", file);
        }
      else
        {
	  if (!newline && DECL_NAME (node))
	    fprintf (file, " %s", IDENTIFIER_POINTER (DECL_NAME (node)));
	  newline = 1;
     	  dump_tree (file, "(size)", DECL_SIZE (node), indent + INDENT);
      	  dump_tree (file, "(unit size)", DECL_SIZE_UNIT (node), indent + INDENT);
        }
    }
  
  if (!newline && DECL_NAME (node))
    fprintf (file, " %s", IDENTIFIER_POINTER (DECL_NAME (node)));
  
  (*lang_hooks.dump_decl) (file, node, indent, 1);
}

void
print_ref (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent ATTRIBUTE_UNUSED;
{
  if (TREE_THIS_VOLATILE (node))
    fputs (" volatile", file);
  if (TREE_READONLY (node))
    fputs (" readonly", file);
}

#define MAX_COMMENT 50

/* The "..." arguments are a list of zero or more strings with a NULL
   as the last list item.  They are used to annotate each respective
   operand.  The annotation convention is that if an string is not
   enclosed in parentheses it will annotation it's operand display. If
   it is enclosed in parentheses it will be appended to the kind:address
   display.  Examples:
   
      annotation kind:address ....
  or
      kind:address (annotation) ....
*/
      
void 
print_operands VPARAMS ((FILE *file, tree node, int indent, int follow, ...))
{
#ifndef ANSI_PROTOTYPES
  FILE *file;
  tree node;
  int indent, follow;
#endif
  int i, first_rtl, len, len1, nomore, maxlen;
  va_list ap;
  char *s, temp[MAX_COMMENT + 2];
  
  VA_START (ap, follow);
#ifndef ANSI_PROTOTYPES
  file = va_arg (ap, FILE *);
  node = va_arg (ap, tree);
  indent = va_arg (ap, int);
  follow = va_arg (ap, int);
#endif
  
  len = TREE_CODE_LENGTH (TREE_CODE (node));
  first_rtl = first_rtl_op (TREE_CODE (node));
    
  for (i = nomore = maxlen = 0; i < len && i < first_rtl; ++i)
    {
      s = nomore ? NULL : va_arg (ap, char *);
      if (s == NULL)
	{
	  nomore = 1;
      	  fprintf (file, " ");
	  fprintf (file, HOST_PTR_PRINTF,	
      	  		HOST_PTR_PRINTF_VALUE (TREE_OPERAND (node, i)));
	}
      else
        {
          if (s[0] != '(')
            {
	      len1 = strlen (s);
	      if (len1 > maxlen)
		maxlen = len1;
            }
          else
            {
              s = strcpy (temp, ++s);
              temp[strlen (temp) - 1] = '\0';
            }
	  fprintf (file, " %s=", s);
	  fprintf (file, HOST_PTR_PRINTF,
	  			HOST_PTR_PRINTF_VALUE (TREE_OPERAND (node, i)));
	}
    }
  
  va_end (ap);
  
  if (first_rtl < len)
    {
      for (i = first_rtl; i < len; ++i)
        if (TREE_OPERAND (node, i))
          break;
      if (i < len)
        {
	  fputs (" [rtl=", file);
	  for (i = first_rtl; i < len; ++i)
	    {
	      if (i > first_rtl)
		fputc (',', file);
	      fprintf (file, HOST_PTR_PRINTF,
			    HOST_PTR_PRINTF_VALUE (TREE_OPERAND (node, i)));
	    }
	  fputc (']', file);
      }
    }
    
  if (follow && dump_tree_state.really_follow)
    {
      maxlen = MIN (maxlen, MAX_COMMENT);
      temp[maxlen] = '\0';
      
      /* Reusing the arg list -- does this work when !ANSI_PROTOTYPES?  */
      VA_START (ap, follow);
#     ifndef ANSI_PROTOTYPES
      file = va_arg (ap, FILE *);
      node = va_arg (ap, tree);
      indent = va_arg (ap, int);
#     endif
      
      for (i = nomore = 0; i < len && i < first_rtl; ++i)
	{
	  s = nomore ? NULL : va_arg (ap, char *);
	  if (s == NULL)
	    {
	      nomore = 1;
	      if (maxlen > 0)
	        s = memset(temp, ' ', maxlen);
	    }
	  else if (s[0] != '(')
	    {
	      len = strlen (s);
	      if (maxlen < len)
		  len = maxlen;
	      if (len < maxlen)
		{
		  memset(temp, ' ', maxlen);
		  s = memcpy(temp + maxlen - len - 1, s, len);
		}
	    }
	  dump_tree (file, s, TREE_OPERAND (node, i), indent + INDENT);
	}
      
      va_end (ap);
    }
}

void
print_lineno (file, node)
     FILE *file;
     tree node;
{
  if (!(*lang_hooks.dump_lineno_p) (file, node)
      && TREE_CODE_CLASS (TREE_CODE (node)) == 'd')
    {
      if (TREE_CODE (node) != FUNCTION_DECL || !DECL_BUILT_IN (node))
        {
	  if (dump_tree_state.curr_file
	      && strcmp(dump_tree_state.curr_file, DECL_SOURCE_FILE (node)) == 0)
	    fprintf (file, " line=%d", DECL_SOURCE_LINE (node));
	  else
	    {
	      dump_tree_state.curr_file = (char *)DECL_SOURCE_FILE (node);
	      fprintf (file, " line=%d(%s)", DECL_SOURCE_LINE (node),
			      lbasename(DECL_SOURCE_FILE (node)));
	  }
	}
    }
}

void 
print_integer_constant (file, node, hex)
     FILE *file;
     tree node;
     int hex;
{
  int ok = 1;
  tree type = TREE_TYPE (node);
  int size;
  
  size  = (TREE_CODE_CLASS (TREE_CODE (type)) == 't') ? 
           CST_VALUE (TYPE_SIZE_UNIT (type), ok)
    	   : 4; /*CST_VALUE (type,ok);*/
  if (!ok)
    size = 4;
  
  if (hex)
    {
      if (TREE_INT_CST_HIGH (node) == 0
	  || (TREE_INT_CST_HIGH (node) == -1 && TREE_INT_CST_LOW (node) != 0))
	{
	  if (size == 1)
	    fprintf (file, "0x%.2lX = ", (unsigned long)(TREE_INT_CST_LOW (node) & 0xFF));
	  else if (size == 2)
	    fprintf (file, "0x%.4lX = ", (unsigned long)(TREE_INT_CST_LOW (node) & 0xFFFF));
	  else if (size == 4)
	    fprintf (file, "0x%.8lX = ", (unsigned long)TREE_INT_CST_LOW (node));
	  else
	    fprintf (file, HOST_WIDE_INT_PRINT_DOUBLE_HEX,
		     TREE_INT_CST_HIGH (node), TREE_INT_CST_LOW (node));
	}
    }
    
  if (TREE_INT_CST_HIGH (node) == 0)
    fprintf (file, HOST_WIDE_INT_PRINT_UNSIGNED, TREE_INT_CST_LOW (node));
  else if (TREE_INT_CST_HIGH (node) == -1 && TREE_INT_CST_LOW (node) != 0)
    {
      fputs ("-", file);
      fprintf (file, HOST_WIDE_INT_PRINT_UNSIGNED, -TREE_INT_CST_LOW (node));
    }
  else
    fprintf (file, HOST_WIDE_INT_PRINT_DOUBLE_HEX,
	     TREE_INT_CST_HIGH (node), TREE_INT_CST_LOW (node));
}

void
print_real_constant (file, node)
     FILE *file;
     tree node;
{
  char string[100];
  real_to_decimal (string, &TREE_REAL_CST (node), sizeof (string), 0, 1);
  fputs (string, file);
}

void 
print_string_constant (file, str, maxlen)
     FILE *file;
     const char *str;
     int maxlen;
{
  char c, buf[1024+12+1], *s;
  int  i = 0, len;
  
  if (!str)
    {
      fputs ("(null)", file);
      return;
    }
  
  /* Buffer string so that we write it with a single fputs().  This is
     required for our line wrapping code to know when it is "inside"
     a string.  */
     
  len = strlen (str);
  if (len > 1021)
    len = 1021;
  
  s    = buf;
  *s++ = '"';
  
  while ((c = *str++) != '\0')
    {
      if (c == '\b') {
	*s++ =  '\\'; *s++ = 'b'; i += 2;
      } else if (c == '\f') {
	*s++ =  '\\'; *s++ = 'f'; i += 2;
      } else if (c == '\v') {
	*s++ =  '\\'; *s++ = 'v'; i += 2;
      } else if (c == '\r') {
	*s++ =  '\\'; *s++ = 'r'; i += 2;
      } else if (c == '\n') {
	*s++ =  '\\'; *s++ = 'n'; i += 2;
      } else if (c == '\t') {
	*s++ =  '\\'; *s++ = 't'; i += 2;
      } else if (!isprint (c)) {
	s += sprintf (s, "\\%03o", c); i += 4;
      } else {
	*s++ = c; ++i;
      }
      if (i > maxlen && len > maxlen + 12)
        {
          strcpy (s, "...(more)...");
          break;
        }
    }
  *s++ = '"';
  *s   = '\0';
  fputs (buf, file);
}

void
print_tree_flags (file, node)
     FILE *file;
     tree node;
{
  if (TREE_SIDE_EFFECTS (node))
    fputs ("side-effects", file);
  if (TREE_CONSTANT (node))
    fputs (" const", file);
  if (TREE_ADDRESSABLE (node))
    fputs (" addressable", file);
  if (TREE_THIS_VOLATILE (node))
    fputs (" volatile", file);
  if (TREE_READONLY (node))
    fputs (" readonly", file);
  if (TREE_ASM_WRITTEN (node))
    fputs (" asm-written", file);
  if (TREE_USED (node))
    fputs (" used", file);
  if (TREE_NOTHROW (node))
    fputs (" nothrow", file);
  if (TREE_STATIC (node))
    fputs (" static", file);
  if (TREE_PUBLIC (node))
    fputs (" public", file);
  if (TREE_PRIVATE (node))
    fputs (" private", file);
  if (TREE_PROTECTED (node))
    fputs (" protected", file);
  if (TREE_BOUNDED (node))
    fputs (" bounded", file);
  if (TREE_DEPRECATED (node))
    fputs (" deprecated", file);
  /* APPLE LOCAL begin "unavailable" attribute (radar 2809697) */
  if (TREE_UNAVAILABLE (node))
    fputs (" unavailable", file);
  /* APPLE LOCAL end "unavailable" attribute (radar 2809697) */
 
  if (TREE_LANG_FLAG_0 (node)
      || TREE_LANG_FLAG_1 (node)
      || TREE_LANG_FLAG_2 (node)
      || TREE_LANG_FLAG_3 (node)
      || TREE_LANG_FLAG_4 (node)
      || TREE_LANG_FLAG_5 (node)
      || TREE_LANG_FLAG_6 (node))
    {
      fputs (" tree-flags=#", file);
      if (TREE_LANG_FLAG_0 (node))
	fputc ('0', file);
      if (TREE_LANG_FLAG_1 (node))
	fputc ('1', file);
      if (TREE_LANG_FLAG_2 (node))
	fputc ('2', file);
      if (TREE_LANG_FLAG_3 (node))
	fputc ('3', file);
      if (TREE_LANG_FLAG_4 (node))
	fputc ('4', file);
      if (TREE_LANG_FLAG_5 (node))
	fputc ('5', file);
      if (TREE_LANG_FLAG_6 (node))
	fputc ('6', file);
    }
}

/*-------------------------------------------------------------------*/

/* Just in case print anything set in the common part of this node.  */

static void
print_ERROR_MARK (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent ATTRIBUTE_UNUSED;
{
  if (TREE_CHAIN (node))
    {
      fprintf (file, " chain=");
      fprintf (file, HOST_PTR_PRINTF,
	      HOST_PTR_PRINTF_VALUE (TREE_CHAIN (node)));
    }
  if (TREE_TYPE (node))
    {
      fprintf (file, " type=");
      fprintf (file, HOST_PTR_PRINTF,
	      HOST_PTR_PRINTF_VALUE (TREE_TYPE (node)));
    }
  print_tree_flags (file, node);
}

static void
print_IDENTIFIER_NODE (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  if (TREE_PUBLIC (node))
      fputs (" public", file);
  if (TREE_ADDRESSABLE (node))
    fputs (" addressable", file);
  
  (*lang_hooks.dump_identifier) (file, node, indent, 0);
  fprintf (file, " len=%d %s", 
  		IDENTIFIER_LENGTH (node), IDENTIFIER_POINTER (node));
  (*lang_hooks.dump_identifier) (file, node, indent, 1);
}

static void
print_TREE_LIST (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  if (TREE_VIA_VIRTUAL (node))
    fputs (" via-virt", file);
  if (TREE_VIA_PUBLIC (node))
    fputs (" via-public", file);
  if (TREE_VIA_PRIVATE (node))
    fputs (" via-private", file);
  if (TREE_VIA_PROTECTED (node))
    fputs (" via-protected", file);
  fprintf (file, " purpose=");
  fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE (TREE_PURPOSE (node)));
  fprintf (file, " value=");
  fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE (TREE_VALUE (node)));
  fprintf (file, " chain=");
  fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE (TREE_CHAIN (node)));
  		
  ++dump_tree_state.doing_tree_list;
  (void)node_seen (node, TRUE);
  
  dump_tree (file, "(purpose)", TREE_PURPOSE (node), indent + INDENT);
  dump_tree (file, "(value)", TREE_VALUE (node), indent + INDENT);
  
  for (node = TREE_CHAIN (node); node; node = TREE_CHAIN (node))
    dump_tree (file, annotation, node, indent + 0); /* keep list at same indent */
  
  --dump_tree_state.doing_tree_list;
}

static void
print_TREE_VEC (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  int i, skip_line, len = TREE_VEC_LENGTH (node);
  
  if (TREE_VIA_VIRTUAL (node))
    fputs (" via-virt", file);
  if (TREE_VIA_PUBLIC (node))
    fputs (" via-public", file);
  if (TREE_VIA_PRIVATE (node))
    fputs (" via-private", file);
  if (TREE_VIA_PROTECTED (node))
    fputs (" via-protected", file);
    
  fprintf (file, " len=%d", len);
  
  for (i = skip_line = 0; i < len; ++i)
    if (TREE_VEC_ELT (node, i))
      {
	char temp[MAX_COMMENT + 20];
	
	if (skip_line)
	  newline_and_indent (file, 0);
	skip_line = 1;
	if (annotation)
	  {
	    temp[0] = '(';
	    if (annotation[0] == '(')
	      {
		strcpy (temp+1, annotation+1);
		temp[strlen (temp) - 1] = '\0';
	      }
	    else
	      strcpy (temp+1, annotation);
	    sprintf (temp + strlen (temp), ":%i)", i);
	  }
	else
	    sprintf (temp, "(%i)", i);
	dump_tree (file, temp, TREE_VEC_ELT (node, i), indent + INDENT);
      }
  
  if (len > 1 && indent > 1)
    newline_and_indent (file, 0);
}

static void
print_BLOCK (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent ATTRIBUTE_UNUSED;
{
  tree n;
  
  fprintf (file, " vars=");
  fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE (BLOCK_VARS (node)));
  fprintf (file, " abs-orig=");
  fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE (BLOCK_ABSTRACT_ORIGIN (node)));
  fprintf (file, " super=");
  fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE (BLOCK_SUPERCONTEXT (node)));
  fprintf (file, " sub=");
  fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE (BLOCK_SUBBLOCKS (node)));
  fprintf (file, " frag-origin=");
  fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE (BLOCK_FRAGMENT_ORIGIN (node)));
  fprintf (file, " frag-chain=");
  fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE (BLOCK_FRAGMENT_CHAIN (node)));

  if (BLOCK_HANDLER_BLOCK (node))
      fputs (" handler_block_flag", file);
  if (BLOCK_ABSTRACT (node))
      fputs (" abstract_flag", file);
  
  for (n = BLOCK_VARS (node); n; n = TREE_CHAIN (n))
    if (!node_seen (n, FALSE))
      dump_tree (file, NULL, n, indent + INDENT);
  
#if 0
  for (n = BLOCK_SUBBLOCKS (node); n; n = BLOCK_CHAIN (n))
    if (!node_seen (n))
      dump_tree (file, NULL, n, indent + INDENT);
#endif
  
  /*dump_tree (file, NULL, BLOCK_SUPERCONTEXT (node), indent + INDENT);*/
  
  if (!node_seen (BLOCK_ABSTRACT_ORIGIN (node), FALSE))
    dump_tree (file, NULL, BLOCK_ABSTRACT_ORIGIN (node), indent + INDENT);
}

static void
print_VOID_TYPE (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  print_type (file, annotation, node, indent);
}

static void
print_INTEGER_TYPE (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  if (TYPE_IS_SIZETYPE (node))
    fputs (" sizetype", file);
  if (TYPE_UNSIGNED (node))
    fputs (" uns", file);
    
  print_type (file, annotation, node, indent);
}

static void
print_REAL_TYPE (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  print_type (file, annotation, node, indent);
}

static void
print_COMPLEX_TYPE (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  print_type (file, annotation, node, indent);
}

static void
print_VECTOR_TYPE (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  print_type (file, annotation, node, indent);
}

static void
print_ENUMERAL_TYPE (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  if (TYPE_UNSIGNED (node))
    fputs (" uns", file);
    
  print_type (file, annotation, node, indent);
}

static void
print_BOOLEAN_TYPE (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  print_type (file, annotation, node, indent);
}

static void
print_CHAR_TYPE (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  print_type (file, annotation, node, indent);
}

static void
print_POINTER_TYPE (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  print_type (file, annotation, node, indent);
}

static void
print_OFFSET_TYPE (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  fprintf (file, " basetype=");
  fprintf (file, HOST_PTR_PRINTF,
  			HOST_PTR_PRINTF_VALUE (TYPE_OFFSET_BASETYPE (node)));
  
  print_type (file, annotation, node, indent);
}

static void
print_REFERENCE_TYPE (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  print_type (file, annotation, node, indent);
}

static void
print_METHOD_TYPE (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  tree n;
  
  if (TYPE_METHOD_BASETYPE (node))
    {
      fprintf (file, " basetype=");
      fprintf (file, HOST_PTR_PRINTF,
    			HOST_PTR_PRINTF_VALUE (TYPE_METHOD_BASETYPE (node)));
    }
  fprintf (file, " args=");
  fprintf (file, HOST_PTR_PRINTF,
  			HOST_PTR_PRINTF_VALUE (TYPE_ARG_TYPES (node)));
  
  print_type (file, annotation, node, indent);
  
  for (n = TYPE_ARG_TYPES (node); n; n = TREE_CHAIN (n))
    dump_tree (file, "(args)", n, indent + INDENT);
}

static void
print_FILE_TYPE (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  print_type (file, annotation, node, indent);
}

static void
print_ARRAY_TYPE (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  if (TYPE_STRING_FLAG (node))
    fputs (" string-flag", file);
  if (TYPE_NONALIASED_COMPONENT (node))
    fputs (" nonaliased-component", file);
    
  print_type (file, annotation, node, indent);
}

static void
print_SET_TYPE (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  if (TYPE_STRING_FLAG (node))
    fputs (" string-flag", file);

  print_type (file, annotation, node, indent);
}

static void
print_RECORD_TYPE (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  tree n;
  
  if (TYPE_NO_FORCE_BLK (node))
    fputs (" no-force-blk", file);
  fprintf (file, " fields=");
  fprintf (file, HOST_PTR_PRINTF,
  			HOST_PTR_PRINTF_VALUE (TYPE_FIELDS (node)));
    
  print_type (file, annotation, node, indent);
  (void)node_seen (node, TRUE);
    
  for (n = TYPE_FIELDS (node); n; n = TREE_CHAIN (n))
    dump_tree (file, NULL, n, indent + INDENT);
}

static void
print_UNION_TYPE (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  if (TYPE_NO_FORCE_BLK (node))
    fputs (" no-force-blk", file);
  if (TYPE_TRANSPARENT_UNION (node))
    fputs (" transparent-union", file);
  fprintf (file, " fields=");
  fprintf (file, HOST_PTR_PRINTF,
  			HOST_PTR_PRINTF_VALUE (TYPE_FIELDS (node)));
    
  print_type (file, annotation, node, indent);
  (void)node_seen (node, TRUE);
    
  for (node = TYPE_FIELDS (node); node; node = TREE_CHAIN (node))
    dump_tree (file, NULL, node, indent + INDENT);
}

static void
print_QUAL_UNION_TYPE (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{  
  if (TYPE_NO_FORCE_BLK (node))
    fputs (" no-force-blk", file);
  fprintf (file, " fields=");
  fprintf (file, HOST_PTR_PRINTF,
  				HOST_PTR_PRINTF_VALUE (TYPE_FIELDS (node)));
    
  print_type (file, annotation, node, indent);
  (void)node_seen (node, TRUE);
  
  for (node = TYPE_FIELDS (node); node; node = TREE_CHAIN (node))
    dump_tree (file, NULL, node, indent + INDENT);
}

static void
print_FUNCTION_TYPE (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  tree n;
  
  if (TYPE_METHOD_BASETYPE (node))
    {
      fprintf (file, " basetype=");
      fprintf (file, HOST_PTR_PRINTF,
    			HOST_PTR_PRINTF_VALUE (TYPE_METHOD_BASETYPE (node)));
    }
    
  if (TYPE_RETURNS_STACK_DEPRESSED (node))
    fputs (" ret-stk-depressed", file);
  if (TYPE_AMBIENT_BOUNDEDNESS (node))
    fputs (" ambient-boundedness", file);
  fprintf (file, " args=");
  fprintf (file, HOST_PTR_PRINTF,
  			HOST_PTR_PRINTF_VALUE (TYPE_ARG_TYPES (node)));
  
  print_type (file, annotation, node, indent);
  (void)node_seen (node, TRUE);
    
  for (n = TYPE_ARG_TYPES (node); n; n = TREE_CHAIN (n))
    dump_tree (file, "(args)", n, indent + INDENT);
}

static void
print_LANG_TYPE (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  print_type (file, annotation, node, indent);
}

static void
print_INTEGER_CST (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent ATTRIBUTE_UNUSED;
{
  print_tree_flags (file, node);
  fputc (' ', file);
  print_integer_constant (file, node, 1);
}

static void
print_REAL_CST (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent ATTRIBUTE_UNUSED;
{
  print_tree_flags (file, node);
  fputc (' ', file);
  print_real_constant (file, node);
}

static void
print_COMPLEX_CST (file, annotation, node, indent)
     FILE *file ATTRIBUTE_UNUSED;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_tree_flags (file, node);
  fprintf (file, " real=");
  fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE (TREE_REALPART (node)));
  fprintf (file, " imag=");
  fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE (TREE_IMAGPART (node)));
  
  dump_tree (file, "(real)", TREE_REALPART (node), indent + INDENT);
  dump_tree (file, "(imag)", TREE_IMAGPART (node), indent + INDENT);
}

static void
print_VECTOR_CST (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  tree n, type = TREE_TYPE (node); 
  tree t1 = NULL;
  int  i, ok, size = 0;
  int ok2 = 0;
  char *fmt = (char *)"this is just to stop compiler warning";
  
  union {
      unsigned char  uc[16];
      unsigned short us[8];
      unsigned long  ul[4];
  } vec_value;
  
  print_tree_flags (file, node);

  ok = (type && TREE_CODE (type) == VECTOR_TYPE);
  if (ok)
    {
      type = TREE_TYPE (type);
      n    = TYPE_SIZE_UNIT (type);
      size = CST_VALUE (n, ok);
      t1 = TREE_VECTOR_CST_ELTS (node);
     
      if (TREE_CODE (type) == INTEGER_TYPE
	  && (size == 1 || size == 2 || size == 4))
	{
	  fmt = (char *) (TYPE_UNSIGNED (type) ? "%u%s" : "%d%s");
	  if (TREE_CODE (TREE_VALUE (t1)) == INTEGER_CST)
	    {
	      vec_value.ul[0] = CST_VALUE (TREE_VALUE (t1), ok);
	      vec_value.ul[1] = CST_VALUE (TREE_VALUE (TREE_CHAIN (t1)), ok);
	      vec_value.ul[2] = CST_VALUE (TREE_VALUE (TREE_CHAIN (TREE_CHAIN (t1))), ok);
	      vec_value.ul[3] = CST_VALUE (TREE_VALUE (TREE_CHAIN (TREE_CHAIN (TREE_CHAIN (t1)))), ok);
	    }
	  ok2 = ok;
	}
      else if (TREE_CODE (type) != REAL_TYPE
	       || TREE_CODE (TREE_VALUE (t1)) != REAL_CST
	       || size != 4)
	ok2 = 0;
      else
	ok2 = ok;
    }
  
  if (ok2)
    {
      fprintf (file, " ");
      fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE(TREE_VALUE (t1)));
      fprintf (file, " ");
      fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE(TREE_VALUE (TREE_CHAIN (t1))));
      fprintf (file, " ");
      fprintf (file, HOST_PTR_PRINTF,
			HOST_PTR_PRINTF_VALUE(TREE_VALUE (TREE_CHAIN (TREE_CHAIN (t1)))));
      fprintf (file, " ");
      fprintf (file, HOST_PTR_PRINTF,
      			HOST_PTR_PRINTF_VALUE(TREE_VALUE (TREE_CHAIN (TREE_CHAIN (TREE_CHAIN (t1))))));
      newline_and_indent (file, indent + INDENT);
      fputc ('(', file);
      
      switch (size)
	{
	case 1:
	  for (i = 0; i < 16; ++i)
	    fprintf (file, fmt, vec_value.uc[i], (i < 15) ? "," : "");
	  break;
	  
	case 2:
	  for (i = 0; i < 8; ++i)
	    fprintf (file, fmt, vec_value.us[i], (i < 7) ? "," : "");
	  break;
	  
	case 4:
	  if (TREE_CODE (type) != REAL_TYPE)
	    for (i = 0; i < 4; ++i)
	      fprintf (file, fmt, vec_value.ul[i], (i < 3) ? "," : "");
	  else
	    {
	      print_real_constant (file, TREE_VALUE (t1));
	      fputc (',', file);
	      print_real_constant (file, TREE_VALUE (TREE_CHAIN (t1)));
	      fputc (',', file);
	      print_real_constant (file, TREE_VALUE (TREE_CHAIN (TREE_CHAIN (t1))));
	      fputc (',', file);
	      print_real_constant (file, TREE_VALUE (TREE_CHAIN (TREE_CHAIN (TREE_CHAIN (t1)))));
	    }
	  break;
	}
	
      fputc (')', file);
    }
  else
    {
      dump_tree (file, NULL, TREE_VALUE (t1), indent + INDENT);
      dump_tree (file, NULL, TREE_VALUE (TREE_CHAIN (t1)), indent + INDENT);
      dump_tree (file, NULL, TREE_VALUE (TREE_CHAIN (TREE_CHAIN (t1))), indent + INDENT);
      dump_tree (file, NULL, TREE_VALUE (TREE_CHAIN (TREE_CHAIN (TREE_CHAIN (t1)))), indent + INDENT);
    }
}

static void
print_STRING_CST (file, annotation, node, indent)
     FILE *file ATTRIBUTE_UNUSED;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node ATTRIBUTE_UNUSED;
     int indent ATTRIBUTE_UNUSED;
{
  print_tree_flags (file, node);
  fprintf (file, " ptr=");
  fprintf (file, HOST_PTR_PRINTF,
  		HOST_PTR_PRINTF_VALUE (TREE_STRING_POINTER (node)));
  fprintf (file, " ");
  print_string_constant (file, (char *)TREE_STRING_POINTER (node), 30);
  
  if (TREE_CHAIN (node))
    {
      fprintf (file, " chain=");
      fprintf (file, HOST_PTR_PRINTF, 
      			HOST_PTR_PRINTF_VALUE (TREE_CHAIN (node)));
  
      for (node = TREE_CHAIN (node); node; node = TREE_CHAIN (node))
    	dump_tree (file, NULL, node, indent + INDENT);
    }
}

static void
print_FUNCTION_DECL (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  tree n;
  
  if (DECL_BUILT_IN (node))
    {
      if (DECL_BUILT_IN_CLASS (node) == BUILT_IN_MD)
	fprintf (file, " BUILT_IN_MD %d", DECL_FUNCTION_CODE (node));
      else
	fprintf (file, " %s",
		 built_in_class_names[(int) DECL_BUILT_IN_CLASS (node)]);
    }
    
  if (DECL_BUILT_IN_NONANSI (node))
    fputs (" builtin-nonansi", file);
  
  if (dump_tree_state.doing_call_expr)
    {
      print_decl (file, annotation, node, indent);
      return;
    }
    
  if (DECL_EXTERNAL (node))
    fputs (" ext", file);
  if (TREE_PUBLIC (node))
    fputs (" pub", file);
  if (TREE_PRIVATE (node))
    fputs (" pvt", file);
  if (TREE_PROTECTED (node))
    fputs (" prot", file);
#if 0
  if (TREE_STATIC (node))
    fputs (" def", file);
  if (TREE_ADDRESSABLE (node))
    fputs (" addrsable", file);
#endif
  if (TREE_ASM_WRITTEN (node))
    fputs (" asm-wrtn", file);
  if (TREE_NOTHROW (node))
    fputs (" nothr", file);

  if (DECL_INLINE (node))
    fputs (" inline", file);
  if (DECL_NO_STATIC_CHAIN (node))
    fputs (" no-static-chain", file);
  if (DECL_VIRTUAL_P (node))
    fputs (" virt", file);
  if (DECL_STATIC_CONSTRUCTOR (node))
    fputs (" static-ctor", file);
  if (DECL_STATIC_DESTRUCTOR (node))
    fputs (" static-dtor", file);
  
  if (DECL_INITIAL (node))
    {
      fprintf (file, " initial=");
      fprintf (file, HOST_PTR_PRINTF,	
			  HOST_PTR_PRINTF_VALUE (DECL_INITIAL (node)));
    }
    
  if (DECL_SAVED_INSNS (node))
    {
      fprintf (file, " saved-insns=");
      fprintf (file, HOST_PTR_PRINTF,
    	     HOST_PTR_PRINTF_VALUE (DECL_SAVED_INSNS (node)));
    }

  if (!DECL_EXTERNAL (node))
    {
      fprintf (file, " ");
      fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE (DECL_RESULT (node)));
      fprintf (file, "(");
      fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE (DECL_ARGUMENTS (node)));
      fprintf (file, "){");
      fprintf (file, HOST_PTR_PRINTF,
		 HOST_PTR_PRINTF_VALUE (DECL_LANG_SPECIFIC(node)
		 			   ? DECL_SAVED_TREE (node) : 0));
      fprintf (file, "}");
    }    
  print_decl (file, annotation, node, indent);
  
  /*if (DECL_LANG_SPECIFIC(node) && DECL_SAVED_TREE (node)) ?? */
  if (DECL_RESULT (node))
    dump_tree (file, NULL, DECL_RESULT (node), indent + INDENT);
  
  dump_tree_state.doing_parm_decl = 1;
  for (n = DECL_ARGUMENTS (node); n; n = TREE_CHAIN (n))
    dump_tree (file, NULL, n, indent + INDENT);
  dump_tree_state.doing_parm_decl = 0;
  
  if (DECL_LANG_SPECIFIC(node))    /* saftey test, used by DECL_SAVED_TREE */
    for (n = DECL_SAVED_TREE (node); n; n = TREE_CHAIN (n))
      {
	if (n == DECL_SAVED_TREE (node)
	    && TREE_CODE (n) == EXPR_STMT
	    && EXPR_STMT_EXPR (n) == void_zero_node)
	  {
	    dump_tree_state.really_follow = 0;
	    dump_tree (file, "(dummy, to be deleted)", n, indent + INDENT);
	    dump_tree_state.really_follow = 1;
	  }
	else
	  dump_tree (file, NULL, n, indent + INDENT);
      }
      
   /* FIXME: DECL_VINDEX ?? */
}

static void
print_LABEL_DECL (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  if (TREE_ADDRESSABLE (node))
    fputs (" seen", file);
  if (DECL_TOO_LATE (node))
    fputs (" too-late", file);
  if (DECL_ERROR_ISSUED (node))
    fputs (" error-issued", file);
  print_decl (file, annotation, node, indent);
}

static void
print_CONST_DECL (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  print_decl (file, annotation, node, indent);
}

static void
print_TYPE_DECL (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{  
  if (DECL_ORIGINAL_TYPE (node))
    {
      fprintf (file, " orig_type=");
      fprintf (file, HOST_PTR_PRINTF,
    			HOST_PTR_PRINTF_VALUE (DECL_ORIGINAL_TYPE (node)));
    }
  if (TYPE_DECL_SUPPRESS_DEBUG (node))
    fputs (" suppress-debug", file);
  print_decl (file, annotation, node, indent);
}

static void
print_VAR_DECL (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  if (node_seen (node, FALSE))
    {
      print_decl (file, annotation, node, indent);
      return;
    }
    
  if (TREE_CODE (node) == VAR_DECL && TREE_STATIC (node))
    fputs (" static", file);
  if (TREE_PUBLIC (node))
    fputs (" pub", file);
  if (DECL_EXTERNAL (node))
    fputs (" ext", file);
  if (DECL_REGISTER (node))
   fputs (" regdcl", file);
  if (TREE_CODE (node) == VAR_DECL && DECL_IN_TEXT_SECTION (node))
    fputs (" in-txt-sect", file);
  if (DECL_VIRTUAL_P (node))
    fputs (" virt", file);
  if (DECL_POINTER_ALIAS_SET_KNOWN_P (node))
    {
      fputs (" alias-set=", file);
      fprintf (file, HOST_WIDE_INT_PRINT_DEC, 
	       		DECL_POINTER_ALIAS_SET (node));
    }
  
  if (TREE_CODE (node) == PARM_DECL && DECL_TRANSPARENT_UNION (node))
    fputs (" transp-union", file);
 
  if (TREE_CODE (node) != PARM_DECL)
    {
      fprintf (file, " initial=");
      fprintf (file, HOST_PTR_PRINTF,
      				HOST_PTR_PRINTF_VALUE (DECL_INITIAL (node)));
      print_decl (file, annotation, node, indent);
      dump_tree (file, NULL, DECL_INITIAL (node), indent + INDENT);
    }
  else
    print_decl (file, annotation, node, indent);
}

static void
print_PARM_DECL (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  if (dump_tree_state.doing_parm_decl)
    {
      if (DECL_ARG_TYPE (node))
        {
	  fprintf (file, " arg-type=");
	  fprintf (file, HOST_PTR_PRINTF,
		HOST_PTR_PRINTF_VALUE (DECL_ARG_TYPE (node)));
	}
      if (DECL_ARG_TYPE_AS_WRITTEN (node))
	{
	  fprintf (file, " as-written=");
	  fprintf (file, HOST_PTR_PRINTF,
		HOST_PTR_PRINTF_VALUE (DECL_ARG_TYPE_AS_WRITTEN (node)));
	}
      if (DECL_INCOMING_RTL (node))
	{
      	  fprintf (file, " incoming-rtl=");
	  fprintf (file, HOST_PTR_PRINTF,
      		HOST_PTR_PRINTF_VALUE (DECL_INCOMING_RTL (node)));
	}
    }
  print_VAR_DECL (file, annotation, node, indent);
}

static void
print_RESULT_DECL (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  print_decl (file, annotation, node, indent);
}

static void
print_FIELD_DECL (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  if (node_seen (node, FALSE))
    {
      print_decl (file, annotation, node, indent);
      return;
    }
    
  if (TREE_ADDRESSABLE (node))
    fputs (" addressable", file);
  if (TREE_READONLY (node))
    fputs (" readonly", file);
  
  if (DECL_VIRTUAL_P (node))
    fputs (" virt", file);
  if (DECL_PACKED (node))
    fputs (" packed", file);
  if (DECL_UNSIGNED (node))
    fputs (" uns", file);
  if (DECL_BIT_FIELD (node))
    fputs (" bitfield", file);
  if (DECL_NONADDRESSABLE_P (node))
    fputs (" nonaddr", file);
  fprintf (file, " off-align=");
  fprintf (file, HOST_WIDE_INT_PRINT_UNSIGNED,
	   DECL_OFFSET_ALIGN (node));
  print_decl (file, annotation, node, indent);
}

static void
print_NAMESPACE_DECL (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  print_decl (file, annotation, node, indent);
}

static void
print_COMPONENT_REF (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  print_ref (file, annotation, node, indent);
  print_operands (file, node, indent, TRUE, "(struct/union)", "(field)", NULL);
}

static void
print_BIT_FIELD_REF (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  print_ref (file, annotation, node, indent);
  print_operands (file, node, indent, TRUE, "(struct/union)", "(#bits)", "(pos)", NULL);
}

static void
print_INDIRECT_REF (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  print_ref (file, annotation, node, indent);
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_BUFFER_REF (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  print_ref (file, annotation, node, indent);
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_ARRAY_REF (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  print_ref (file, annotation, node, indent);
  print_operands (file, node, indent, TRUE, "(base)", "(index)", NULL);
}

static void
print_ARRAY_RANGE_REF (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  print_ref (file, annotation, node, indent);
  print_operands (file, node, indent, TRUE, "(base)", "(index)", NULL);
}

static void
print_VTABLE_REF (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  print_ref (file, annotation, node, indent);
  print_operands (file, node, indent, TRUE, "(base)", "(index)", NULL);
}

static void
print_CONSTRUCTOR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  if (TREE_ADDRESSABLE (node))
    fputs (" must-be-in-mem", file);
  if (TREE_STATIC (node))
    fputs (" static", file);
  if (TREE_CONSTANT (node))
    fputs (" const", file);
  if (TREE_BOUNDED (node))
    fputs (" bounded", file);
  
  print_operands (file, node, indent, FALSE, NULL);
  
  for (node = CONSTRUCTOR_ELTS (node); node; node = TREE_CHAIN (node))
    dump_tree (file, NULL, node, indent + INDENT);
}

static void
print_COMPOUND_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  if (TREE_NO_UNUSED_WARNING (node))
    fputs (" no-unused", file);
  
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_MODIFY_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_INIT_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_TARGET_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, "(target)", "(init)",
  			"(cleanup)", "(saved-init)", NULL);
}

static void
print_COND_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, "(cond)", "(?)", "(:)", NULL);
}

static void
print_BIND_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, "(vars)", "(body)", "(block)", NULL);
}

static void
print_CALL_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  if (TREE_NOTHROW (node))
    fputs (" nothrow", file);
  
  dump_tree_state.doing_call_expr = 1;
  print_operands (file, node, indent, TRUE, "(func)", "(args)", NULL);
  dump_tree_state.doing_call_expr = 0;
}

static void
print_METHOD_CALL_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, "(self)", "(args)", NULL);
}

static void
print_WITH_CLEANUP_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_CLEANUP_POINT_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_PLACEHOLDER_EXPR (file, annotation, node, indent)
     FILE *file ATTRIBUTE_UNUSED;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node ATTRIBUTE_UNUSED;
     int indent ATTRIBUTE_UNUSED;
{
  /*print_operands (file, node, indent, FALSE, NULL);*/
  /* this is an 'x' node, not an expr node */
}

static void
print_WITH_RECORD_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_PLUS_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_MINUS_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_MULT_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_TRUNC_DIV_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_CEIL_DIV_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_FLOOR_DIV_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_ROUND_DIV_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_TRUNC_MOD_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_CEIL_MOD_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_FLOOR_MOD_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_ROUND_MOD_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_RDIV_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_EXACT_DIV_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_FIX_TRUNC_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_FIX_CEIL_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_FIX_FLOOR_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_FIX_ROUND_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_FLOAT_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_NEGATE_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_MIN_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_MAX_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_ABS_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_FFS_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_LSHIFT_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_RSHIFT_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_LROTATE_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_RROTATE_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_BIT_IOR_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_BIT_XOR_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_BIT_AND_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_BIT_ANDTC_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_BIT_NOT_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_TRUTH_ANDIF_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_TRUTH_ORIF_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_TRUTH_AND_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_TRUTH_OR_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_TRUTH_XOR_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_TRUTH_NOT_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_LT_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_LE_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_GT_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_GE_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_EQ_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_NE_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_UNORDERED_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_ORDERED_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_UNLT_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_UNLE_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_UNGT_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_UNGE_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_UNEQ_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_IN_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_SET_LE_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_CARD_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_RANGE_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_CONVERT_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_NOP_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_NON_LVALUE_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_SAVE_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_VIEW_CONVERT_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_UNSAVE_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_RTL_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_ADDR_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_REFERENCE_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_ENTRY_VALUE_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_FDESC_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_COMPLEX_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_CONJ_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_REALPART_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_IMAGPART_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_PREDECREMENT_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_PREINCREMENT_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_POSTDECREMENT_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_POSTINCREMENT_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_VA_ARG_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_TRY_CATCH_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, "(try)", "(catch)", NULL);
}

static void
print_TRY_FINALLY_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_GOTO_SUBROUTINE_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_LABEL_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_GOTO_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_RETURN_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_EXIT_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_LOOP_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_LABELED_BLOCK_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_EXIT_BLOCK_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_EXPR_WITH_FILE_LOCATION (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_SWITCH_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_EXC_PTR_EXPR (file, annotation, node, indent)
     FILE *file;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node;
     int indent;
{
  print_operands (file, node, indent, TRUE, NULL);
}

static void
print_CLZ_EXPR (file, annotation, node, indent)
     FILE *file ATTRIBUTE_UNUSED;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node ATTRIBUTE_UNUSED;
     int indent ATTRIBUTE_UNUSED;
{
  /* TO DO */
}
static void
print_CTZ_EXPR (file, annotation, node, indent)
     FILE *file ATTRIBUTE_UNUSED;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node ATTRIBUTE_UNUSED;
     int indent ATTRIBUTE_UNUSED;
{
  /* TO DO */
}
static void
print_PARITY_EXPR (file, annotation, node, indent)
     FILE *file ATTRIBUTE_UNUSED;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node ATTRIBUTE_UNUSED;
     int indent ATTRIBUTE_UNUSED;
{
  /* TO DO */
}
static void
print_POPCOUNT_EXPR (file, annotation, node, indent)
     FILE *file ATTRIBUTE_UNUSED;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node ATTRIBUTE_UNUSED;
     int indent ATTRIBUTE_UNUSED;
{
  /* TO DO */
}

/*-------------------------------------------------------------------*/

/* Alaways the last lang_dump_tree_p to keep lang_dump_tree_p from being
   NULL.  A return of 0 always forces the tree node switch to go to its
   default case.  */
   
static int 
no_dump_tree_p (file, annotation, node, indent)
     FILE *file ATTRIBUTE_UNUSED;
     const char *annotation ATTRIBUTE_UNUSED;
     tree node ATTRIBUTE_UNUSED;
     int indent ATTRIBUTE_UNUSED;
{
  return 0;
}

/* Language-specific initializers call this to add to the chain of language-
   specific tree node dumpers.  It is expected that their default node case
   will call the function returned from here which was saved during their
   initialization.  */
   
lang_dump_tree_p_t
set_dump_tree_p (new_lang_dump_tree_p)
     lang_dump_tree_p_t new_lang_dump_tree_p;
{
   lang_dump_tree_p_t old_lang_dump_tree_p = lang_dump_tree_p;
   lang_dump_tree_p = new_lang_dump_tree_p;
   return old_lang_dump_tree_p;
}

/* Annotations enclosed in parentheses are appended to the initial
   kind:address display.  Otherwise they prefix it.  Note that a
   prefix annotation is expected to have at least a trailing
   space but a parenthesized annotation should end with it's
   delimiting right parentheses.  */
   
void 
dump_tree (file, annotation, node, indent)
     FILE *file;
     const char *annotation;
     tree node;
     int indent;
{
  int good_node, no_new_line, plen = 0;
  static tree previous_node;
  
  no_new_line = dump_tree_state.no_new_line;
  dump_tree_state.no_new_line = 0;
  
  /* prefix_len contains the length of the fixed part of a tree node
     display line and can be used by tree node dumpers to control
     line wrapping if the line info for that node gets too long.  */
     
  if (node == NULL)
    return;
    
  if (dump_tree_state.nesting_depth < 0)
    {
      /* If not called from dmp_tree3() then create hash table to record
         which nodes we visit.  We still also do this for dmp_tree3()
         but only the first time dmp_tree3() calls this routine.  After
         that it's dmp_tree3() responsibility to free the hash table.  */
      if (dump_tree_state.visit_only_once != DMP_TREE_VISIT_ONCE2)
        {
      	  init_dump_state(node);
      	  table = (struct bucket **) xmalloc (HASH_SIZE * sizeof (struct bucket *));
      	  memset ((char *) table, 0, HASH_SIZE * sizeof (struct bucket *));
      	  if (dump_tree_state.visit_only_once == DMP_TREE_VISIT_ONCE1)
      	    dump_tree_state.visit_only_once = DMP_TREE_VISIT_ONCE2;
      	}
      
      indent = 0;
      previous_node = NULL_TREE;
#if DMP_TREE_WRAPPED_OUTPUT
      {
        char *wrap_str = getenv(DMP_TREE_LINE_WRAP);
	line_index = prev_line_was_null = prev_line_was_wrapped = 0;
	wrap_column = 0;
	if (wrap_str)
	  wrap_column = strtol (wrap_str, NULL, 10);
#if defined(TIOCGSIZE) || defined(TIOCGWINSZ)
	if (wrap_column < MIN_LINE_WRAP || wrap_column > MAX_LINE_WRAP)
	  {
#ifdef TIOCGWINSZ
	    wrap_column = ioctl (fileno(file), TIOCGWINSZ, &winsize);
	    if (wrap_column >= 0)
	      wrap_column = winsize.ws_col;
#endif
#if defined(TIOCGSIZE) && !defined(TIOCGWINSZ)
	    wrap_column = ioctl (fileno(file), TIOCGSIZE, &winsize);
	    if (wrap_column >= 0)
	      wrap_column = winsize.ts_cols;
#endif
          }
      }
#endif /* TIOCGSIZE || TIOCGWINSZ */
      if (wrap_column < MIN_LINE_WRAP || wrap_column > MAX_LINE_WRAP)
	wrap_column = DEFAULT_LINE_WRAP;
#endif /* DMP_TREE_WRAPPED_OUTPUT */
    }
  
  if (dump_tree_state.visit_only_once != DMP_TREE_VISIT_ANY
      && node_seen (node, TRUE))
    return;
  
  if (dump_tree_state.dump_full_type && node_seen (node, FALSE))
    return;
    
  if (dump_tree_state.doing_tree_list && node_seen (node, FALSE))
    return;
    
  if (dump_tree_state.nesting_depth >= dump_tree_state.max_depth)
    return;
  
  if (dump_tree_state.nesting_depth >= 0)
    {
      if (!no_new_line)
        {
	  newline_and_indent (file, 0);
	  if (statement_code_p (TREE_CODE (node))
	      || (TREE_CODE (node) == TREE_VEC && TREE_VEC_LENGTH (node) > 1))
	    {
	      if (previous_node
		  && TREE_CODE (previous_node) != TREE_CODE (node)
		  && (*lang_hooks.dump_blank_line_p) (previous_node, node))
		newline_and_indent (file, 0);
	    }
      	}
      previous_node = node;
      if (indent > 0)
        fprintf (file, "%*c", indent, ' ');
    }
  
  ++dump_tree_state.nesting_depth;
  
  plen = indent;
  if (annotation && *annotation && *annotation != '(')
    plen += fprintf (file, "%s", annotation);

  good_node = ((int) TREE_CODE (node) < dump_tree_state.max_code);
      
  if (good_node)
    {
      plen += fprintf (file, "%s:", tree_code_name[(int) TREE_CODE (node)]);
      plen += fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE (node));
    }
  else
    {
      plen += fprintf (file, "%d (?):", (int) TREE_CODE (node));
      plen += fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE (node));
    }

  if (annotation && *annotation == '(')
    plen += fprintf (file, " %s", annotation);
    
  if (good_node)
    {
      if (TREE_CODE_CLASS (TREE_CODE (node)) != 't'
	  && TREE_TYPE (node)
	  && TREE_CODE_CLASS (TREE_CODE (TREE_TYPE (node))) == 't')
	{
	  tree type, type0 = TREE_TYPE (node);
	  
	  if (dump_tree_state.doing_call_expr
	      && TREE_CODE (node) == FUNCTION_DECL && TREE_TYPE (type0))
	    type = TREE_TYPE (type0);
	  else
	    type = type0;
	    
	  fprintf (file, " t=");
	  fprintf (file, HOST_PTR_PRINTF, HOST_PTR_PRINTF_VALUE (type0));
	  
	  if (TYPE_NAME (type))
	    {
	      if (TREE_CODE (TYPE_NAME (type)) == IDENTIFIER_NODE)
	        {
	          if (IDENTIFIER_POINTER (TYPE_NAME (type))
	              && *IDENTIFIER_POINTER (TYPE_NAME (type)))
		  fprintf (file, " {%s}", IDENTIFIER_POINTER (TYPE_NAME (type)));
		}
	      else if (TREE_CODE (TYPE_NAME (type)) == TYPE_DECL
		       && DECL_NAME (TYPE_NAME (type))
		       && IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (type)))
		       && *IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (type))))
		fprintf (file, " {%s}",
			 IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (type))));
	    }
	  else
	    fprintf (file, " {%s}", tree_code_name[(int) TREE_CODE (type)]);
	   
	  /* If doing a full program dump we also want to dump the types too.
	     So dmp_tree3() will do it after we dump all the decls's.  All
	     we do here is record all the decl's types in an array.  We won't
	     worry about dups here.  That is taken care of when we process
	     this array.  */
	     
	  if (dump_tree_state.visit_only_once == DMP_TREE_VISIT_ONCE2
	      && !node_seen (type, FALSE))
	    {
	      if (type_array_next >= type_array_size)
		{
		  type_array_size += type_array_incr;
		  type_array_incr = TYPE_ARRAY_INCR;
		  type_array = (tree *) xrealloc (type_array,
		  				  sizeof (tree) * type_array_size);
		}
  
  		type_array[type_array_next++] = type;
	    }
	}
      else if (dump_tree_state.dump_full_type && TREE_TYPE (node))
	{
	  fprintf (file, " t=");
	  fprintf (file, HOST_PTR_PRINTF,
		   HOST_PTR_PRINTF_VALUE (TREE_TYPE (node)));
	}
    }
  
#if DMP_TREE_WRAPPED_OUTPUT
  prefix_len = plen + 1;  	/* include blank following prefix */
  prev_line_was_wrapped = 0;
#endif

  print_lineno (file, node);	 /* line nbr info where possible */
  
  if (! (*lang_dump_tree_p) (file, annotation, node, indent))
    {
      switch (TREE_CODE (node)) 
        {
#         define DEFTREECODE(SYM, NAME, TYPE, LENGTH) \
        	case SYM: print_ ## SYM (file, annotation, node, indent); break;
#         include "tree.def"
#         undef DEFTREECODE
        default:
          print_ERROR_MARK (file, annotation, node, indent);
          break;
        }
    }
 
  --dump_tree_state.nesting_depth;
    
  if (dump_tree_state.nesting_depth < 0)
    {
      newline_and_indent (file, 0);
      if (dump_tree_state.visit_only_once != DMP_TREE_VISIT_ONCE2)
      	free_hash_table ();
    }
  else
    (void)node_seen (node, TRUE);
    
}

/* Called from debugger to dump the tree for a specific node.  */
void 
dmp_tree (node)
     tree node;
{
  dump_tree_state.max_depth = INT_MAX;
  dump_tree_state.visit_only_once = DMP_TREE_VISIT_ANY;
  dump_tree (stderr, NULL, node, 0);
}

/* Same as dmp_tree() but limit the nesting to specified depth.  */
void 
dmp_tree1 (node, max_depth)
     tree node;
     int max_depth;
{
  if (max_depth <= 0)
    max_depth = 1;
  dump_tree_state.max_depth = max_depth;
  dump_tree_state.visit_only_once = DMP_TREE_VISIT_ANY;
  dump_tree (stderr, NULL, node, 0);
}

/* Same as dmp_tree() but displays never show a referenced node
   more than once.  */
void 
dmp_tree2 (node)
     tree node;
{
  dump_tree_state.max_depth = INT_MAX;
  dump_tree_state.visit_only_once = DMP_TREE_VISIT_ONCE;
  dump_tree (stderr, NULL, node, 0);
}

/* Called only from tree-dump.c to handle our dmp_tree() types of
   displays when dumping an entire program to a file by specifying
   -fdmp-translation-unit.  */
void 
dmp_tree3 (file, node, flags)
     FILE *file;
     tree node;
     int flags ATTRIBUTE_UNUSED;
{
  int i;
  
  dump_tree_state.max_depth = INT_MAX;
  dump_tree_state.visit_only_once = DMP_TREE_VISIT_ONCE1;
  
  while (node)
    {
      dump_tree (file, NULL, node, 0);
      node = TREE_CHAIN (node);
    }
    
  newline_and_indent (file, 0);
  
  if (type_array_next > 0)
    {
      for (i = 0; i < type_array_next;  ++i)
	dump_tree (file, NULL, type_array[i], 0);
	
      free (type_array);
    }
    
  free_hash_table ();
}

/*-------------------------------------------------------------------*/

#if DMP_TREE_WRAPPED_OUTPUT

/* The three routines below here are what's actually called when fprintf,
   fputc, or fputs are used in the tree dumper.  These are used to
   intercept the output to impose an appropriate line wrapping convention
   on nodes that get too long for the display (lines > wrap_column).  The
   stdio.h names are redefined by macros to call these routines when 
   DMP_TREE_WRAPPED_OUTPUT is set.
   
   The line wrap convention is to wrap only on blanks between the node's
   info.  Blanks within bracketed or quoted info does not count.  The
   wrapped portion is indented to start wherever the node's indented
   kind:address portion of the display ends.  */
   
#undef fprintf
#undef fputc
#undef fputs

int 
dmp_tree_fprintf VPARAMS ((FILE *file, const char *fmt, ...))
{
  int len;
  va_list ap;
  
#ifndef ANSI_PROTOTYPES
  FILE *file;
  char *fmt;
#endif
  
  VA_START (ap, fmt);
#ifndef ANSI_PROTOTYPES
  file = va_arg (ap, FILE *);
  fmt  = va_arg (ap, char *);
#endif
  
  len = vsprintf (&curr_line[line_index], fmt, ap);
  line_index += len;
  
  va_end (ap);
  
  if (curr_line[line_index-1] == '\n')
    {
      if (line_index != 1 || !(prev_line_was_wrapped || prev_line_was_null))
        {
      	  curr_line[line_index] = '\0';
      	  fputs (curr_line, file);
      	  fflush (file);
        }
      prev_line_was_null = (line_index == 1);
      line_index = 0;
    }
  else if (line_index >= wrap_column)
    {
      char c, unwritten_part[MAX_LINE_WRAP+2];
      int  i, end, delimiter, nesting;
      
      prev_line_was_wrapped = 0;
      
      /* Limit how far to the left we'll search to about wrap_column/2 */
      end = wrap_column/2;
      if (end < prefix_len)
        end = prefix_len;
      
      /* Search left the the first blank to the left of the wrap point.
         We assume that bracketed entities (e.g., quoted strings,
         parenthetical entities, etc.) are written with a single
         fprintf() so that we know to ignore blanks embedded within
         pairs of these things.  */
         
      i = line_index - 1;
      delimiter = nesting = 0;
      while (i >= end)
        {
          c = curr_line[i];
	  if (curr_line[i-1] == '\\')
	    --i;
          else if (nesting)
            {
	      if (c == delimiter && --nesting <= 0)
		delimiter = 0;
            }
          else if (c == '"' || c == '\'') {
            delimiter = c; ++nesting;
	  } else if (c == ')') {
	    delimiter = '('; ++nesting;
	  } else if (c == ']') {
	    delimiter = '['; ++nesting;
	  } else if (c == '>') {
	    delimiter = '<'; ++nesting;
	  } else if (c == '}') {
	    delimiter = '{'; ++nesting;
	  } else if (c == ' ' && i < wrap_column)
	    break;
          --i;
        }
        
      if (i < end)
      	return len;
      
      /* The wrapped (unwritten) part is everthing to the right of the
         blank found above.  */
         
      strcpy (unwritten_part, &curr_line[i+1]);
      
      /* Delete trailing blanks on the left part we are going to print
         and print it... */
         
      while (curr_line[i] == ' ' && i >= end)
        --i;
      if (i < end)
      	return len;
      	
      curr_line[i+1] = '\n';
      curr_line[i+2] = '\0';
      prev_line_was_wrapped = 1;
      fputs (curr_line, file);
      fflush (file);
      
      /* Write the wrapped portion, indented by the current prefix_len... */
      
      line_index = prefix_len;
      memset(curr_line, ' ', line_index);
      curr_line[line_index] = '\0';
      
      dmp_tree_fprintf (file, "%s", unwritten_part);
    }
    
  return len;
}

int 
dmp_tree_fputc (c, file)
     int c;
     FILE *file;
{
  dmp_tree_fprintf (file, "%c", c);
  return c;
}

int 
dmp_tree_fputs (s, file)
     const char *s;
     FILE *file;
{
  return dmp_tree_fprintf (file, "%s", s);
}

#endif /* DMP_TREE_WRAPPED_OUTPUT */

/*-------------------------------------------------------------------*/

/* Special routine called for debugging chains only.  This is NOT a
   tree node type.  */

void print_TREE_CHAIN PARAMS ((tree));

void
print_TREE_CHAIN (node)
     tree node;
{
  while (node)
    {
      dump_tree (stderr, NULL, node, 0); /* keep list at same indent */
      node = TREE_CHAIN (node);
    }
}

