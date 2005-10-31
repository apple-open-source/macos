/* Loop optimizations over tree-ssa.
   Copyright (C) 2003 Free Software Foundation, Inc.
   
This file is part of GCC.
   
GCC is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.
   
GCC is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.
   
You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "rtl.h"
#include "tm_p.h"
#include "hard-reg-set.h"
#include "basic-block.h"
#include "output.h"
#include "diagnostic.h"
#include "tree-flow.h"
#include "tree-dump.h"
#include "tree-pass.h"
#include "timevar.h"
#include "cfgloop.h"
#include "flags.h"
#include "tree-inline.h"
#include "tree-scalar-evolution.h"
/* APPLE LOCAL begin lno */
#include "tree-data-ref.h"
#include "function.h"
/* APPLE LOCAL end lno */

/* The loop tree currently optimized.  */

struct loops *current_loops;

/* Initializes the loop structures.  DUMP is the file to that the details
   about the analysis should be dumped.  */

/* APPLE LOCAL lno */
struct loops *
tree_loop_optimizer_init (FILE *dump)
{
  struct loops *loops = loop_optimizer_init (dump);

  if (!loops)
    return NULL;

  /* Creation of preheaders may create redundant phi nodes if the loop is
     entered by more than one edge, but the initial value of the induction
     variable is the same on all of them.  */
  kill_redundant_phi_nodes ();
  rewrite_into_ssa (false);
  bitmap_clear (vars_to_rename);

  rewrite_into_loop_closed_ssa ();
#ifdef ENABLE_CHECKING
  verify_loop_closed_ssa ();
#endif

  return loops;
}

/* The loop superpass.  */

static bool
gate_loop (void)
{
  return flag_tree_loop_optimize != 0;
}

struct tree_opt_pass pass_loop = 
{
  "loop",				/* name */
  gate_loop,				/* gate */
  NULL,					/* execute */
  NULL,					/* sub */
  NULL,					/* next */
  0,					/* static_pass_number */
  TV_TREE_LOOP,				/* tv_id */
  PROP_cfg,				/* properties_required */
  0,					/* properties_provided */
  0,					/* properties_destroyed */
  TODO_ggc_collect,			/* todo_flags_start */
  TODO_dump_func | TODO_verify_ssa | TODO_ggc_collect,	/* todo_flags_finish */
  0					/* letter */
};

/* Loop optimizer initialization.  */

static void
tree_ssa_loop_init (void)
{
  current_loops = tree_loop_optimizer_init (dump_file);
  if (!current_loops)
    return;

  /* Find the loops that are exited just through a single edge.  */
  mark_single_exit_loops (current_loops);

  scev_initialize (current_loops);
}
  
struct tree_opt_pass pass_loop_init = 
{
  "loopinit",				/* name */
  NULL,					/* gate */
  tree_ssa_loop_init,			/* execute */
  NULL,					/* sub */
  NULL,					/* next */
  0,					/* static_pass_number */
  TV_TREE_LOOP_INIT,			/* tv_id */
  PROP_cfg,				/* properties_required */
  0,					/* properties_provided */
  0,					/* properties_destroyed */
  0,					/* todo_flags_start */
  TODO_dump_func,			/* todo_flags_finish */
  0					/* letter */
};

/* Loop invariant motion pass.  */

static void
tree_ssa_loop_im (void)
{
  if (!current_loops)
    return;

  tree_ssa_lim (current_loops);
}

static bool
gate_tree_ssa_loop_im (void)
{
  return flag_tree_loop_im != 0;
}

struct tree_opt_pass pass_lim = 
{
  "lim",				/* name */
  gate_tree_ssa_loop_im,		/* gate */
  tree_ssa_loop_im,			/* execute */
  NULL,					/* sub */
  NULL,					/* next */
  0,					/* static_pass_number */
  TV_LIM,				/* tv_id */
  PROP_cfg,				/* properties_required */
  0,					/* properties_provided */
  0,					/* properties_destroyed */
  0,					/* todo_flags_start */
  TODO_dump_func,                	/* todo_flags_finish */
  0					/* letter */
};

