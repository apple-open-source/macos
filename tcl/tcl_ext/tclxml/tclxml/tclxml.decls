# tclxml.decls --
#
# This file contains the declarations for all supported public functions
# that are exported by the TCLXML library via the stubs table. This file
# is used to generate the tclxmlDecls.h/tclxmlStubsLib.c/tclxmlStubsInit.c
# files.
#	

# Declare each of the functions in the public BLT interface.  Note that
# the an index should never be reused for a different function in order
# to preserve backwards compatibility.

library tclxml

# Define the TCLXML interface:

interface tclxml
#hooks {}

declare 0 generic {
    int Tclxml_Init(Tcl_Interp *interp)
}
declare 1 generic {
    int Tclxml_SafeInit(Tcl_Interp *interp)
}

#########################################################################
###  Parser registration API

declare 2 generic {
    int TclXML_RegisterXMLParser (Tcl_Interp *interp, \
	TclXML_ParserClassInfo *parser)
}

#########################################################################
###  Application callback C API.
###  These are equivalent to the Tcl API, and may be used in conjunction.

declare 3 generic {
    int TclXML_RegisterElementStartProc (Tcl_Interp *interp, \
	TclXML_Info *parser, ClientData clientData, \
	TclXML_ElementStartProc *callback)
}
declare 4 generic {
    int TclXML_RegisterElementEndProc (Tcl_Interp *interp, \
	TclXML_Info *parser, ClientData clientData, \
	TclXML_ElementEndProc *callback)
}
declare 5 generic {
    int TclXML_RegisterCharacterDataProc (Tcl_Interp *interp, \
	TclXML_Info *parser, ClientData clientData, \
	TclXML_CharacterDataProc *callback)
}
declare 6 generic {
    int TclXML_RegisterPIProc (Tcl_Interp *interp, TclXML_Info *parser, \
	ClientData clientData, TclXML_PIProc *callback)
}
declare 7 generic {
    int TclXML_RegisterDefaultProc (Tcl_Interp *interp, \
	TclXML_Info *parser, ClientData clientData, \
	TclXML_DefaultProc *callback)
}
declare 8 generic {
    int TclXML_RegisterUnparsedProc (Tcl_Interp *interp, \
	TclXML_Info *parser, ClientData clientData, \
	TclXML_UnparsedProc *callback)
}
declare 9 generic {
    int TclXML_RegisterNotationDeclProc (Tcl_Interp *interp, \
	TclXML_Info *parser, ClientData clientData, \
	TclXML_NotationDeclProc *callback)
}
declare 10 generic {
    int TclXML_RegisterEntityProc (Tcl_Interp *interp, TclXML_Info *parser, \
	ClientData clientData, TclXML_EntityProc *callback)
}
declare 11 generic {
    int TclXML_RegisterUnknownEncodingProc (Tcl_Interp *interp, \
	TclXML_Info *parser, ClientData clientData, \
	TclXML_UnknownEncodingProc *callback)
}
declare 12 generic {
    int TclXML_RegisterCommentProc (Tcl_Interp *interp, TclXML_Info *parser, \
	ClientData clientData, TclXML_CommentProc *callback)
}
declare 13 generic {
    int TclXML_RegisterNotStandaloneProc (Tcl_Interp *interp, \
	TclXML_Info *parser, ClientData clientData, \
	TclXML_NotStandaloneProc *callback)
}
declare 14 generic {
    int TclXML_RegisterElementDeclProc (Tcl_Interp *interp, \
	TclXML_Info *parser, ClientData clientData, \
	TclXML_ElementDeclProc *callback)
}
declare 15 generic {
    int TclXML_RegisterAttListDeclProc (Tcl_Interp *interp, \
	TclXML_Info *parser, ClientData clientData, \
	TclXML_AttlistDeclProc *callback)
}
declare 16 generic {
    int TclXML_RegisterStartDoctypeDeclProc (Tcl_Interp *interp, \
	TclXML_Info *parser, ClientData clientData, \
	TclXML_StartDoctypeDeclProc *callback)
}
declare 17 generic {
    int TclXML_RegisterEndDoctypeDeclProc (Tcl_Interp *interp, \
	TclXML_Info *parser, ClientData clientData, \
	TclXML_EndDoctypeDeclProc *callback)
}

#########################################################################
###  Call-ins for parser class implementations.
###  A parser implementation calls these functions.
###  The generic layer then invokes application callbacks
###  that may be defined.

declare 18 generic {
    void TclXML_ElementStartHandler (void *userdata, Tcl_Obj *name, \
	Tcl_Obj *atts)
}
declare 19 generic {
    void TclXML_ElementEndHandler (void	*userData, Tcl_Obj *name)
}
declare 20 generic {
    void TclXML_CharacterDataHandler (void *userData, Tcl_Obj *s)
}
declare 21 generic {
    void TclXML_ProcessingInstructionHandler (void *userData, \
	Tcl_Obj *target, Tcl_Obj *data)
}
declare 22 generic {
    int TclXML_ExternalEntityRefHandler (ClientData clientData, \
	Tcl_Obj *openEntityNames, Tcl_Obj *base, Tcl_Obj *systemId, \
	Tcl_Obj *publicId)
}
declare 23 generic {
    void TclXML_DefaultHandler (void *userData, Tcl_Obj *s)
}
declare 24 generic {
    void TclXML_UnparsedDeclHandler (void *userData, Tcl_Obj *entityname, \
	Tcl_Obj *base, Tcl_Obj *systemId, Tcl_Obj *publicId, \
	Tcl_Obj *notationName)
}
declare 25 generic {
    void TclXML_NotationDeclHandler (void *userData, Tcl_Obj *notationName, \
	Tcl_Obj *base, Tcl_Obj *systemId, Tcl_Obj *publicId)
}
declare 26 generic {
    int TclXML_UnknownEncodingHandler (void *encodingHandlerData, \
	Tcl_Obj *name, void *info)
}

#########################################################################
###  Following added by ericm@scriptics, 1999.6.25

###  Prototype definition for the comment handler
declare 27 generic {
    void TclXML_CommentHandler (void *userData, Tcl_Obj *data)
}

###  Prototype for Not Standalone Handler
declare 28 generic {
    int	TclXML_NotStandaloneHandler (void *userData)
}

#########################################################################
###  Added by ericm@scriptics.com, 1999.09.13

###  Prototype for (Element|Attlist) Declaration Handlers
declare 31 generic {
    void TclXML_ElementDeclHandler (void *userData, Tcl_Obj *name, \
	Tcl_Obj *contentspec)
}
declare 32 generic {
    void TclXML_AttlistDeclHandler (void *userData, Tcl_Obj *name, \
	Tcl_Obj *attributes)
}

###  Prototypes for the Doctype Decl handlers
declare 33 generic {
    void TclXML_StartDoctypeDeclHandler (void *userData, Tcl_Obj *name)
}
declare 34 generic {
    void TclXML_EndDoctypeDeclHandler (void *userData)
}
