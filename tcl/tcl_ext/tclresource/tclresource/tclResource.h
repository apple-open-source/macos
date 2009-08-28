// -------------------------------------------------------
// File: "tclResource.h"
//                        Created: 2003-09-20 10:13:07
//              Last modification: 2005-12-27 17:12:12
// Author: Bernard Desgraupes
// e-mail: <bdesgraupes@users.sourceforge.net>
// www: <http://sourceforge.net/projects/tclresource>
// (c) Copyright : Bernard Desgraupes, 2003-2004, 2005
// All rights reserved.
// -------------------------------------------------------

#ifndef _TCLRESOURCE_H
#define _TCLRESOURCE_H

#define TARGET_API_MAC_CARBON	1
#define TARGET_API_MAC_OSX		1

// Stubs mechanism enabled
#define USE_TCL_STUBS

#include <Carbon/Carbon.h>

struct SFReply {char dummy;};
typedef struct SFReply SFReply;
typedef struct SFReply StandardFileReply;


#if TARGET_RT_MAC_MACHO
	#ifdef MAC_TCL
		#undef MAC_TCL
	#endif
#endif

#include <Tcl/tcl.h>
#include <Tcl/tclInt.h>

#ifndef CONST84 // Tcl 8.4 backwards compatibility
#      define CONST84 
#      define CONST84_RETURN CONST
#endif


#endif // _TCLRESOURCE_H
