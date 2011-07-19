/*
 * tifftcl.h --
 *
 *	Interface to libtiff.
 *
 * Copyright (c) 2002-2004 Andreas Kupries <andreas_kupries@users.sourceforge.net>
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
 * $Id: tifftcl.h 279 2010-06-30 15:03:06Z nijtmans $
 *
 */

#ifndef __TIFFTCL_H__
#define __TIFFTCL_H__

#include <tcl.h>

#define TIFFTCL_MAJOR_VERSION	3
#define TIFFTCL_MINOR_VERSION	9
#define TIFFTCL_RELEASE_LEVEL	TCL_RELEASE
#define TIFFTCL_RELEASE_SERIAL	4

#define TIFFTCL_VERSION		"3.9.4"
#define TIFFTCL_PATCH_LEVEL	"3.9.4"

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
 *----------------------------------------------------------------------------
 * C API for Tifftcl generic layer
 *----------------------------------------------------------------------------
 */

/*
 *----------------------------------------------------------------------------
 * Function prototypes for publicly accessible routines
 *----------------------------------------------------------------------------
 */

#include "tifftclDecls.h"

/*
 *----------------------------------------------------------------------------
 * Function prototypes for stub initialization.
 *----------------------------------------------------------------------------
 */

const char *
Tifftcl_InitStubs(Tcl_Interp *interp, const char *version, int exact);

#endif /* RC_INVOKED */
#endif /* __TIFFTCL_H__ */
