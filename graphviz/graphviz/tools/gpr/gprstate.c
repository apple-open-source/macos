/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/

/*
 * gpr state
 *
 */

#include <gprstate.h>
#include <error.h>
#include <sfstr.h>

int  validTVT(int c)
{
  int rv = 0;

  switch (c) {
  case TV_flat :
  case TV_dfs :
  case TV_fwd :
  case TV_rev :
  case TV_ne :
  case TV_en :
    rv = 1;
    break;
  }
  return rv;
}

void
initGPRState (Gpr_t*state, Vmalloc_t* vm, gpr_info* info)
{
  state->tgtname = vmstrdup (vm, "gvpr_result");
  state->tvt = TV_flat;
  state->tvroot = 0;
  state->outFile = info->outFile;
  state->argc = info->argc;
  state->argv = info->argv;
}

Gpr_t*
openGPRState ()
{
  Gpr_t*      state;

  if (!(state = newof(0, Gpr_t, 1, 0)))
    error (ERROR_FATAL, "Could not create gvpr state: out of memory");

  if (!(state->tmp = sfstropen()))
    error (ERROR_FATAL, "Could not create state");

  return state;
}

