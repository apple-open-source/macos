#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"


MODULE = XSTest		PACKAGE = XSTest		

int
is_even(input)
	int     input
    CODE:
	RETVAL = (input % 2 == 0);
    OUTPUT:
	RETVAL
