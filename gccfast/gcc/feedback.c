/* APPLE LOCAL begin this entire file */
/* Handle the tree-based feedback info.
   Copyright (C) 2003 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "tree.h"
#include "rtl.h"
#include "flags.h"
#include "toplev.h"
#include "output.h"
#include "expr.h"
#include "feedback.h"
#include "basic-block.h"
#include "profile.h"
#include "tree-inline.h"
#include "gcov-io.h"
#include "ggc.h"
#include "langhooks.h"
#include "hashtab.h"

static tree feedback_counts_table;
static tree feedback_call_counts_table;
/* These relate the tree to the call and arc counts.  Each hash
   table entry represents a mapping from a tree to an index in
   *_to_counter_table, the first of possibly several for that
   tree.  The map entries are created during a tree walk before
   inlining, which is expected to visit the same trees (relatively
   speaking) in the same order in the creation and use passes.
   The entries in the *_to_counter_table's point to actual counters;
   some entries are -1 meaning there is no counter.  Multiple trees 
   can point to the same map table entry; this happens during RTL
   generation when one tree is replaced by another, or when a
   control-flow operation occurs within a placeholder.  The inliner
   can create new map entries when it clones function bodies; in
   this case a new counter is created also. */
struct map GTY (()) { tree t; HOST_WIDE_INT i; };
static GTY ((param_is (struct map))) htab_t control_flow_table;
static GTY ((param_is (struct map))) htab_t call_table;
static GTY (()) varray_type control_flow_to_counter_table;
static GTY (()) varray_type call_to_counter_table;
/* Indices to increment through read-in data in use_feedback pass. */
static unsigned int call_counter = 0;
static unsigned int control_flow_counter = 0;
/* Counts themselves, use_feedback pass only. */
static GTY (()) varray_type control_flow_counts;
static GTY (()) varray_type call_counts;

static rtx create_feedback_counter_rtx PARAMS ((tree, int *, int *));
static tree find_decoratable_tree PARAMS ((tree *, int *, void *));
static int find_tree_in_table PARAMS ((htab_t, tree));
static hashval_t map_hash PARAMS ((const void *));
static int map_eq PARAMS ((const void*, const void*));

static FILE *db_file = 0;

static hashval_t map_hash (x)
    const void *x;
{
  struct map* p = (struct map*) x;
  return htab_hash_pointer (p->t);
}

static int map_eq (x, y)
    const void *x, *y;
{
  struct map* p = (struct map*) x;
  struct map* q = (struct map*) y;
  return htab_eq_pointer (p->t, q->t);
}

/* Record of the trees we've decorated.  Garbage collection shouldn't
   see or walk over this.  */
static htab_t seen_fndecl;

/* Called once. */
void
init_feedback (filename)
    const char* filename;
{
  if (flag_create_feedback)
    {
      char name[20];
      /* Create table of counts.  We don't know how big it is yet; the size
         will be fixed up later. */
      tree gcov_type_type = make_unsigned_type (GCOV_TYPE_SIZE);
      tree domain_tree
	= build_index_type (build_int_2 (1000, 0)); /* replaced later */
      tree gcov_type_array_type
	= build_array_type (gcov_type_type, domain_tree);

      feedback_counts_table
	= build (VAR_DECL, gcov_type_array_type, NULL_TREE, NULL_TREE);
      TREE_STATIC (feedback_counts_table) = 1;
      ASM_GENERATE_INTERNAL_LABEL (name, "LFDO", 0);
      DECL_NAME (feedback_counts_table) = get_identifier (name);
      DECL_ALIGN (feedback_counts_table) = TYPE_ALIGN (gcov_type_type);

      feedback_call_counts_table
	= build (VAR_DECL, gcov_type_array_type, NULL_TREE, NULL_TREE);
      TREE_STATIC (feedback_call_counts_table) = 1;
      ASM_GENERATE_INTERNAL_LABEL (name, "LFDO", 1);
      DECL_NAME (feedback_call_counts_table) = get_identifier (name);
      DECL_ALIGN (feedback_call_counts_table) = TYPE_ALIGN (gcov_type_type);

      control_flow_table = htab_create_ggc (50, map_hash, map_eq, 0);
      call_table = htab_create_ggc (50, map_hash, map_eq, 0);
      VARRAY_INT_INIT (control_flow_to_counter_table, 50, 
			    "cflowctr table");
      VARRAY_INT_INIT (call_to_counter_table, 50, "callctr table");
    }
  if (flag_use_feedback)
    {
      int len = strlen(filename);
      char *db_file_name = alloca (len + 4);
      strcpy (db_file_name, filename);
      strcat (db_file_name, ".db");
      db_file = fopen (db_file_name, "rb");
      if (!db_file)
	{
	  warning ("file %s not found, execution counts assumed to be zero",
		 db_file_name);
	  /* Prevent future problems.  */
	  flag_use_feedback = 0;
	  return;
	}
      control_flow_table = htab_create_ggc (50, map_hash, map_eq, 0);
      call_table = htab_create_ggc (50, map_hash, map_eq, 0);
      VARRAY_INT_INIT (control_flow_to_counter_table, 50, 
			    "cflowtoctr table");
      VARRAY_INT_INIT (call_to_counter_table, 50, "calltoctr table");
      VARRAY_WIDEST_INT_INIT (control_flow_counts, 50, "cflowctr table");
      VARRAY_WIDEST_INT_INIT (call_counts, 50, "callctr table");

      /* Read in counts. */
      /* These will eventually be related to trees, but the trees
	 don't exist yet. */
      /* There is some vestigial function-level info in the data
	 structures, but it isn't useful; because deferrals may be
	 done differently in the two passes, there's no guarantee
	 functions are compiled in the same order.  They should
	 however be *parsed* in the same order. */
      if (db_file)
	{
	  /* These are more logically unsigned, but __read_xxx deals 
             only in signed objects <sigh> */
	  long ltemp;
	  gcov_type gtemp;
	  long nfuncs;
	  long size_of_header;
	  long narcs, ncalls, narcindices, ncallindices;
	  long j;
	  if (/* magic - must match libgcc2.c */
	      (__read_long (&ltemp, db_file, 4) || ltemp != -457)
	      /* number of functions */
	      || (__read_long (&nfuncs, db_file, 4))
	      /* total number of arcs in file */
	      || (__read_long (&narcs, db_file, 4))
	      /* total number of calls in file */
	      || (__read_long (&ncalls, db_file, 4))
	      /* total number of arc indices in file */
	      || (__read_long (&narcindices, db_file, 4))
	      /* total number of call indices in file */
	      || (__read_long (&ncallindices, db_file, 4))
	      /* we'll use this in a minute */
	      || (__read_long (&size_of_header, db_file, 4))
	      /* skip some uninteresting stuff */
	      || fseek (db_file, 7 * 4 + size_of_header, SEEK_SET))
	    {
	    read_error:;
	      warning ("file %s corrupted, ignored", db_file_name);
	      db_file = 0;
	      return;
	    }
	  for (j = narcs; j > 0; j--)
	    if (__read_gcov_type (&gtemp, db_file, 8))
	      goto read_error;
	    else
	      VARRAY_PUSH_WIDEST_INT (control_flow_counts, gtemp);

	  for (j = ncalls; j > 0; j--)
	    if (__read_gcov_type (&gtemp, db_file, 8))
	      goto read_error;
	    else
	      VARRAY_PUSH_WIDEST_INT (call_counts, gtemp);

	  for (j = narcindices; j > 0; j--)
	    if (__read_long (&ltemp, db_file, 4))
	      goto read_error;
	    else
	      VARRAY_PUSH_INT (control_flow_to_counter_table, ltemp);

	  for (j = ncallindices; j > 0; j--)
	    if (__read_long (&ltemp, db_file, 4))
	      goto read_error;
	    else
	      VARRAY_PUSH_INT (call_to_counter_table, ltemp);
	}
    }
  seen_fndecl = htab_create (131, htab_hash_pointer,
			     htab_eq_pointer, NULL);
}

