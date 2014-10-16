/**************************************************************
 *
 *	curvegen.c
 *
 *	CM curve generator.
 *
 *  Compile with:
 *
 *  % cc -O curvegen.c tools.c giants.c ellproj.c -lm -o curvegen
 *
 *	Updates:
 *		27 Sep 98    REC - Creation
 *
 *
 *	c. 1998 Perfectly Scientific, Inc.
 *	All Rights Reserved.
 *
 *
 *************************************************************/

/* include files */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32 

#include <process.h>

#endif

#include <string.h>
#include "giants.h"
#include "tools.h"

#define DCOUNT 27

int disc12[DCOUNT] =  {-3, -4, -7, -8, -11, -19, -43, -67, -163, -15, -20, -24, -35, -40, -51, -52, -88, -91, -115, -123, -148, -187, -232, -235, -267, -403, -427}; /* All discriminants of class number 1,2. */

/**************************************************************
 *
 *	Main Function
 *
 **************************************************************/

#define CM_SHORTS 4096

main(int argc, char **argv) {
    giant p = newgiant(CM_SHORTS);
	giant u = newgiant(CM_SHORTS);
	giant v = newgiant(CM_SHORTS);
	giant g[6];
    giant plus_order = newgiant(CM_SHORTS);
    giant minus_order = newgiant(CM_SHORTS);
	giant a = newgiant(CM_SHORTS);
    giant b = newgiant(CM_SHORTS);
    int d, dc, olen, k;

    init_tools(CM_SHORTS);    /* Basic algorithms. */
    printf("Give base prime p:\n"); fflush(stdout);
    gin(p);
    for(dc=0; dc < 6; dc++) g[dc] = newgiant(CM_SHORTS);
    for(dc = 0; dc < DCOUNT; dc++) {
			d = disc12[dc];
			/* Next, seek representation 4N = u^2 + |d| v^2. */
			if(cornacchia4(p, d, u, v) == 0) continue;
/* Here, (u,v) give the quadratic representation of 4p. */
			printf("D: %d\n", d); fflush(stdout);
			gtog(u, g[0]);
			switch(d) {
				case -3: olen = 3;  /* Six orders: p + 1 +- g[0,1,2]. */
						gtog(u, g[1]); gtog(v, g[2]);
						addg(g[2], g[2]); addg(v, g[2]); /* g[2] := 3v. */
						addg(g[2], g[1]); gshiftright(1, g[1]);  /* g[1] = (u + 3v)/2. */
						subg(u, g[2]); gshiftright(1, g[2]); absg(g[2]); /* g[2] = |u-3v|/2. */
						break;
				case -4: olen = 2;  /* Four orders: p + 1 +- g[0,1]. */
						gtog(v, g[1]); addg(g[1], g[1]); /* g[1] = 2v. */
						break;
				default: olen = 1;  /* Two orders: p + 1 +- g[0]. */
			}
			for(k=0; k < olen; k++) {
				 gtog(p, plus_order); iaddg(1, plus_order);
				 gtog(p, minus_order); iaddg(1, minus_order);
				 addg(g[k], plus_order);
				 subg(g[k], minus_order);
				 printf("curve orders: \n");
				 printf("(%d) ", prime_probable(plus_order));
                 gout(plus_order);
				 printf("(%d) ", prime_probable(minus_order));
				 gout(minus_order);
			}
   }
}