/* Loop unswitching pass.  */

static void
tree_ssa_loop_unswitch (void)
{
  if (!current_loops)
    return;

  tree_ssa_unswitch_loops (current_loops);
}

static bool
gate_tree_ssa_loop_unswitch (void)
{
  return flag_unswitch_loops != 0;
}

struct tree_opt_pass pass_unswitch = 
{
  "unswitch",				/* name */
  gate_tree_ssa_loop_unswitch,		/* gate */
  tree_ssa_loop_unswitch,		/* execute */
  NULL,					/* sub */
  NULL,					/* next */
  0,					/* static_pass_number */
  TV_TREE_LOOP_UNSWITCH,		/* tv_id */
  PROP_cfg,				/* properties_required */
  0,					/* properties_provided */
  0,					/* properties_destroyed */
  0,					/* todo_flags_start */
  TODO_dump_func,                	/* todo_flags_finish */
  0					/* letter */
};

/* APPLE LOCAL begin lno */
/* A pass for testing of loop infrastructure.  */

static void
tree_ssa_loop_test (void)
{
  if (!current_loops)
    return;

  scev_analysis ();
  analyze_all_data_dependences (current_loops);
}

static bool
gate_tree_ssa_loop_test (void)
{
  return flag_tree_ssa_loop_test != 0;
}

struct tree_opt_pass pass_loop_test = 
{
  "lptest",				/* name */
  gate_tree_ssa_loop_test,		/* gate */
  tree_ssa_loop_test,			/* execute */
  NULL,					/* sub */
  NULL,					/* next */
  0,					/* static_pass_number */
  0,					/* tv_id */
  PROP_cfg,				/* properties_required */
  0,					/* properties_provided */
  0,					/* properties_destroyed */
  0,					/* todo_flags_start */
  0,					/* todo_flags_finish */
  0
};

/* Marks loops that cannot be removed in DCE, since they are possibly
   infinite.  */

static void
tree_mark_maybe_inf_loops (void)
{
  if (!current_loops)
    return;

  cfun->marked_maybe_inf_loops = 1;
  mark_maybe_infinite_loops (current_loops);
}

static bool
gate_tree_mark_maybe_inf_loops (void)
{
  return (flag_tree_dce != 0 && optimize >= 2);
}

struct tree_opt_pass pass_mark_maybe_inf_loops = 
{
  "miloops",				/* name */
  gate_tree_mark_maybe_inf_loops,	/* gate */
  tree_mark_maybe_inf_loops,		/* execute */
  NULL,					/* sub */
  NULL,					/* next */
  0,					/* static_pass_number */
  TV_MARK_MILOOPS,  			/* tv_id */
  PROP_cfg | PROP_ssa,			/* properties_required */
  0,					/* properties_provided */
  0,					/* properties_destroyed */
  0,					/* todo_flags_start */
  TODO_dump_func,                	/* todo_flags_finish */
  0
};

/* APPLE LOCAL end lno */

/* APPLE LOCAL begin loops-to-memset  */
/* Loops to memset pass.  */

static void
tree_ssa_loop_memset (void)
{
  if (!current_loops)
    return;
  
  tree_ssa_memset (current_loops);
}

static bool
gate_tree_ssa_loop_memset (void)
{
  return flag_tree_loop_memset != 0;
}

struct tree_opt_pass pass_memset =
{
  "memset",                            /* name */
  gate_tree_ssa_loop_memset,           /* gate */
  tree_ssa_loop_memset,                        /* execute */
  NULL,                                        /* sub */
  NULL,                                        /* next */
  0,                                   /* static_pass_number */
  TV_LIM,                              /* tv_id */
  PROP_cfg,                            /* properties_required */
  0,                                   /* properties_provided */
  0,                                   /* properties_destroyed */
  0,                                   /* todo_flags_start */
  TODO_dump_func,                      /* todo_flags_finish */
  0                                    /* letter */
};
/* APPLE LOCAL end loops-to-memset */

