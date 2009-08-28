# tcldom.decls --
#
# This file contains the declarations for all supported public
# functions that are exported by the TCLDOM/lixml2 library via the
# stubs table. This file is used to generate the tcldomxmlDecls.h /
# tcldomxmlStubsLib.c / tcldomxmlStubsInit.c files.
#

# Declare each of the functions in the public TclDOM/libxml2
# interface.  Note that the an index should never be reused for a
# different function in order to preserve backwards compatibility.

library tcldomxml

# Define the TCLDOM/libxml2 interface:

interface tcldomxml
#hooks {}

declare 0 generic {
    int Tcldomxml_Init(Tcl_Interp *interp)
}
if 0 {
    ## No SafeInit declared by package.
    declare 1 generic {
	int Tcldomxml_SafeInit(Tcl_Interp *interp)
    }
}

#########################################################################
###  Accessor API

declare 2 generic {
    Tcl_Obj * TclDOM_NewDoc (Tcl_Interp *interp)
}
declare 3 generic {
    Tcl_Obj * TclDOM_CreateObjFromNode (xmlNodePtr nodePtr)
}
declare 4 generic {
    Tcl_Obj * TclDOM_CreateObjFromDoc  (xmlDocPtr  docPtr)
}
declare 5 generic {
    int TclDOM_GetNodeFromObj (Tcl_Interp *interp, Tcl_Obj *objPtr, xmlNodePtr *nodePtrPtr)
}
declare 6 generic {
    int TclDOM_GetDocFromObj  (Tcl_Interp *interp, Tcl_Obj *objPtr, xmlDocPtr *docPtrPtr)
}
#########################################################################
