# tdom.decls --
#
#	This file contains the declarations for all supported public
#	functions that are exported by the tDOM library via the stubs table.
#
# Copyright (c) 2002 Rolf Ade.

library tdom
interface tdom
#hooks {}

declare 0 generic {
    int TclExpatObjCmd (ClientData dummy, Tcl_Interp *interp, 
                        int objc, Tcl_Obj *CONST objv[])
}
declare 1 generic {
    int CheckExpatParserObj (Tcl_Interp *interp, Tcl_Obj *CONST nameObj)
}
declare 2 generic {
     int CHandlerSetInstall (Tcl_Interp *interp, Tcl_Obj *CONST expatObj,
                             CHandlerSet *handlerSet)
}
declare 3 generic {
     int CHandlerSetRemove (Tcl_Interp *interp, Tcl_Obj *CONST expatObj,
                            char *handlerSetName)
}
declare 4 generic {
     CHandlerSet * CHandlerSetCreate (char *name)
}
declare 5 generic {
     CHandlerSet * CHandlerSetGet (Tcl_Interp *interp, Tcl_Obj *CONST expatObj,
                                   char *handlerSetName)
}
declare 6 generic {
     void * CHandlerSetGetUserData (Tcl_Interp *interp, 
                                    Tcl_Obj *CONST expatObj,
                                    char *handlerSetName)
}
declare 7 generic {
     TclGenExpatInfo * GetExpatInfo (Tcl_Interp *interp,
                                     Tcl_Obj *CONST expatObj)
}
declare 8 generic {
     int XML_GetCurrentLineNumber(XML_Parser parser)
}
declare 9 generic {
     int XML_GetCurrentColumnNumber(XML_Parser parser)
}
declare 10 generic {
     long XML_GetCurrentByteIndex(XML_Parser parser)
}
declare 11 generic {
     int XML_GetCurrentByteCount(XML_Parser parser)
}
declare 12 generic {
     enum XML_Status XML_SetBase(XML_Parser parser, const XML_Char *base)
}
declare 13 generic {
     const XML_Char * XML_GetBase(XML_Parser parser)
}
declare 14 generic {
     int XML_GetSpecifiedAttributeCount(XML_Parser parser)
}
declare 15 generic {
     int XML_GetIdAttributeIndex(XML_Parser parser)
}
declare 16 generic {
     domNode * tcldom_getNodeFromName(Tcl_Interp *interp, char *nodeName, 
                                      char **errMsg)
}
declare 17 generic {
     domDocument * tcldom_getDocumentFromName (Tcl_Interp  *interp, 
                                               char *docName, char **errMsg)
}

