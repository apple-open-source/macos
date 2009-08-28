// File: "TclResource_headers.h"
//                        Created: 2003-09-22 10:47:15
//              Last modification: 2004-09-01 22:04:56
// Author: Bernard Desgraupes
// Description: Use this header to include the precompiled headers
// on OSX for dylib target built with CW Pro 8


#pragma check_header_flags on

#if __POWERPC__
#include "MW_TclResourceHeaderCarbonX"
#endif

#ifdef MAC_TCL
#undef MAC_TCL
#endif
