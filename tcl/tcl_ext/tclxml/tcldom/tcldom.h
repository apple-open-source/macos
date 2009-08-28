/* tcldom.h --
 *
 *	Generic layer of TclDOM API.
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
 * $Id: tcldom.h,v 1.7 2002/10/31 23:40:07 andreas_kupries Exp $
 */

#ifndef __TCLDOM_H__
#define __TCLDOM_H__

#include <tcl.h>

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
#ifdef BUILD_tcldom
# define TCL_STORAGE_CLASS DLLEXPORT
#else
# ifdef USE_TCL_STUBS
#  define TCL_STORAGE_CLASS
# else
#  define TCL_STORAGE_CLASS DLLIMPORT
# endif
#endif

/*
 * C API for TclDOM generic layer
 *
 * C callback functions to application code and their registration functions.
 * These all mimic the Tcl callbacks.
 */

/*
 * The structure below is used to refer to a DOM Implementation.
 */

typedef struct TclDOM_Implementation {
  Tcl_Obj *name;		/* Name of this implementation */
  Tcl_ObjType *type;		/* Object type pointer of this impl */

  Tcl_ObjCmdProc *create;
  Tcl_ObjCmdProc *parse;
  Tcl_ObjCmdProc *serialize;
  Tcl_ObjCmdProc *document;
  Tcl_ObjCmdProc *documentfragment;
  Tcl_ObjCmdProc *node;
  Tcl_ObjCmdProc *element;
  Tcl_ObjCmdProc *select;
} TclDOM_Implementation;

/*
 * The following function is required to be defined in all stubs aware
 * extensions of TclDOM.  The function is actually implemented in the stub
 * library, not the main Tcldom library, although there is a trivial
 * implementation in the main library in case an extension is statically
 * linked into an application.
 */

EXTERN CONST char *	Tcldom_InitStubs _ANSI_ARGS_((Tcl_Interp *interp,
			    CONST char *version, int exact));

#ifndef USE_TCLDOM_STUBS

/*
 * When not using stubs, make it a macro.
 */

#define Tcldom_InitStubs(interp, version, exact) \
    Tcl_PkgRequire(interp, "dom::generic", version, exact)

#endif

/*
 *----------------------------------------------------------------------------
 *
 * Function prototypes for publically accessible routines
 *
 *----------------------------------------------------------------------------
 */

#include "tcldomDecls.h"

#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT

#ifdef __cplusplus
}
#endif

#endif /* __TCLDOM_H__ */
