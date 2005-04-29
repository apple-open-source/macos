/* this dummy file is here to fool libtool and automake */
#include "tk.h"

/* need to reference an entry point to fool Sun's ld */
void tkstubs_dummy() {Tk_InitStubs(0, 0, 0);}