/* Called once. */
tree
end_feedback ()
{
  if (flag_create_feedback && profile_info.count_instrumented_edges > 0)
    {
      {
	/* fix up size of the feedback_counts_table */
	/* if empty, allocate 4 bytes for convenience; they won't be read */
	tree gcov_type_type = make_unsigned_type (GCOV_TYPE_SIZE);
	tree domain_tree
	  = build_index_type (build_int_2 (
		MAX (0, profile_info.count_instrumented_edges-1), 0));
	tree gcov_type_array_type
	  = build_array_type (gcov_type_type, domain_tree);
	TREE_TYPE (feedback_counts_table) = gcov_type_array_type;

	DECL_SIZE (feedback_counts_table) = TYPE_SIZE (gcov_type_array_type);
	DECL_SIZE_UNIT (feedback_counts_table) 
	    = TYPE_SIZE_UNIT (gcov_type_array_type);

	/* emit feedback_counts_table */
	assemble_variable (feedback_counts_table, 0, 0, 0);
      }
    if (profile_info.count_instrumented_calls > 0)
      {
	/* fix up size of the feedback_call_counts_table */
	tree gcov_type_type = make_unsigned_type (GCOV_TYPE_SIZE);
	tree domain_tree
	  = build_index_type (build_int_2 (
		profile_info.count_instrumented_calls-1, 0));
	tree gcov_type_array_type
	  = build_array_type (gcov_type_type, domain_tree);
	TREE_TYPE (feedback_call_counts_table) = gcov_type_array_type;

	DECL_SIZE (feedback_call_counts_table) = TYPE_SIZE (gcov_type_array_type);
	DECL_SIZE_UNIT (feedback_call_counts_table) 
	    = TYPE_SIZE_UNIT (gcov_type_array_type);

	/* emit feedback_call_counts_table */
	assemble_variable (feedback_call_counts_table, 0, 0, 0);
      }
    }
  if (flag_use_feedback)
    {
      if (db_file)
	fclose (db_file);
    }
  return feedback_counts_table;
}

/* The following two routines must be kept in sync.
   A cleaner way would be to add a HOST_WIDEST_INT variant
   to NOTEs, but we really do *not* want them to get bigger. */
void set_fdo_note_count (note, count)
    rtx note;
    HOST_WIDEST_INT count;
{
#if HOST_BITS_PER_WIDEST_INT == HOST_BITS_PER_WIDE_INT
  NOTE_FDO_COUNT (note) = count;
#else
#if HOST_BITS_PER_WIDEST_INT == 2 * HOST_BITS_PER_WIDE_INT
  union { HOST_WIDE_INT x[2]; HOST_WIDEST_INT y; } u;
  u.y = count;
  /* Names reflect big-endian, but it doesn't matter. */
  NOTE_FDO_COUNT_HIGH (note) = u.x[0];
  NOTE_FDO_COUNT (note) = u.x[1];
#else
  abort ();
#endif
#endif
}

/* Keep in sync with previous routine. */
HOST_WIDEST_INT get_fdo_note_count (note)
      rtx note;
{
  HOST_WIDEST_INT count;
#if HOST_BITS_PER_WIDEST_INT == HOST_BITS_PER_WIDE_INT
  count = NOTE_FDO_COUNT (note);
#else
#if HOST_BITS_PER_WIDEST_INT == 2 * HOST_BITS_PER_WIDE_INT
  union { HOST_WIDE_INT x[2]; HOST_WIDEST_INT y; } u;
  /* Names reflect big-endian, but it doesn't matter. */
  u.x[0] = NOTE_FDO_COUNT_HIGH (note);
  u.x[1] = NOTE_FDO_COUNT (note);
  count = u.y;
#else
  abort ();
#endif
#endif
  return count;
}

static rtx
create_feedback_counter_rtx (table, total_counter, func_counter)
    tree table;
    int *total_counter;
    int *func_counter;
{
  tree t;
  rtx insns_head = NULL_RTX;
  /* This hook should generate a tree which is emitted immediately by
     expand_expr, rather than requiring flushing the queue at the
     right place later.  In C, for example, we use a[i] = a[i] + 1
     instead of a[i]++. */
  t = (*lang_hooks.create_feedback_counter_tree) (table, *total_counter);
  if (t)
    {
      (*total_counter)++;
      (*func_counter)++;
      start_sequence ();
      expand_expr (t, NULL_RTX, VOIDmode, 0);
      insns_head = get_insns ();
      end_sequence ();
    }
  return insns_head;
}

/* Returns -1 if not found, otherwise a nonnegative index. */

static int
find_tree_in_table (table, t)
    htab_t table;
    tree t;
{
  struct map temp;
  struct map* entry;
  temp.t = t;

  entry = (struct map *)htab_find (table, (PTR)&temp);
  if (entry)
    return entry->i;
  else
    return -1;
}

/* In feedback generation pass, emit an increment of a counter.
   This is at relative location OFFSET within the area of
   counters allocated for T.  Do not do this if OFFSET is -1.

   In feedback use pass, emit an FDO_COUNT NOTE.  If USE is 
   fdo_incoming, make an FDO_COUNT_INCOMING note.  If USE is 
   fdo_outgoing, make an FDO_COUNT_OUTGOING note.  If USE is 
   fdo_block, make an FDO_COUNT_BLOCK note.  If USE is 
   fdo_none, do not make a note.

   FDO_COUNT_INCOMING notes appear at the beginning of a block,
   and refer to what is expected to be the only incoming arc
   (a branch from somewhere else, or fallthrough).  
   FDO_COUNT_OUTGOING notes appear at the end of a block, and
   refer to what is expected to be the only outgoing arc
   (should be followed by unconditional branch or label). 
   These may appear in places that can't be reached, e.g.
   after an unconditional branch and before a label.
   FDO_COUNT_BLOCK notes are just what you think.  
   Logically incoming and outgoing notes carry no more information
   than block notes, but are helpful in consistency checking. */

