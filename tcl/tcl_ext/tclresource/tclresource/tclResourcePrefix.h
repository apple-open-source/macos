// File: "tclResourcePrefix.h"
//                        Created: 2003-09-22 10:47:15
//              Last modification: 2005-12-27 17:28:48
// Author: Bernard Desgraupes
// Description: Use this header on OSX for dylib target built with CodeWarrior

#define TARGET_API_MAC_CARBON	1
#define TARGET_API_MAC_OSX	1

// Stubs mechanism enabled
#define USE_TCL_STUBS

#ifndef __MWERKS__
#error "This prefix file is for the CodeWarrior target only."
#else
#include "tclResource_headers.h"
#endif  //  __MWERKS__

