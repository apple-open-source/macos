/*
CarbonÅ is currently under development and is not yet complete. Any comments
related to Carbon are preliminary, and you should confirm that any workarounds
we (Metrowerks) implement do work in the latest shipping version of Carbon.

If we (Metrowerks) are using an API from the CarbonLib, its version and date
that version was current will be identified in a comment using the following
format:

	// Carbon API from CarbonLib vX.X, version current as of MM/DD/YY

If we (Metrowerks) are working around the CarbonLib for whatever reason, the
version of the CarbonLib we are working around and the date at which that
version of the CarbonLib was current will be identified in the comment using
the following format:

	// CarbonLib workaround for CarbonLib vX.X, version current as of MM/DD/YY
*/

// ===========================================================================
//	PP_Prefix.h					PowerPlant 1.9.3	©1993-1998 Metrowerks Inc.
// ===========================================================================
//
//	To insure that PowerPlant and compiler options are set properly,
//	the first #include for every file should be <PP_Prefix.h>, or some
//	header that indirectly #include's this file.
//
//	Via nested #include's, every PP class file eventually #include's
//	<PP_Prefix.h>. So #include'ing any PP header as your first #include
//	file or using one of the PP Precompiled header files is OK.

#ifndef _H_PP_Prefix
#define _H_PP_Prefix
#pragma once

	// Header files required for almost all PP files

#include <PP_Macros.h>
#include <PP_Types.h>
#include <PP_Constants.h>
#include <UException.h>


#endif