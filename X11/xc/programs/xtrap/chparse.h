/* $XFree86: xc/programs/xtrap/chparse.h,v 1.1 2001/11/02 23:29:34 dawes Exp $ */

extern int chparse(int max_delay, int rest_delay, int *state, int *private,
		   int param[], int *nparam, int inter[], int *ninter,
		   int *final);
extern void dumpsequence(int state, int c, int private, int param[],
			int nparam, int inter[], int ninter, int final,
			short *column);


