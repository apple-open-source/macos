/*
 * pngtcl.h --
 *
 *	Interface to libpng.
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
 * $Id: pngtcl.h 286 2010-07-07 11:08:08Z nijtmans $
 *
 */

#ifndef __PNGTCL_H__
#define __PNGTCL_H__

#include <tcl.h>

#define PNGTCL_MAJOR_VERSION	1
#define PNGTCL_MINOR_VERSION	4
#define PNGTCL_RELEASE_LEVEL	TCL_RELEASE
#define PNGTCL_RELEASE_SERIAL	3

#define PNGTCL_VERSION		"1.4.3"
#define PNGTCL_PATCH_LEVEL	"1.4.3"

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
 * C API for Pngtcl generic layer
 *----------------------------------------------------------------------------
 */

/*
 *----------------------------------------------------------------------------
 * Function prototypes for publically accessible routines
 *----------------------------------------------------------------------------
 */

#include "pngtclDecls.h"

/*
 *----------------------------------------------------------------------------
 * Function prototypes for stub initialization.
 *----------------------------------------------------------------------------
 */

const char *
Pngtcl_InitStubs(Tcl_Interp *interp, const char *version, int exact);

#endif /* RC_INVOKED */
#endif /* __PNGTCL_H__ */
