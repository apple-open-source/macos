// ===========================================================================
//	KM_Headers.9d.h			©1996-1997 Metrowerks Inc. All rights reserved.
// ===========================================================================
// Prefix header for CodeWarrior Classic CFM Kerberos Control Panel debugging build

// debug new seems to give false warnings on std::string so we'll disable
// it so we can keep using rest of debugging classes
#define PP_DEBUGNEW_SUPPORT			0

	#include "KM_Headers.9d"
