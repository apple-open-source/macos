# tcldom.decls --
#
# This file contains the declarations for all supported public functions
# that are exported by the TCLDOM library via the stubs table. This file
# is used to generate the tcldomDecls.h/tcldomStubsLib.c/tcldomStubsInit.c
# files.
#	

# Declare each of the functions in the public BLT interface.  Note that
# the an index should never be reused for a different function in order
# to preserve backwards compatibility.

library tcldom

# Define the TCLDOM interface:

interface tcldom
#hooks {}
if 0 {
declare 0 generic {
    int Tcldom_Init(Tcl_Interp *interp)
}}
if 0 {
    ## No SafeInit declared by package.
    declare 1 generic {
	int Tcldom_SafeInit(Tcl_Interp *interp)
    }
}

#########################################################################
###  Implementation registration API

declare 2 generic {
    int TclDOM_Register (Tcl_Interp *interp, \
	TclDOM_Implementation *impl)
}

#########################################################################
