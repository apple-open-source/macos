#include "config.h"
#include "system.h"
#include "tree.h"
#include "rtl.h"
#include "flags.h"
#include "toplev.h"
#include "output.h"
#include "c-tree.h"
#include "c-common.h"
#include "expr.h"
#include "feedback.h"
#include "basic-block.h"
#include "profile.h"
#include "tree-inline.h"
#include "gcov-io.h"
#include "ggc.h"
#include "langhooks.h"

/** Please increment the magic number in feedback.c and 
    (2 places) in libgcc2.c whenever a substantiative change
    is made to this size computation. ***/

unsigned int
c_find_control_flow_tree (t)
    tree t;
{
  unsigned int size = 0;
  switch (TREE_CODE (t))
    {
      case IF_STMT:
        size = 4;
        break;
      case FOR_STMT:
        size = 5;
	break;
      case WHILE_STMT:
      case DO_STMT:
	size = 3;
	break;
      case CONTINUE_STMT:
      case BREAK_STMT:
      case GOTO_STMT:
      case RETURN_STMT:
	size = 1;
	break;
      case SWITCH_STMT:
	/* For tablejumps, there is one arc for switch entry,
	   one between the out-of-range check and the tablejump,
	   and one per case in the table, plus one more for the
	   default when reached from the table.  The default when
	   reached from the out-of-range check is not counted
	   directly; it can be computed by subtracting the first
	   two counts.  */
	/* For switch by tree of compares, cases towards the center
	   of the tree generate 2 branches, leaves may generate 0 or 1.
	   2 per case is a safe max; I'm sure a smaller limit could
	   be derived. */
	if (TREE_CODE (SWITCH_BODY (t)) != COMPOUND_STMT)
	  /* no case labels; one branch past the body is genned */
	  size = 1;
	else if (COMPOUND_BODY (SWITCH_BODY (t)) == NULL_TREE)
	  /* empty body */
	  size = 1;
	else if (TREE_CODE (COMPOUND_BODY (SWITCH_BODY (t))) == SCOPE_STMT
	    && SCOPE_BEGIN_P (COMPOUND_BODY (SWITCH_BODY (t))))
	  {
	    tree tt = TREE_CHAIN (COMPOUND_BODY (SWITCH_BODY (t)));
	    /* Incorrect input might get this to loop or something;
	       for this purpose I don't care. */
	    int nesting = 1;
	    int default_found = 0;
	    size = 2;
	    for (; tt && nesting; tt = TREE_CHAIN (tt))
	      {
		if (TREE_CODE (tt) == SCOPE_STMT)
		  {
		    if (SCOPE_BEGIN_P (tt))
		      nesting++;
		    else if (SCOPE_END_P (tt))
		      nesting--;
		  }
		else if (TREE_CODE (tt) == CASE_LABEL && nesting == 1)
		  {
		    size += 2;
		    if (CASE_LOW (tt) == NULL_TREE) /* default: */
		      default_found = 1;
		  }
	      }
	    if (!default_found)
	      size += 2;
	  }
	break;
      default:	/* non-control-flow stmts, constants, etc. */
	break;
    }
  return size;
}

tree
c_create_feedback_counter_tree (table, ix)
     tree table;
     int ix;
{
  tree t;
  /* This produces a[i] = a[i] + 1 */
  /* Generating postincrement requires flushing the RTL queue at the right
     place later; this form is emitted immediately. */
  t = build_modify_expr (
	       build_array_ref (table, 
		  build_int_2 (ix, 0)),
	       NOP_EXPR,
	       build_binary_op (PLUS_EXPR,
		   build_array_ref (table,
		      build_int_2 (ix, 0)),
		   build_int_2 (1, 0), 0));
  return t;
}
void c_decorate_for_feedback (fndecl, find_call)
     tree fndecl;
     walk_tree_fn find_call;
{
  walk_tree_without_duplicates (&DECL_SAVED_TREE (fndecl), 
		  find_call, (void *)0);
}