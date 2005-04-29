/* this dummy file is here to fool libtool and automake */
#include "tcl.h"

/* need to reference an entry point to fool Sun's ld */
void tclstubs_dummy() {Tcl_InitStubs(0, 0, 0);}
