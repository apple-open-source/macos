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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include    "fdp.h"
#include    "options.h"
#include    "tlayout.h"
#include        <time.h>
#ifndef MSWIN32
#include        <unistd.h>
#endif

char *Info[] = {
    "fdp",              /* Program */
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

int
main (int argc, char** argv)
{
	Agraph_t *g;
	
	gvc = gvNEWcontext(Info, username());

	argc = fdp_doArgs (argc, argv);
	dotneato_initialize (gvc, argc, argv);
  
#ifndef MSWIN32
	signal (SIGUSR1, toggle);
	signal (SIGINT, intr);
#endif

  {
    #define NUMNODES 5
  
    Agnode_t *node[NUMNODES];
    char name[10];
    int j, k;
    int count = 0;
  
    while (1) {
  
      /* Create a new graph */
      g = agopen("new_graph", AGDIGRAPH);

      /* Add nodes */
      for (j=0; j<NUMNODES; j++) {
        sprintf(name, "%d", j);
        node[j] = agnode(g, name);
      }
    
      /* Connect nodes */
      for (j=0; j<NUMNODES; j++) {
        for (k=j+1; k<NUMNODES; k++) {
          agedge(g,node[j],node[k]);
        }
      }
    
      /* Bind graph to layout and rendering context */
      gvBindContext(gvc, g);
    
      /* Perform layout */
      fdp_layout(g);
  
      /* Delete layout */
      fdp_cleanup(g);
  
      /* Delete graph */
      agclose(g);

      /* first log is baseline, second log shows leaks. */
#ifdef DMALLOC
      dmalloc_log_unfreed();
      if (count) return 1;
#endif
      count++;
    }
  }
}