void 
emit_rtx_feedback_counter (t, offset, use)
    tree t;
    int offset;
    enum fdo_note_kind use;
{
  if (flag_create_feedback && offset >= 0)
    {
      int ix;
      rtx r, note;
      if ((unsigned)offset >= n_slots (t))
	abort ();
      r = create_feedback_counter_rtx (feedback_counts_table,
	    &profile_info.count_instrumented_edges,
	    &profile_info.count_edges_instrumented_now);
      note = emit_note (NULL, NOTE_INSN_FDO_COUNT_RTX);
      NOTE_FDO_COUNT_RTX (note) = r;
      ix = find_tree_in_table (control_flow_table, t);
      if (ix == -1)
	abort ();
      VARRAY_INT (control_flow_to_counter_table, ix + offset) =
	  profile_info.count_instrumented_edges - 1;
    }
  else if (flag_use_feedback && use != fdo_none)
    {
      HOST_WIDEST_INT count;
      rtx note;
      if ((unsigned)offset >= n_slots (t))
	abort ();
      count = times_arc_executed (t, offset);
      if (use == fdo_incoming)
        note = emit_note (NULL, NOTE_INSN_FDO_COUNT_INCOMING);
      else if (use == fdo_outgoing)
	note = emit_note (NULL, NOTE_INSN_FDO_COUNT_OUTGOING);
      else if (use == fdo_block)
	note = emit_note (NULL, NOTE_INSN_FDO_COUNT_BLOCK);
      else if (use == fdo_tablejump)
	note = emit_note (NULL, NOTE_INSN_FDO_COUNT_TABLEJUMP);
      else
	abort ();
      set_fdo_note_count (note, count);
    }
}

/* Compute number of slots to reserve in the tables for a
   given tree.  This is a maximum, it is OK for there to
   be empty slots.  This is language and target dependent. */
unsigned int 
n_slots (t)
      tree t;
{
  unsigned int size = 0;
  switch (TREE_CODE (t))
    {
      case FUNCTION_DECL:
	size = 3;
	break;
      case TRUTH_ORIF_EXPR:
      case TRUTH_ANDIF_EXPR:
      case FLOAT_EXPR:
      case FIX_TRUNC_EXPR:
	size = 2;
	break;
      case COND_EXPR:
	size = 4;
	break;
      case ARRAY_REF:
      case LABEL_DECL:
      case MIN_EXPR:
      case MAX_EXPR:
	size = 1;
	break;
      case LT_EXPR:
      case LE_EXPR:
      case EQ_EXPR:
      case NE_EXPR:
      case GE_EXPR:
      case GT_EXPR:
      case UNLT_EXPR:
      case UNEQ_EXPR:
      case UNGT_EXPR:
      case UNGE_EXPR:
      case UNLE_EXPR:
      case UNORDERED_EXPR:
      case ORDERED_EXPR:
        size = 1;
	break;
      default:
	size = (*lang_hooks.find_control_flow_tree) (t);
    }
  return size;
}

/* During RTL generation a tree can be replaced with a new one.
   Record this in the tables so that looking up the new tree
   gives us the same counter(s) as the old tree.  This does not
   cause any new entries to be made in the control_flow_to_counter
   table or the control_flow_count table; it just causes
   an additional tree to point to existing entries in those tables.
   This can be called more than once for the same newt; the
   latest value will replace earlier ones. */
void
clone_rtx_feedback_counter (oldt, newt)
    tree oldt;
    tree newt;
{
  /* This gets called from fold () before the tables are built. */
  if (!(seen_fndecl && htab_find_slot (seen_fndecl, current_function_decl, NO_INSERT)))
    return;
  if (flag_create_feedback || flag_use_feedback)
    {
      unsigned int n = n_slots (oldt);
      int ix;
      struct map *m;
      struct map temp;
      PTR *p;
      /* This can get called for arbitrary trees, allow clean bailout. */
      if (n == 0)
	return;
      ix = find_tree_in_table (control_flow_table, oldt);
      if (ix == -1)
	abort ();
      temp.t = newt;
      m = (struct map *)htab_find (control_flow_table, (PTR)&temp);
      if (m)
	{
	  /* newt already had an entry, overwrite it. */
	  m->i = ix;
	  return;
        }
      m = (struct map *)ggc_alloc (sizeof (struct map));
      m->t = newt;
      p = htab_find_slot (control_flow_table, (PTR) m, INSERT);
      m->i = ix;
      *p = (PTR) m;
      /* make an entry for FUNCTION_DECLs in this table too, so
	 cloning will work for nodes within the cloned body. */
      if (TREE_CODE (newt) == FUNCTION_DECL && DECL_SAVED_TREE (newt))
	{ 
	  PTR slot = htab_find_slot (seen_fndecl, newt, INSERT);
	  *(tree *)slot = newt;
	}
    }
}

void 
emit_rtx_call_feedback_counter (t)
    tree t;
{
  if (flag_create_feedback)
    {
      int ix;
      rtx r, note;
      ix = find_tree_in_table (call_table, t);
      if (ix == -1)
	/* A call not in the table occurs for those generated
	   during RTL expansion, for example by aggregate copy.
	   These are not visible to the inliner on pass 2,
	   so just ignore them. */
	return;
      r = create_feedback_counter_rtx (feedback_call_counts_table,
	    &profile_info.count_instrumented_calls,
	    &profile_info.count_calls_instrumented_now);
      note = emit_note (NULL, NOTE_INSN_FDO_COUNT_RTX);
      NOTE_FDO_COUNT_RTX (note) = r;
      VARRAY_INT (call_to_counter_table, ix) = 
	  profile_info.count_instrumented_calls - 1;
    }
  /* Do not put anything in the RTL for calls with use_feedback. */
}