/* Loop autovectorization.  */

static void
tree_vectorize (void)
{
  if (!current_loops)
    return;

  bitmap_clear (vars_to_rename);
  vectorize_loops (current_loops);
}

static bool
gate_tree_vectorize (void)
{
  return flag_tree_vectorize != 0;
}

struct tree_opt_pass pass_vectorize =
{
  "vect",                               /* name */
  gate_tree_vectorize,                  /* gate */
  tree_vectorize,                       /* execute */
  NULL,                                 /* sub */
  NULL,                                 /* next */
  0,                                    /* static_pass_number */
  TV_TREE_VECTORIZATION,                /* tv_id */
  PROP_cfg | PROP_ssa,                  /* properties_required */
  0,                                    /* properties_provided */
  0,                                    /* properties_destroyed */
  0,                                    /* todo_flags_start */
  TODO_dump_func,			/* todo_flags_finish */
  0					/* letter */
};


/* Loop nest optimizations.  */

static void
tree_linear_transform (void)
{
  if (!current_loops)
    return;

  linear_transform_loops (current_loops);
}

static bool
gate_tree_linear_transform (void)
{
  return flag_tree_loop_linear != 0;
}

struct tree_opt_pass pass_linear_transform =
{
  "ltrans",				/* name */
  gate_tree_linear_transform,		/* gate */
  tree_linear_transform,       		/* execute */
  NULL,					/* sub */
  NULL,					/* next */
  0,					/* static_pass_number */
  TV_TREE_LINEAR_TRANSFORM,  		/* tv_id */
  PROP_cfg | PROP_ssa,			/* properties_required */
  0,					/* properties_provided */
  0,					/* properties_destroyed */
  0,					/* todo_flags_start */
  TODO_dump_func,                	/* todo_flags_finish */
  0				        /* letter */	
};

/* APPLE LOCAL begin lno */
/* Prefetching.  */

static void
tree_ssa_loop_prefetch (void)
{
  if (!current_loops)
    return;

  tree_ssa_prefetch_arrays (current_loops);
}

static bool
gate_tree_ssa_loop_prefetch (void)
{
  return flag_prefetch_loop_arrays != 0;
}

struct tree_opt_pass pass_loop_prefetch =
{
  "prefetch",				/* name */
  gate_tree_ssa_loop_prefetch,		/* gate */
  tree_ssa_loop_prefetch,	       	/* execute */
  NULL,					/* sub */
  NULL,					/* next */
  0,					/* static_pass_number */
  TV_TREE_PREFETCH,	  		/* tv_id */
  PROP_cfg | PROP_ssa,			/* properties_required */
  0,					/* properties_provided */
  0,					/* properties_destroyed */
  0,					/* todo_flags_start */
  TODO_dump_func,                	/* todo_flags_finish */
  0
};
/* APPLE LOCAL end lno */

/* Canonical induction variable creation pass.  */

static void
tree_ssa_loop_ivcanon (void)
{
  if (!current_loops)
    return;

  canonicalize_induction_variables (current_loops);
}

static bool
gate_tree_ssa_loop_ivcanon (void)
{
  return flag_tree_loop_ivcanon != 0;
}

struct tree_opt_pass pass_iv_canon =
{
  "ivcanon",				/* name */
  gate_tree_ssa_loop_ivcanon,		/* gate */
  tree_ssa_loop_ivcanon,	       	/* execute */
  NULL,					/* sub */
  NULL,					/* next */
  0,					/* static_pass_number */
  TV_TREE_LOOP_IVCANON,	  		/* tv_id */
  PROP_cfg | PROP_ssa,			/* properties_required */
  0,					/* properties_provided */
  0,					/* properties_destroyed */
  0,					/* todo_flags_start */
  TODO_dump_func,                	/* todo_flags_finish */
  0					/* letter */
};

