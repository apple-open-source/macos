/*
 * tclxml.h --
 *
 *	Generic interface to XML parsers.
 *
 * Copyright (c) 1999-2003 Steve Ball, Zveno Pty Ltd
 *
 * Zveno Pty Ltd makes this software and associated documentation
 * available free of charge for any purpose.  You may make copies
 * of the software but you must include all of this notice on any copy.
 *
 * Zveno Pty Ltd does not warrant that this software is error free
 * or fit for any purpose.  Zveno Pty Ltd disclaims any liability for
 * all claims, expenses, losses, damages and costs any user may incur
 * as a result of using, copying or modifying the software.
 *
 * $Id: tclxml.h,v 1.14 2003/02/25 04:09:00 balls Exp $
 *
 */

#ifndef __TCLXML_H__
#define __TCLXML_H__

#include "tcl.h"

#define TCLXML_MAJOR_VERSION	2
#define TCLXML_MINOR_VERSION	6
#define TCLXML_RELEASE_LEVEL	TCL_RELEASE
#define TCLXML_RELEASE_SERIAL	6

#define TCLXML_VERSION		"2.6"
#define TCLXML_PATCH_LEVEL	"2.6"

/*
 * Used to block the rest of this header file from resource compilers so
 * we can just get the version info.
 */
#ifndef RC_INVOKED

/* TIP 27 update. If CONST84 is not defined we are compiling against a
 * core before 8.4 and have to disable some CONST'ness.
 */

#ifndef CONST84
#   define CONST84
#endif



/*
 * Fix the Borland bug that's in the EXTERN macro from tcl.h.
 */
#ifndef TCL_EXTERN
#   undef DLLIMPORT
#   undef DLLEXPORT
#   if defined(STATIC_BUILD)
#	define DLLIMPORT
#	define DLLEXPORT
#   elif (defined(__WIN32__) && (defined(_MSC_VER) || (__BORLANDC__ >= 0x0550) || (defined(__GNUC__) && defined(__declspec)))) || (defined(MAC_TCL) && FUNCTION_DECLSPEC)
#	define DLLIMPORT __declspec(dllimport)
#	define DLLEXPORT __declspec(dllexport)
#   elif defined(__BORLANDC__)
#	define OLDBORLAND 1
#	define DLLIMPORT __import
#	define DLLEXPORT __export
#   else
#	define DLLIMPORT
#	define DLLEXPORT
#   endif
    /* Avoid name mangling from C++ compilers. */
#   ifdef __cplusplus
#	define TCL_EXTRNC extern "C"
#   else
#	define TCL_EXTRNC extern
#   endif
    /* Pre-5.5 Borland requires the attributes be placed after the */
    /* return type. */
#   ifdef OLDBORLAND
#	define TCL_EXTERN(RTYPE) TCL_EXTRNC RTYPE TCL_STORAGE_CLASS
#   else
#	define TCL_EXTERN(RTYPE) TCL_EXTRNC TCL_STORAGE_CLASS RTYPE
#   endif
#endif



/*
 * These macros are used to control whether functions are being declared for
 * import or export in Windows, 
 * They map to no-op declarations on non-Windows systems.
 * Assumes that tcl.h defines DLLEXPORT & DLLIMPORT correctly.
 * The default build on windows is for a DLL, which causes the DLLIMPORT
 * and DLLEXPORT macros to be nonempty. To build a static library, the
 * macro STATIC_BUILD should be defined before the inclusion of tcl.h
 *
 * If a function is being declared while it is being built
 * to be included in a shared library, then it should have the DLLEXPORT
 * storage class.  If is being declared for use by a module that is going to
 * link against the shared library, then it should have the DLLIMPORT storage
 * class.  If the symbol is beind declared for a static build or for use from a
 * stub library, then the storage class should be empty.
 *
 * The convention is that a macro called BUILD_xxxx, where xxxx is the
 * name of a library we are building, is set on the compile line for sources
 * that are to be placed in the library.  When this macro is set, the
 * storage class will be set to DLLEXPORT.  At the end of the header file, the
 * storage class will be reset to DLLIMPORt.
 */

#undef TCL_STORAGE_CLASS
#ifdef BUILD_Tclxml
# define TCL_STORAGE_CLASS DLLEXPORT
#else
# ifdef USE_TCLXML_STUBS
#  define TCL_STORAGE_CLASS
# else
#  define TCL_STORAGE_CLASS DLLIMPORT
# endif
#endif


/*
 * C API for TclXML generic layer
 *
 * C callback functions to application code and their registration functions.
 * These all mimic the Tcl callbacks.
 */

