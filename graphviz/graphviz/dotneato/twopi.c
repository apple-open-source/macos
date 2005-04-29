/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/
#pragma prototyped

/*
 * Written by Emden R. Gansner
 * Derived from Graham Wills' algorithm described in GD'97.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include   "circle.h"
#include        <time.h>
#ifdef HAVE_UNISTD_H
#include	<unistd.h>
#endif

char *Info[] = {
    "twopi",            /* Program */
    VERSION,            /* Version */
    BUILDDATE           /* Build Date */
};

static GVC_t *gvc;

#ifndef MSWIN32
static void intr(int s)
{
	if (gvc->g) dotneato_write(gvc);
	dotneato_terminate(gvc);
	exit(1);
}
#endif

int main (int argc, char** argv)
{
  graph_t *g, *prev = NULL;

  gvc = gvNEWcontext(Info, username());

  dotneato_initialize (gvc, argc, argv);
#ifndef MSWIN32
	signal (SIGUSR1, toggle);
	signal (SIGINT, intr);
#endif

  while ((g = next_input_graph())) {
    if (prev) {
      twopi_cleanup(prev);
      agclose(prev);
    }
    prev = g;

    gvBindContext(gvc, g);

    twopi_layout(g);
    dotneato_write(gvc);
  }
  dotneato_terminate(gvc);
  return 1;
}
