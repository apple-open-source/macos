#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"


MODULE = Readonly::XS		PACKAGE = Readonly::XS		

int
is_sv_readonly(sv)
    SV *sv
PROTOTYPE: $
CODE:
    RETVAL = SvREADONLY(sv);
OUTPUT:
    RETVAL

void
make_sv_readonly(sv)
    SV *sv
PROTOTYPE: $
CODE:
    SvREADONLY_on(sv);