static tree
find_decoratable_tree (t, i, j)
    tree *t;
    int *i ATTRIBUTE_UNUSED;
    void *j ATTRIBUTE_UNUSED;
{
  unsigned int size = 0;
  if (TREE_CODE (*t) == CALL_EXPR)
    {
      struct map *m = (struct map *)ggc_alloc (sizeof (struct map));
      PTR* p;
      m->t = *t;
      p = htab_find_slot (call_table, (PTR) m, INSERT);
      if (flag_create_feedback)
	{
	  VARRAY_PUSH_INT (call_to_counter_table, -1);
	  m->i = VARRAY_ACTIVE_SIZE (call_to_counter_table) - 1;
	}
      else
	m->i = call_counter++;
      *p = (PTR) m;
    }
  else
    size = n_slots (*t);

  if (size > 0)
    {    
      /* Reserve "size" elements in the control_flow_to_counter
	 table.  */
      struct map *m = (struct map *)ggc_alloc (sizeof (struct map));
      PTR* p;
      m->t = *t;
      p = htab_find_slot (control_flow_table, (PTR) m, INSERT);
      /* There may already be a slot especially in the case of
	 FUNCTION_DECL, don't overwrite an old entry. */
      /* The NULL here is really the value of EMPTY_ENTRY in
	 hashtab.c, which is 0, but not exported for some reason. */
      if (*p == NULL)
	{
	  if (flag_create_feedback)
	    {
	      VARRAY_PUSH_INT (control_flow_to_counter_table, -1);
	      m->i = VARRAY_ACTIVE_SIZE (control_flow_to_counter_table) - 1;
	      while (--size > 0)
		VARRAY_PUSH_INT (control_flow_to_counter_table, -1);
	    }
	  else
	    {
	      m->i = control_flow_counter;
	      control_flow_counter += size;
	    }
	  *p = (PTR) m;
	}
    }
  return NULL_TREE;
}

/* Per-function entry point, called just before tree-based inlining. */
/* Walk the tree in a deterministic order and find all the interesting
   control-flow nodes and call nodes.  For us this includes e.g. ?: 
   which is target dependent.  Later, we will put feedback info in the RTL, and 
   construct a map from tree indices to feedback indices (using the 
   tree address to find the tree index).  This map goes out into the 
   feedback file, is read back in pass 2, and is used to find the feedback
   info for calls before inlining. */

void 
decorate_for_feedback (fndecl)
    tree fndecl;
{
  if (DECL_SAVED_TREE (fndecl))
    {
      if (flag_create_feedback || flag_use_feedback)
	{
	    /* This code is useful for finding pass1/pass2 mismatch bugs.
	    BEGIN COMMENTED-OUT CODE
*/	    if (getenv ("SECRET_DUMP"))
	      {
		static int nfunc=0;
		if (flag_create_feedback)
		  printf("function %d calls %d control flow %d\n",
		     nfunc, VARRAY_ACTIVE_SIZE(call_to_counter_table), 
		     VARRAY_ACTIVE_SIZE(control_flow_to_counter_table));
		else
		  printf("function %d calls %d control flow %d\n",
		     nfunc, call_counter, control_flow_counter);
		nfunc++;
	      }
/*            END COMMENTED-OUT CODE */
	  /* Space for function-entry and function-exit arcs.*/
	  struct map *m = (struct map *)ggc_alloc (sizeof (struct map));
	  PTR* p;
	  PTR slot = htab_find_slot (seen_fndecl, fndecl, NO_INSERT);
	  /* Abort if invited to decorate this tree a second time.  */
	  if (slot)
	    abort();
	  m->t = fndecl;
	  p = htab_find_slot (control_flow_table, (PTR) m, INSERT);
	  if (flag_create_feedback)
	    {
	      m->i = VARRAY_ACTIVE_SIZE (control_flow_to_counter_table);
	      VARRAY_PUSH_INT (control_flow_to_counter_table, -1);
	      VARRAY_PUSH_INT (control_flow_to_counter_table, -1);
	      VARRAY_PUSH_INT (control_flow_to_counter_table, -1);
	    }
	  else
	    {
	      m->i = control_flow_counter;
	      control_flow_counter += 3;
	    }
          *p = (PTR) m;
	  if (flag_create_feedback)
	    /* Ensure constructor will come out. */
	    need_func_profiler = 1;
	  profile_info.count_edges_instrumented_now = 0;  /* per func counter */
	  profile_info.count_calls_instrumented_now = 0;  /* per func counter */
	  /* Walk the tree in language-dependent fashion, calling 
	     find_decoratable_tree on each call or control-flow node.  
             This *should* just be a direct call to 
             walk_tree_without_duplicates, but that currently 
	     understands only the C-language-family version of trees. */
	  (*lang_hooks.decorate_for_feedback) (fndecl, find_decoratable_tree);
	  /* Remember we've decorated this function body.  */
	  slot = htab_find_slot (seen_fndecl, fndecl, INSERT);
	  *(tree *)slot = fndecl;
	}
    }
}

/* Called per function, after RTL expansion complete. */
/* In pass2 called twice for inline functions, once after 
   deferral and once after RTL gen.  */
void
expand_function_feedback_end ()
{
  if (flag_create_feedback)
    {
      /* Expand feedback counter NOTEs into real RTL.  We delayed this
	 until now to avoid perturbing the generating RTL so it will not
	 cause differences between pass1 and pass2, for example in
	 group_case_nodes(). */
      rtx insn;
      rtx next = NULL_RTX;  /* dumb compiler thinks use is uninitialized */
      /* Abort if we haven't yet decorated this function body.  */
      if (!htab_find_slot (seen_fndecl, current_function_decl, NO_INSERT))
	abort();
      for (insn = get_insns (); insn; insn = next)
	{
	  next = NEXT_INSN (insn);
	  if (GET_CODE (insn) == NOTE
	      && NOTE_LINE_NUMBER (insn) == NOTE_INSN_FDO_COUNT_RTX)
	    {
	      emit_insn_before (NOTE_FDO_COUNT_RTX (insn), insn);
	      delete_insn (insn);
	    }
	}
    }
}