typedef int (TclXML_ElementStartProc) _ANSI_ARGS_((Tcl_Interp *interp, ClientData clientData, Tcl_Obj *namePtr, Tcl_Obj *attListPtr));
typedef int (TclXML_ElementEndProc) _ANSI_ARGS_((Tcl_Interp *interp, ClientData clientData, Tcl_Obj *namePtr));
typedef int (TclXML_CharacterDataProc) _ANSI_ARGS_((Tcl_Interp *interp, ClientData clientData, Tcl_Obj *dataPtr));
typedef int (TclXML_PIProc) _ANSI_ARGS_((Tcl_Interp *interp, ClientData clientData, Tcl_Obj *targetPtr, Tcl_Obj *dataPtr));
typedef int (TclXML_DefaultProc) _ANSI_ARGS_((Tcl_Interp *interp, ClientData clientData, Tcl_Obj *dataPtr));
typedef int (TclXML_UnparsedProc) _ANSI_ARGS_((Tcl_Interp *interp, ClientData clientData, Tcl_Obj *entityPtr, Tcl_Obj *basePtr, Tcl_Obj *systemIdPtr, Tcl_Obj *publicIdPtr, Tcl_Obj *notationNamePtr));
typedef int (TclXML_NotationDeclProc) _ANSI_ARGS_((Tcl_Interp *interp, ClientData clientData, Tcl_Obj *namePtr, Tcl_Obj *basePtr, Tcl_Obj *systemIdPtr, Tcl_Obj *publicIdPtr));
typedef int (TclXML_EntityProc) _ANSI_ARGS_((Tcl_Interp *interp, ClientData clientData, Tcl_Obj *namePtr, Tcl_Obj *basePtr, Tcl_Obj *systemIdPtr, Tcl_Obj *publicIdPtr));
typedef int (TclXML_UnknownEncodingProc) _ANSI_ARGS_((Tcl_Interp *interp, ClientData clientData, Tcl_Obj *dataPtr, void *info));
typedef int (TclXML_CommentProc) _ANSI_ARGS_((Tcl_Interp *interp, ClientData clientData, Tcl_Obj *dataPtr));
typedef int (TclXML_NotStandaloneProc) _ANSI_ARGS_((Tcl_Interp *interp, ClientData clientData));
typedef int (TclXML_ElementDeclProc) _ANSI_ARGS_((Tcl_Interp *interp, ClientData clientData, Tcl_Obj *namePtr, Tcl_Obj *contentspecPtr));
typedef int (TclXML_AttlistDeclProc) _ANSI_ARGS_((Tcl_Interp *interp, ClientData clientData, Tcl_Obj *elementnamePtr, Tcl_Obj *attrdefnsPtr));
typedef int (TclXML_StartDoctypeDeclProc) _ANSI_ARGS_((Tcl_Interp *interp, ClientData clientData, Tcl_Obj *namePtr));
typedef int (TclXML_EndDoctypeDeclProc) _ANSI_ARGS_((Tcl_Interp *interp, ClientData clientData));

/*
 * The structure below is used to refer to a parser object.
 */

