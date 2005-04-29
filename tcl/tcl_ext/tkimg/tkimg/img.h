/* img.h */

#ifndef _IMG
#define _IMG

#include "tcl.h"
#include "tk.h"

#define IMG_MAJOR_VERSION 1
#define IMG_MINOR_VERSION 2
#define IMG_RELEASE_LEVEL 1
#define IMG_RELEASE_SERIAL 2

#define IMG_VERSION "1.2"
#define IMG_PATCH_LEVEL "1.2.4"

#ifndef RESOURCE_INCLUDED

#undef TCL_STORAGE_CLASS
#ifdef BUILD_img
#   define TCL_STORAGE_CLASS DLLEXPORT
#else
#   ifdef USE_TCL_STUBS
#	define TCL_STORAGE_CLASS
#   else
#	define TCL_STORAGE_CLASS DLLIMPORT
#   endif
#endif

/*
 * Fix the Borland bug that's in the EXTERN macro from tcl.h.
 * - ak - Using the definition from Incr Tcl now -
 */

#if defined(__WIN32__)
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   undef WIN32_LEAN_AND_MEAN
#endif

#ifndef TCL_EXTERN
#   undef DLLIMPORT
#   undef DLLEXPORT
#   if (defined(__WIN32__) && (defined(_MSC_VER) || (__BORLANDC__ >= 0x0550) || (defined(__GNUC__) && defined(__declspec)))) \
	    || (defined(MAC_TCL) && FUNCTION_DECLSPEC)
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
#   ifdef STATIC_BUILD
#       define DLLIMPORT
#       define DLLEXPORT
#   endif
    /*
     * Make sure name mangling won't happen when the c++ language extensions
     * are used.
     */
#   ifdef __cplusplus
#	define TCL_CPP "C"
#   else
#	define TCL_CPP
#   endif
    /*
     * Borland requires the attributes be placed after the return type.
     */
#   ifdef OLDBORLAND
#	define TCL_EXTERN(rtnType) extern TCL_CPP rtnType TCL_STORAGE_CLASS
#   else
#	define TCL_EXTERN(rtnType) extern TCL_CPP TCL_STORAGE_CLASS rtnType
#   endif
#endif


TCL_EXTERN(int) Img_Init _ANSI_ARGS_((Tcl_Interp *interp));
TCL_EXTERN(int) Img_SafeInit _ANSI_ARGS_((Tcl_Interp *interp));

#endif /* RESOURCE_INCLUDED */

#endif /* _IMG */
