/* exp_closetcl.c - close tcl files */

/* isolated in it's own file since it has hooks into Tcl and exp_clib user */
/* might like to avoid dragging it in */

#include "expect_cf.h"
#include "tclInt.h"

void (*exp_close_in_child)() = 0;

void
exp_close_tcl_files() {
	int i;

	/* So much for close-on-exec.  Tcl doesn't mark its files that way */
	/* everything has to be closed explicitly. */

#if 0
/* Not necessary with Tcl 7.5? */
	for (i=3; i<tclNumFiles;i++) close(i);
#endif
}