/* Called once, from the profile.c end-of-file code. */
void
add_new_feedback_specifics_to_header (value_chain_addr, decl_chain_addr)
    tree *value_chain_addr;
    tree *decl_chain_addr;
{
  /* The various arc tables are nonzero-sized if we reach here. 
     The call tables may not be and we have to handle this case. */
  tree value_chain = *value_chain_addr;
  tree decl_chain = *decl_chain_addr;
  tree field_decl;
  tree value;
  {
    /* Call counts table. */
    tree type_array_pointer_type
      = build_pointer_type (TREE_TYPE (feedback_call_counts_table));

    field_decl
      = build_decl (FIELD_DECL, get_identifier ("counts2"),
		    build_pointer_type (make_unsigned_type (GCOV_TYPE_SIZE)));
    TREE_CHAIN (field_decl) = decl_chain;
    decl_chain = field_decl;

    if (profile_info.count_instrumented_calls)
      value = build1 (ADDR_EXPR, type_array_pointer_type, 
			    feedback_call_counts_table);
    else
      value = build1 (INTEGER_CST, type_array_pointer_type, 0);
    value_chain = tree_cons (field_decl, value, value_chain);
  }
  {
    /* Size of call counts table. */
    field_decl
      = build_decl (FIELD_DECL, get_identifier ("ncounts2"),
		    long_integer_type_node);
    TREE_CHAIN (field_decl) = decl_chain;
    decl_chain = field_decl;

    value_chain = tree_cons (field_decl,
			     convert (long_integer_type_node,
				      build_int_2 (
					profile_info.count_instrumented_calls, 
					0)), value_chain);
  }
  {
    /* Arc indices table. */
    /* emit control-flow-tree-to-counter table */
    /* (indexes into these tables are implicit, constructed at runtime 
       by a tree walk.  They are the same in feedback creation and use 
       passes.) */
    tree domain = build_index_type (build_int_2 (
	  VARRAY_ACTIVE_SIZE (control_flow_to_counter_table) - 1, 0));
    tree array_type = 
	build_array_type (make_unsigned_type (LONG_TYPE_SIZE), domain);
    tree type_array_pointer_type = build_pointer_type (array_type);
    tree tree_to_counter_table = 
	build (VAR_DECL, array_type, NULL_TREE, NULL_TREE);
    tree value_chain2 = NULL_TREE;
    unsigned int i;
    char name[20];
    TREE_STATIC (tree_to_counter_table) = 1;
    ASM_GENERATE_INTERNAL_LABEL (name, "LFDO", 2);
    DECL_NAME (tree_to_counter_table) = get_identifier (name);
    DECL_SIZE (tree_to_counter_table) = TYPE_SIZE (array_type);
    DECL_SIZE_UNIT (tree_to_counter_table) = TYPE_SIZE_UNIT (array_type);
    DECL_ALIGN (tree_to_counter_table) = TYPE_ALIGN (array_type);

    for (i = 0; 
	 i < VARRAY_ACTIVE_SIZE (control_flow_to_counter_table); 
	 i++)
      value_chain2 = tree_cons (
	      build_int_2 (i, 0),
	      build_int_2
		    (VARRAY_INT (control_flow_to_counter_table, i), 0),
	      value_chain2);

    DECL_INITIAL (tree_to_counter_table)
      = build (CONSTRUCTOR, array_type, NULL_TREE,
	       nreverse (value_chain2));

    assemble_variable (tree_to_counter_table, 0, 0, 0);

    field_decl
      = build_decl (FIELD_DECL, get_identifier ("counts3"),
		    build_pointer_type (make_unsigned_type (LONG_TYPE_SIZE)));
    TREE_CHAIN (field_decl) = decl_chain;
    decl_chain = field_decl;

    value_chain = tree_cons (field_decl, build1 (ADDR_EXPR,
					  type_array_pointer_type,
					  tree_to_counter_table), 
			     value_chain);
  }
  {
    /* Size of arc indices table. */
    field_decl
      = build_decl (FIELD_DECL, get_identifier ("ncounts3"),
		    long_integer_type_node);
    TREE_CHAIN (field_decl) = decl_chain;
    decl_chain = field_decl;

    value_chain = tree_cons (field_decl,
			     convert (long_integer_type_node,
				      build_int_2 (
					VARRAY_ACTIVE_SIZE (
						control_flow_to_counter_table),
					0)), value_chain);
  }
  {
    /* Call indices table. */
    /* emit call-tree-to-counter table */
    tree domain = build_index_type (build_int_2 (
	  VARRAY_ACTIVE_SIZE (call_to_counter_table) - 1, 0));
    tree array_type = 
	build_array_type (make_unsigned_type (LONG_TYPE_SIZE), domain);
    tree type_array_pointer_type = build_pointer_type (array_type);
    tree tree_to_counter_table = 
	build (VAR_DECL, array_type, NULL_TREE, NULL_TREE);
    tree value_chain2 = NULL_TREE;
    unsigned int i;
    char name[20];
    TREE_STATIC (tree_to_counter_table) = 1;
    ASM_GENERATE_INTERNAL_LABEL (name, "LFDO", 3);
    DECL_NAME (tree_to_counter_table) = get_identifier (name);
    DECL_SIZE (tree_to_counter_table) = TYPE_SIZE (array_type);
    DECL_SIZE_UNIT (tree_to_counter_table) = TYPE_SIZE_UNIT (array_type);
    DECL_ALIGN (tree_to_counter_table) = TYPE_ALIGN (array_type);

    for (i = 0; 
	 i < VARRAY_ACTIVE_SIZE (call_to_counter_table); 
	 i++)
      value_chain2 = tree_cons (
	      build_int_2 (i, 0),
	      build_int_2
		    (VARRAY_INT (call_to_counter_table, i), 0),
	      value_chain2);

    DECL_INITIAL (tree_to_counter_table)
      = build (CONSTRUCTOR, array_type, NULL_TREE,
	       nreverse (value_chain2));

    if (VARRAY_ACTIVE_SIZE (call_to_counter_table))
      assemble_variable (tree_to_counter_table, 0, 0, 0);

    field_decl
      = build_decl (FIELD_DECL, get_identifier ("counts4"),
		    build_pointer_type (make_unsigned_type (LONG_TYPE_SIZE)));
    TREE_CHAIN (field_decl) = decl_chain;
    decl_chain = field_decl;

    if (VARRAY_ACTIVE_SIZE (call_to_counter_table))
      value = build1 (ADDR_EXPR, type_array_pointer_type, 
			    tree_to_counter_table);
    else
      value = build1 (INTEGER_CST, type_array_pointer_type, 0);
    value_chain = tree_cons (field_decl, value, value_chain);
  }
  {
    /* Size of call indices table. */
    field_decl
      = build_decl (FIELD_DECL, get_identifier ("ncounts4"),
		    long_integer_type_node);
    TREE_CHAIN (field_decl) = decl_chain;
    decl_chain = field_decl;

    value_chain = tree_cons (field_decl,
			     convert (long_integer_type_node,
				      build_int_2 (
					VARRAY_ACTIVE_SIZE (call_to_counter_table),
					0)), value_chain);
  }
  *decl_chain_addr = decl_chain;
  *value_chain_addr = value_chain;
}

/* Get number of times a call was executed. 
   Returns -1 if no feedback info for this call available.
   T must be a CALL_EXPR. */

HOST_WIDEST_INT
times_call_executed(t)
    tree t;
{
  int ix, j;
  if (TREE_CODE (t) != CALL_EXPR)
    abort ();
  if (!flag_use_feedback)
    return -1;
  ix = find_tree_in_table (call_table, t);
  if (ix == -1)
    return -1;
  j = VARRAY_INT (call_to_counter_table, ix);
  return VARRAY_WIDEST_INT (call_counts, j);
}

/* Set the number of times a call was executed, for
   future use.  T must be a CALL_EXPR.  It need not
   have been known to the machinery previously, i.e.
   the inliner can use this for cloned copies; however,
   it is only available on the use pass (the creation
   pass doesn't permit inlining.)  */