/* Record bounds on numbers of iterations of loops.  */

static void
tree_ssa_loop_bounds (void)
{
  if (!current_loops)
    return;

  estimate_numbers_of_iterations (current_loops);
  scev_reset ();
}

struct tree_opt_pass pass_record_bounds =
{
  NULL,					/* name */
  NULL,					/* gate */
  tree_ssa_loop_bounds,		       	/* execute */
  NULL,					/* sub */
  NULL,					/* next */
  0,					/* static_pass_number */
  TV_TREE_LOOP_BOUNDS,	  		/* tv_id */
  PROP_cfg | PROP_ssa,			/* properties_required */
  0,					/* properties_provided */
  0,					/* properties_destroyed */
  0,					/* todo_flags_start */
  0,			              	/* todo_flags_finish */
  0					/* letter */
};

/* Complete unrolling of loops.  */

static void
tree_complete_unroll (void)
{
  if (!current_loops)
    return;

  tree_unroll_loops_completely (current_loops);
}

static bool
gate_tree_complete_unroll (void)
{
  return flag_peel_loops || flag_unroll_loops;
}

struct tree_opt_pass pass_complete_unroll =
{
  "cunroll",				/* name */
  gate_tree_complete_unroll,		/* gate */
  tree_complete_unroll,		       	/* execute */
  NULL,					/* sub */
  NULL,					/* next */
  0,					/* static_pass_number */
  TV_COMPLETE_UNROLL,	  		/* tv_id */
  PROP_cfg | PROP_ssa,			/* properties_required */
  0,					/* properties_provided */
  0,					/* properties_destroyed */
  0,					/* todo_flags_start */
  TODO_dump_func,                	/* todo_flags_finish */
  0					/* letter */
};

/* Induction variable optimizations.  */

static void
tree_ssa_loop_ivopts (void)
{
  if (!current_loops)
    return;

  tree_ssa_iv_optimize (current_loops);
}

static bool
gate_tree_ssa_loop_ivopts (void)
{
  return flag_ivopts != 0;
}

struct tree_opt_pass pass_iv_optimize =
{
  "ivopts",				/* name */
  gate_tree_ssa_loop_ivopts,		/* gate */
  tree_ssa_loop_ivopts,		       	/* execute */
  NULL,					/* sub */
  NULL,					/* next */
  0,					/* static_pass_number */
  TV_TREE_LOOP_IVOPTS,	  		/* tv_id */
  PROP_cfg | PROP_ssa,			/* properties_required */
  0,					/* properties_provided */
  0,					/* properties_destroyed */
  0,					/* todo_flags_start */
  TODO_dump_func,                	/* todo_flags_finish */
  0					/* letter */
};

/* Loop optimizer finalization.  */

static void
tree_ssa_loop_done (void)
{
  if (!current_loops)
    return;

#ifdef ENABLE_CHECKING
  verify_loop_closed_ssa ();
#endif

  free_numbers_of_iterations_estimates (current_loops);
  scev_finalize ();
  loop_optimizer_finalize (current_loops,
			   (dump_flags & TDF_DETAILS ? dump_file : NULL));
  current_loops = NULL;
}
  
struct tree_opt_pass pass_loop_done = 
{
  "loopdone",				/* name */
  NULL,					/* gate */
  tree_ssa_loop_done,			/* execute */
  NULL,					/* sub */
  NULL,					/* next */
  0,					/* static_pass_number */
  TV_TREE_LOOP_FINI,			/* tv_id */
  PROP_cfg,				/* properties_required */
  0,					/* properties_provided */
  0,					/* properties_destroyed */
  0,					/* todo_flags_start */
  TODO_cleanup_cfg | TODO_dump_func,	/* todo_flags_finish */
  0					/* letter */
};

