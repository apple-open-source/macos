#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

MODULE = Devel::GlobalDestruction PACKAGE = Devel::GlobalDestruction

PROTOTYPES: ENABLE

I32
in_global_destruction()
	PROTOTYPE:
	CODE:
		RETVAL = PL_dirty;
	OUTPUT:
		RETVAL