void
set_times_call_executed(t, n)
    tree t;
    HOST_WIDEST_INT n;
{
  int ix, j;
  if (TREE_CODE (t) != CALL_EXPR)
    abort ();
  if (!flag_use_feedback)
    return;
  ix = find_tree_in_table (call_table, t);
  if (ix != -1)
    {
      j = VARRAY_INT (call_to_counter_table, ix);
      VARRAY_WIDEST_INT (call_counts, j) = n;
    }
  else
    {
      struct map *m = (struct map *)ggc_alloc (sizeof (struct map));
      PTR* p;
      m->t = t;
      p = htab_find_slot (call_table, (PTR) m, INSERT);
      m->i = VARRAY_ACTIVE_SIZE (call_to_counter_table);
      *p = (PTR )m;
      VARRAY_PUSH_INT (call_to_counter_table, VARRAY_ACTIVE_SIZE (call_counts));
      VARRAY_PUSH_WIDEST_INT (call_counts, n);
   }
}

/* Get number of times an arc was executed.  T is a tree that
   has some sort of control flow in it, in language-dependent
   and target-dependent fashion.  T may have multiple counters
   associated with it which are distinguished by offset;
   typically this occurs when T generates multiple basic blocks,
   for example FOR_STMT.
   Returns -1 if no feedback info for this call available. */

HOST_WIDEST_INT
times_arc_executed (t, offset)
    tree t;
    unsigned int offset;
{
  int ix, j;
  if (!flag_use_feedback)
    return -1;
  ix = find_tree_in_table (control_flow_table, t);
  if (ix == -1)
    return -1;
  j = VARRAY_INT (control_flow_to_counter_table, ix + offset);
  return VARRAY_WIDEST_INT (control_flow_counts, j);
}

/* Set the number of times an arc was executed, for
   future use.  T need not have been known to the 
   machinery previously, i.e. the inliner can use 
   this for cloned copies.  However, this feature can
   be used only in the use pass; the create pass
   doesn't permit inlining. */

void
set_times_arc_executed(t, offset, n)
    tree t;
    unsigned int offset;
    HOST_WIDEST_INT n;
{
  int ix, j;
  if (!flag_use_feedback)
    return;
  ix = find_tree_in_table (control_flow_table, t);
  if (ix == -1)
    {
      unsigned int size = n_slots (t);
      if (size > 0)
	{
	  struct map *m = (struct map *)ggc_alloc (sizeof (struct map));
	  PTR* p;
	  m->t = t;
	  p = htab_find_slot (control_flow_table, (PTR) m, INSERT);
	  m->i = ix = VARRAY_ACTIVE_SIZE (control_flow_to_counter_table);
	  *p = (PTR )m;

	  /* Create 'size' slots in the other tables; all counts but 
	     the one at 'offset' are set to 'never executed' for the moment. */
	  for (; size > 0; size--)
	    {
	      VARRAY_PUSH_INT (control_flow_to_counter_table, 
			    VARRAY_ACTIVE_SIZE (control_flow_counts));
	      VARRAY_PUSH_WIDEST_INT (control_flow_counts, -1);
	    }
	}
    }              
  j = VARRAY_INT (control_flow_to_counter_table, ix + offset);
  VARRAY_WIDEST_INT (control_flow_counts, j) = n;
}

/* FIXME: These are taken from profile.c, as is much of the code
   that uses them.  There should be more sharing. */
/* Additional information about the edges we need.  */
struct edge_info {
  unsigned int count_valid : 1;
  
  /* Is on the spanning tree.  */
  unsigned int on_tree : 1;
  
  /* Pretend this edge does not exist (it is abnormal and we've
     inserted a fake to compensate).  */
  unsigned int ignore : 1;
};

struct bb_info {
  unsigned int count_valid : 1;

  /* Number of successor and predecessor edges.  */
  gcov_type succ_count;
  gcov_type pred_count;
};

#define EDGE_INFO(e)  ((struct edge_info *) (e)->aux)
#define BB_INFO(b)  ((struct bb_info *) (b)->aux)

/* Convert count info in NOTEs found in the RTL into BR_PROB
   notes on the branch instructions.  This may be called as
   often as the flowgraph is recreated, but of course the
   NOTE info must be propagated through in between.  */