typedef struct TclXML_Info {
    Tcl_Interp *interp;		/* Interpreter for this instance */
    Tcl_Obj *name;		/* name of this instance */

    Tcl_Obj *base;		/* base URI for document entity */

    void *parserClass;		/* Parser-specific functions
				 * Actually of type TclXML_ParserClassInfo
				 */
    ClientData clientData;	/* Parser-specific data structure */
    
    int final;			/* input data complete? */
    int validate;		/* Validate document? */
    
    int status;			/* application status */
    Tcl_Obj *result;		/* application return result */
    
    int continueCount;		/* reference count for continue */
    Tcl_Obj *context;           /* reference to the context pointer */

    Tcl_Obj *cdata;		/* Accumulates character data */
    int nowhitespace;		/* Whether to ignore white space */
    int reportempty;		/* Whether to report empty elements */
    int expandinternalentities;	/* Whether to expand internal entities */
    int paramentities;		/* Whether to include parameter entities */

    Tcl_Obj *elementstartcommand;	/* Script for element start */
    TclXML_ElementStartProc *elementstart;	/* Callback for element start */
    ClientData elementstartdata;
    Tcl_Obj *elementendcommand;	        /* Script for element end */
    TclXML_ElementEndProc *elementend;	/* Callback for element end */
    ClientData elementenddata;
    Tcl_Obj *datacommand;	        /* Script for character data */
    TclXML_CharacterDataProc *cdatacb;	/* Callback for character data */
    ClientData cdatacbdata;
    Tcl_Obj *picommand;		        /* Script for processing instruction */
    TclXML_PIProc *pi;			/* Callback for processing instruction */
    ClientData pidata;
    Tcl_Obj *defaultcommand;	        /* Script for default data */
    TclXML_DefaultProc *defaultcb;		/* Callback for default data */
    ClientData defaultdata;
    Tcl_Obj *unparsedcommand;		/* Script for unparsed entity declaration */
    TclXML_UnparsedProc *unparsed;	/* Callback for unparsed entity declaraion */
    ClientData unparseddata;
    Tcl_Obj *notationcommand;		/* Script for notation declaration */
    TclXML_NotationDeclProc *notation;	/* Callback for notation declaraion */
    ClientData notationdata;
    Tcl_Obj *entitycommand;		/* Script for external entity */
    TclXML_EntityProc *entity;		/* Callback for external entity */
    ClientData entitydata;
    Tcl_Obj *unknownencodingcommand;	/* Script for unknown encoding */
    TclXML_UnknownEncodingProc *unknownencoding;	/* Callback for unknown encoding */
    ClientData unknownencodingdata;
    /* Following added by ericm@scriptics */
    Tcl_Obj *commentCommand;		/* Script for comments */
    TclXML_CommentProc *comment;		/* Callback for comments */
    ClientData commentdata;
    Tcl_Obj *notStandaloneCommand;	/* Script for "not standalone" docs */
    TclXML_NotStandaloneProc *notStandalone;	/* Callback for "not standalone" docs */
    ClientData notstandalonedata;

    Tcl_Obj *elementDeclCommand;	/* Script for <!ELEMENT decl's */
    TclXML_ElementDeclProc *elementDecl;	/* Callback for element declaration */
    ClientData elementdecldata;
    Tcl_Obj *attlistDeclCommand;	/* Script for <!ATTLIST decl's */
    TclXML_AttlistDeclProc *attlistDecl;	/* Callback for attribute list declaration */
    ClientData attlistdecldata;

  /* Do we really need these? */
    Tcl_Obj *startDoctypeDeclCommand;	/* Script for <!DOCTYPE decl's */
    TclXML_StartDoctypeDeclProc *startDoctypeDecl;	/* Callback for document type declaration start */
    ClientData startdoctypedecldata;
    Tcl_Obj *endDoctypeDeclCommand;    /* Script for <!DOCTYPE decl ends */
    TclXML_EndDoctypeDeclProc *endDoctypeDecl;	/* Callback for document type declaration start */
    ClientData enddoctypedecldata;

} TclXML_Info;

/*
 * These function definitions are provided by a parser
 * implementation and registered with this module.
 */

typedef ClientData (TclXML_CreateProc) _ANSI_ARGS_((Tcl_Interp *interp, TclXML_Info *xmlinfo));
typedef ClientData (TclXML_CreateEntityParserProc) _ANSI_ARGS_((Tcl_Interp *interp, ClientData clientData));
typedef int (TclXML_ParseProc) _ANSI_ARGS_((ClientData clientData, char *buffer, int len, int final));
typedef int (TclXML_ConfigureProc) _ANSI_ARGS_((ClientData clientData, int objc, Tcl_Obj *CONST objv[]));
typedef int (TclXML_GetProc) _ANSI_ARGS_((ClientData clientData, int objc, Tcl_Obj *CONST objv[]));
typedef int (TclXML_ResetProc) _ANSI_ARGS_((ClientData clientData));
typedef int (TclXML_DeleteProc) _ANSI_ARGS_((ClientData clientData));

/*
 * The structure below is used store function pointers 
 * for a parser implementation.
 */

typedef struct TclXML_ParserClassInfo {
  Tcl_Obj *name;

  TclXML_CreateProc *create;	/* Direct-call creation proc */
  Tcl_Obj *createCmd;	/* Tcl command creation script */

  TclXML_CreateEntityParserProc *createEntity;
  Tcl_Obj *createEntityCmd;

  TclXML_ParseProc *parse;
  Tcl_Obj *parseCmd;

  TclXML_ConfigureProc *configure;
  Tcl_Obj *configureCmd;

  TclXML_GetProc *get;
  Tcl_Obj *getCmd;
  
  TclXML_ResetProc *reset;
  Tcl_Obj *resetCmd;

  TclXML_DeleteProc *destroy;
  Tcl_Obj *destroyCmd;

} TclXML_ParserClassInfo;

/*
 *----------------------------------------------------------------------------
 *
 * Function prototypes for publically accessible routines
 *
 *----------------------------------------------------------------------------
 */

#include "tclxmlDecls.h"

#ifdef USE_TCLXML_STUBS
TCL_EXTRNC CONST char *
    TclXML_InitStubs _ANSI_ARGS_((Tcl_Interp *interp, CONST char *version, int exact));
#endif

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#endif /* RC_INVOKED */
#endif /* __TCLXML_H__ */
