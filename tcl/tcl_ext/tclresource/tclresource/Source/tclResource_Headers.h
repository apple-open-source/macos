// File: "tclResource_Headers.h"
//                        Created: 2003-09-22 10:47:15
//              Last modification: 2003-09-22 10:59:45
// Author: Bernard Desgraupes
// Description: Use this header to include the precompiled headers
// on OSX for dylib target built with CW Pro 8


#pragma check_header_flags on

#if __POWERPC__
	#if TARGET_API_MAC_CARBON
	// Carbon
	#if TARGET_API_MAC_OSX
	// Carbon on X
		#include "MW_tclResourceHeaderCarbonX"
	#else
	// CarbonLib on Classic
		#include "MW_tclResourceHeaderCarbon"
	#endif
	#else
	// Classic
		#include "MW_tclResourceHeaderPPC"
	#endif
#endif

#ifdef MAC_TCL
#undef MAC_TCL
#endif