void
note_feedback_count_to_br_prob ()
{
  basic_block bb;
  int changes;
  int passes;
  int hist_br_prob[20];
  int num_never_executed;
  int num_branches;
  int roundoff_err_found;
  int n_edges;
  int max_n_edges = 0;
  int i;
  rtx insn;

  if (!flag_use_feedback)
    return;

  /* To handle the case where a callee calls exit(), or
     things do not add up due to inliner roundoff errors. */
  /* This isn't workable, unfortunately, because it expects
     the block and arc counts to be meaningful on input (and
     calls verify_flow_info to be sure), which they aren't. */
  /* flow_call_edges_add (NULL); */
  /* To enable handling of noreturn blocks.  */
  add_noreturn_fake_exit_edges ();
  connect_infinite_loops_to_exit ();

  alloc_aux_for_blocks (sizeof (struct bb_info));
  alloc_aux_for_edges (sizeof (struct edge_info));

  /* Initialize counters of succ and pred.  */
  FOR_EACH_BB (bb)
    {
      edge e;
      for (e = bb->succ; e; e = e->succ_next)
	BB_INFO (bb)->succ_count++;
      for (e = bb->pred; e; e = e->pred_next)
	BB_INFO (bb)->pred_count++;
    }
  /* Process all feedback notes.  */
  /* The entry and exit pseudoblocks are not visited in these walks.
     It is also possible for there to be notes that are not in any
     block.  These will not be visited here and will be removed later. */
  /* It is permitted to have multiple NOTEs of the same kind.  These
     are of course expected to have the same value.  Since this
     condition is inefficient, a warning is currently given, but there
     are probably cases where the dup can't be eliminated. */
  FOR_EACH_BB (bb)
    {
      edge e;
      gcov_type in_block_count = -1;
      gcov_type out_block_count = -1;
      gcov_type block_count = -1;
      rtx insn;
      for (insn = bb->head; insn != NEXT_INSN (bb->end); 
		insn = NEXT_INSN (insn))
	if (GET_CODE (insn) == NOTE)
	  {
	    if (NOTE_LINE_NUMBER (insn) == NOTE_INSN_FDO_COUNT_INCOMING)
	      {
		/* Should have exactly one predecessor.  */
		int count = 0;
		if (in_block_count != -1)
		  {
		    if (get_fdo_note_count (insn) != in_block_count)
		      abort ();
#if 0
		  /* This happens legitimately a fair amount */
		    warning ("Duplicate COUNT_INCOMING note.");
#endif
		    continue;
		  }
		for (e = bb->pred; e; e = e->pred_next)
		  count++;
		if (count != 1)
		  warning("COUNT_INCOMING in block with multiple predecessors.");
		bb->pred->count = in_block_count = get_fdo_note_count (insn);
		EDGE_INFO (bb->pred)->count_valid = 1;
		/*BB_INFO (bb)->count_valid = 1;*/
		BB_INFO (bb)->pred_count--;
		BB_INFO (bb->pred->src)->succ_count--;
		delete_insn (insn);
	      }
	    if (NOTE_LINE_NUMBER (insn) == NOTE_INSN_FDO_COUNT_OUTGOING)
	      {
		/* Should have exactly one successor.  */
		int count = 0;
		edge real_exit = 0;
		if (out_block_count != -1)
		  {
		    if (get_fdo_note_count (insn) != out_block_count)
		      abort ();
		    warning ("Duplicate COUNT_OUTGOING note.");
		    continue;
		  }
		/* Spurious edges to EXIT_BLOCK get inserted for unconditional
		   branches by connect_infinite_loops_to_exit.  This is OK.
		   Ignore these while handling the OUTGOING note. */
		for (e = bb->succ; e; e = e->succ_next)
		  if (e->flags & EDGE_FAKE)
		    {
		      BB_INFO (bb)->succ_count--;
		      e->count = 0;
		      EDGE_INFO (e)->count_valid = 1;
		    }
		  else
		    {
		      count++;
		      real_exit = e;
		    }
		if (count != 1)
		  warning("COUNT_OUTGOING in block with multiple successors.");
		out_block_count = get_fdo_note_count (insn);
		real_exit->count = out_block_count = get_fdo_note_count (insn);
		EDGE_INFO (real_exit)->count_valid = 1;
		/*BB_INFO (bb)->count_valid = 1;*/
		BB_INFO (bb)->succ_count--;
		BB_INFO (real_exit->dest)->pred_count--;
		delete_insn (insn);
	      }
	    if (NOTE_LINE_NUMBER (insn) == NOTE_INSN_FDO_COUNT_TABLEJUMP)
	      {
		/* The NOTE contains a vector of (label_ref, value) pairs. */
		rtvec vec = NOTE_FDO_TABLEJUMP (insn);
		int i;
		for ( i=0; i < GET_NUM_ELEM (vec); i += 2)
		  {
		    rtx label = XEXP (RTVEC_ELT (vec, i), 0);
		    rtx count = RTVEC_ELT (vec, i+1);
		    HOST_WIDEST_INT value = INTVAL (count);
		    /* This is n^2 which might get slow for big switches... */
		    for (e = bb->succ; e; e = e->succ_next)
		      if (e->dest->head == label)
			{
			  if (EDGE_INFO (e)->count_valid)
			    {
			      if (e->count != value)
				warning ("Duplicate TABLEJUMP counts don't match!");			  }
			  else
			    {
			      BB_INFO (bb)->succ_count--;
			      BB_INFO (e->dest)->pred_count--;
			    }
			  EDGE_INFO (e)->count_valid = 1;
			  e->count = value;
			  break;
			}
		  } 
		delete_insn (insn);
	      }
	    if (NOTE_LINE_NUMBER (insn) == NOTE_INSN_FDO_COUNT_BLOCK)
	      {
		/* No info available about edges */
		if (block_count != -1)
		  {
		    /* After inlining we get -1 block counts coming in.
		       For now, ignore these and keep the old one.
		       Reenable this abort later. */
		    if (get_fdo_note_count (insn) != block_count)
		      {
			if (get_fdo_note_count (insn) == -1)
			  continue;
			abort ();
		      }
		    /* This is too common to be worth warning about. */
		    /* warning ("Duplicate COUNT_BLOCK note."); */
		    continue;
		  }
		bb->count = block_count = get_fdo_note_count (insn);
		BB_INFO (bb)->count_valid = 1;
		delete_insn (insn);
	      }
	  }
      /* The following are errors.  They probably indicate a mismatch
         between the pass1 and pass2 tree structures.  */
      /* The off-by-1 case arises legitimately when the block
	 contains a function call that does not return.  This
	 might be true of any function (that calls exit(), for
	 example) so just assume it is this case. */
      /* The counts are not likely to add up quite right after
	 inlining either.  Some heuristics may be necessary. */
      if (in_block_count != -1 && out_block_count != -1 
	  && in_block_count != out_block_count
	  && in_block_count != out_block_count + 1)
	warning("INCOMING and OUTGOING counts do not agree!");
      if (in_block_count != -1 && block_count != -1
	  && in_block_count != block_count
	  && in_block_count != block_count + 1)
	warning("INCOMING and BLOCK counts do not agree!");
      if (block_count != -1 && out_block_count != -1
	  && block_count != out_block_count
	  && block_count != out_block_count + 1)
	warning("BLOCK and OUTGOING counts do not agree!");
    }

  /* Remove any lurking FDO_COUNT notes (they can occur outside blocks). */
  for (insn = get_insns (); insn; insn = NEXT_INSN (insn))
    if (GET_CODE (insn) == NOTE
	&& (NOTE_LINE_NUMBER (insn) == NOTE_INSN_FDO_COUNT_INCOMING
	    || NOTE_LINE_NUMBER (insn) == NOTE_INSN_FDO_COUNT_OUTGOING
	    || NOTE_LINE_NUMBER (insn) == NOTE_INSN_FDO_COUNT_BLOCK
	    || NOTE_LINE_NUMBER (insn) == NOTE_INSN_FDO_COUNT_TABLEJUMP))
      delete_insn (insn);

  /* If the entry count is bigger than the exit count, we called
     exit() or some such (possibly indirectly); things may not add up
     right.  The same is true if we have roundoff errors from inlining.
     We can't tell, so just apply a small fudge factor. */

  /* FIXME: almost everything from here down duplicates profile.c */
  changes = 1;
  passes = 0;
  roundoff_err_found = 0;
  while (changes)
    {
      passes++;
      changes = 0;
      FOR_EACH_BB (bb)
	{
	  struct bb_info *bi = BB_INFO (bb);
	  if (! bi->count_valid)
	    {
	      if (bi->succ_count == 0)
		{
		  edge e;
		  gcov_type total = 0;

		  for (n_edges = 0, e = bb->succ; e; e = e->succ_next)
		    {
		      total += e->count;
		      n_edges++;
		    }
		  bb->count = total;
		  bi->count_valid = 1;
		  changes = 1;
		  if (n_edges > max_n_edges)
		    max_n_edges = n_edges;
		}
	      else if (bi->pred_count == 0)
		{
		  edge e;
		  gcov_type total = 0;

		  for (n_edges = 0, e = bb->pred; e; e = e->pred_next)
		    {
		      total += e->count;
		      n_edges++;
		    }
		  bb->count = total;
		  bi->count_valid = 1;
		  changes = 1;
		  if (n_edges > max_n_edges)
		    max_n_edges = n_edges;
		}
	    }
	  if (bi->count_valid)
	    {
	      if (bi->succ_count == 1)
		{
		  edge e, remaining_e = 0;
		  gcov_type total = 0;

		  for (n_edges = 0, e = bb->succ; e; e = e->succ_next)
		    {
		      n_edges++;
		      if (!EDGE_INFO (e)->count_valid)
			remaining_e = e;
		      else
			total += e->count;
		    }
		  if (n_edges > max_n_edges)
		    max_n_edges = n_edges;

		  /* Calculate count for remaining edge by conservation.  */
		  total = bb->count - total;
		  /* With inlining roundoff errors in scaling may result 
		     in things not adding up right.  For now just make
		     sure no edge has a negative count.  We allow a 
		     fudge factor of 1 per edge on the first pass, 
		     certainly enough, possibly too much.  On later
		     passes the max error may have propagated anywhere
		     so we have to be more permissive. */
		  if (total >= -(passes == 1 ?  n_edges : max_n_edges)
		      && total <= -1)
		    {
		      total = 0;
		      roundoff_err_found = 1;
		    }

		  if (! remaining_e)
		    abort ();
		  EDGE_INFO (remaining_e)->count_valid = 1;
		  remaining_e->count = total;
		  bi->succ_count--;

		  BB_INFO (remaining_e->dest)->pred_count--;
		  changes = 1;
		}
	      if (bi->pred_count == 1)
		{
		  edge e, remaining_e = 0;
		  gcov_type total = 0;

		  /* One of the counts will be invalid, but it is zero,
		     so adding it in also doesn't hurt.  */
		  for (n_edges = 0, e = bb->pred; e; e = e->pred_next)
		    {
		      n_edges++;
		      if (!EDGE_INFO (e)->count_valid)
			remaining_e = e;
		      else
			total += e->count;
		    }
		  if (n_edges > max_n_edges)
		    max_n_edges = n_edges;

		  /* Calculate count for remaining edge by conservation.  */
		  total = bb->count - total;

		  /* Note, gdb has been known to show you stopped here
		     when you really reached some other abort() ! */
		  if (! remaining_e)
		    abort ();
		  /* With inlining roundoff errors in scaling may result 
		     in things not adding up right.  For now just make
		     sure no edge has a negative count.  We allow a 
		     fudge factor of 1 per edge on the first pass, 
		     certainly enough, possibly too much.  On later
		     passes the max error may have propagated anywhere
		     so we have to be more permissive. */
		  if (total >= -(passes == 1 ?  n_edges : max_n_edges)
		      && total <= -1)
		    {
		      total = 0;
		      roundoff_err_found = 1;
		    }
		  EDGE_INFO (remaining_e)->count_valid = 1;
		  remaining_e->count = total;
		  bi->pred_count--;

		  BB_INFO (remaining_e->src)->succ_count--;
		  changes = 1;
		}
	    }
	}
    }
  if (rtl_dump_file)
    {
    /* can't do this, it tries to print some reg-related structures
       that haven't been built yet. */
    /*dump_flow_info (rtl_dump_file);*/
    fprintf (rtl_dump_file, "Graph solving took %d passes.\n\n", passes);
    }

  /* If the graph has been correctly solved, every block will have a
     succ and pred count of zero.  The occurrence of this warning means
     some more emit_rtx_feedback_counter calls are needed, somewhere...*/
  FOR_EACH_BB (bb)
    {
      if (BB_INFO (bb)->succ_count || BB_INFO (bb)->pred_count)
	warning("Unable to compute execution counts!");
    }

  /* For every edge, calculate its branch probability and add a reg_note
     to the branch insn to indicate this.  Also emit new block count
     notes into the RTL.  In combination with the BR_PROB notes these
     should be adequate to (more or less) recompute the edge counts, 
     should we want that. */

  for (i = 0; i < 20; i++)
    hist_br_prob[i] = 0;
  num_never_executed = 0;
  num_branches = 0;

  FOR_EACH_BB (bb)
    {
      edge e;
      gcov_type total;
      rtx note, insn;

      if (!BB_INFO (bb)->count_valid)
	abort ();
      total = bb->count;
      /* Make a note with the block count.  Put it after the
	 BASIC_BLOCK note.  This is for debugging; nobody uses the note. */
      for (insn = bb->head; ; insn = NEXT_INSN (insn))
	{
	  if (NOTE_INSN_BASIC_BLOCK_P (insn))
	    {
	      note = emit_note_after (NOTE_INSN_FDO_COUNT_BLOCK, insn);
	      set_fdo_note_count (note, total);
	      break;
	    }
	  if (insn == bb->end)	/* No BASIC_BLOCK note?? */
	    break;
	}
      for (e = bb->succ; e; e = e->succ_next)
	{
	  if (!EDGE_INFO (e)->count_valid)
	    abort ();
	  /* Sums may be off due to inlining and noreturn calls.  
	     We allow an error of 1 per edge, but by now the max
	     error might have propagated anywhere in the graph. */
	  if (e->count - total >= 1 && e->count - total <= max_n_edges)
	    total = e->count;
	  e->probability = (e->count * REG_BR_PROB_BASE + total / 2) / total;
	  if (e->probability < 0 || e->probability > REG_BR_PROB_BASE)
	    {
	      error ("corrupted profile info: prob for %d-%d thought to be %d",
		     e->src->index, e->dest->index, e->probability);
	      e->probability = REG_BR_PROB_BASE / 2;
	    }
	}
      if (bb->index >= 0
	  && any_condjump_p (bb->end)
	  && bb->succ->succ_next)
	{
	  int prob;
	  edge e;
	  int index;

	  /* Find the branch edge.  It is possible that we do have fake
	     edges here.  */
	  for (e = bb->succ; e->flags & (EDGE_FAKE | EDGE_FALLTHRU);
	       e = e->succ_next)
	    continue; /* Loop body has been intentionally left blank.  */

	  prob = e->probability;
	  index = prob * 20 / REG_BR_PROB_BASE;

	  if (index == 20)
	    index = 19;
	  hist_br_prob[index]++;

	  note = find_reg_note (bb->end, REG_BR_PROB, 0);
	  /* There may be already note put by some other pass, such
	     as builtin_expect expander.  */
	  if (note)
	    XEXP (note, 0) = GEN_INT (prob);
	  else
	    REG_NOTES (bb->end)
	      = gen_rtx_EXPR_LIST (REG_BR_PROB, GEN_INT (prob),
				   REG_NOTES (bb->end));
	  num_branches++;
	}
    }

  if (rtl_dump_file)
    {
      fprintf (rtl_dump_file, "%d branches\n", num_branches);
      fprintf (rtl_dump_file, "%d branches never executed\n",
	       num_never_executed);
      if (num_branches)
	for (i = 0; i < 10; i++)
	  fprintf (rtl_dump_file, "%d%% branches in range %d-%d%%\n",
		   (hist_br_prob[i] + hist_br_prob[19-i]) * 100 / num_branches,
		   5 * i, 5 * i + 5);

      fputc ('\n', rtl_dump_file);
      fputc ('\n', rtl_dump_file);
    }

  remove_fake_edges ();
  free_aux_for_edges ();
  free_aux_for_blocks ();
}

#include "gt-feedback.h"
/* APPLE LOCAL end this entire file */
