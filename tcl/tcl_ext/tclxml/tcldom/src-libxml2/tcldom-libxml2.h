/* tcldom-libxml2.h --
 *
 *	libxml2 wrapper for TclDOM.
 *
 * Copyright (c) 2002 Zveno Pty Ltd
 * http://www.zveno.com/
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
 * $Id: tcldom-libxml2.h,v 1.8 2002/10/31 23:40:22 andreas_kupries Exp $
 */

#ifndef __TCLDOM_LIBXML2_H__
#define __TCLDOM_LIBXML2_H__

#include <tcldom.h>
#include <libxml/tree.h>

/*
 * For C++ compilers, use extern "C"
 */

#ifdef __cplusplus
extern "C" {
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
#ifdef BUILD_Tcldomxml
# define TCL_STORAGE_CLASS DLLEXPORT
#else
# ifdef USE_TCL_STUBS
#  define TCL_STORAGE_CLASS
# else
#  define TCL_STORAGE_CLASS DLLIMPORT
# endif
#endif

EXTERN Tcl_FreeInternalRepProc	TclDOM_DocFree;
EXTERN Tcl_DupInternalRepProc	TclDOM_DocDup;
EXTERN Tcl_UpdateStringProc	TclDOM_DocUpdate;
EXTERN Tcl_SetFromAnyProc	TclDOM_DocSetFromAny;

EXTERN Tcl_FreeInternalRepProc	TclDOM_NodeFree;
EXTERN Tcl_DupInternalRepProc	TclDOM_NodeDup;
EXTERN Tcl_UpdateStringProc	TclDOM_NodeUpdate;
EXTERN Tcl_SetFromAnyProc	TclDOM_NodeSetFromAny;

/*
 * Object types
 */

EXTERN Tcl_ObjType TclDOM_DocObjType;
EXTERN Tcl_ObjType TclDOM_NodeObjType;

/*
 * The following function is required to be defined in all stubs aware
 * extensions of TclDOM.  The function is actually implemented in the stub
 * library, not the main Tcldom library, although there is a trivial
 * implementation in the main library in case an extension is statically
 * linked into an application.
 */

EXTERN CONST char *	Tcldomxml_InitStubs _ANSI_ARGS_((Tcl_Interp *interp,
			    CONST char *version, int exact));

#ifndef USE_TCLDOMXML_STUBS

/*
 * When not using stubs, make it a macro.
 */

#define Tcldomxml_InitStubs(interp, version, exact) \
    Tcl_PkgRequire(interp, "dom::generic", version, exact)

#endif

/*
 * Accessor functions => Stubs
 */

#include "tcldomxmlDecls.h"

#undef  TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#ifdef __cplusplus
}
#endif

#endif /* TCLDOM_LIBXML2_H__ */
