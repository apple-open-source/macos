#include "tcl.h"
#include "dot.h"

typedef struct tkgendata_s {
	Tcl_Interp	*interp;
	char		*canvas;
	int			eval;
} tkgendata_t;

#ifndef CONST84
#define CONST84
#endif
