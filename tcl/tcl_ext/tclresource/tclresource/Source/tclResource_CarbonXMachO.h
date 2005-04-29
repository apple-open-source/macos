// File: "tclResource_CarbonXMachO.h"
//                        Created: 2003-09-22 10:47:15
//              Last modification: 2003-10-24 19:36:02
// Author: Bernard Desgraupes
// Description: Use this header on OSX for dylib target built with CW Pro 8

#define TARGET_API_MAC_CARBON	1
#define TARGET_API_MAC_OSX	1
#define TCLRESOURCE_OSX		1
#define TCLRESOURCE_CARBON	1

// Stubs mechanism enabled
#define USE_TCL_STUBS

#ifndef __MWERKS__
#error "This prefix file is for the CW8 MachO target only."

#else

#define TCLRESOURCE_USE_FRAMEWORK_INCLUDES

// The following macro characterizes this target
#define TCLRESOURCE_MW_MACHO

#include "tclResource_Headers.h"

#endif  //  __MWERKS__

