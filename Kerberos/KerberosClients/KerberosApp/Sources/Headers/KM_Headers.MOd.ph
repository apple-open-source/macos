// ===========================================================================
//	KM_Headers.MOd.ph
// ===========================================================================
//
//	Header prefix for ProjectBuilder version of Kerberos.app
//  Based on the PowerPlant precompiled headers


	
#pragma once on

#include <ConditionalMacros.h>

//  MW suggests these defines for Carbon targets
//	Carbon Target
#define PP_Target_Carbon		1

#define PP_Target_Classic		(!PP_Target_Carbon)

#if __GNUC__ >= 3
#define PP_Uses_PowerPlant_Namespace	1
#define PP_Uses_Std_Namespace		1
#else
#define PP_Uses_PowerPlant_Namespace	0
#define PP_Uses_Std_Namespace		0
#endif

#define PP_Supports_Pascal_Strings		1
#define PP_StdDialogs_Option	PP_StdDialogs_NavServicesOnly
#define	PP_Uses_Old_Integer_Types		0
#define PP_Obsolete_AllowTargetSwitch	0
#define PP_Obsolete_ThrowExceptionCode	0
#define PP_Warn_Obsolete_Classes		1

#define PP_Suppress_Notes_22			1

#define PP_Uses_Periodical_Timers		1

// need to do this because normally this is defind in a Metrowerks header which
// is not part of the standard headers
#ifdef __GNUC__
#define _STD ::std
#define _CSTD ::std
	#ifndef __dest_os
	#define __dest_os __mac_os_x // defining this here keeps PP_Macros.h from trying to load an MSL header
	#endif
#endif

#ifndef MACDEV_DEBUG
#define MACDEV_DEBUG 1
#endif

#if __cplusplus
    #if MACDEV_DEBUG
    #include <PP_DebugHeaders.cp>
    #else
    #include <PP_ClassHeaders.cp>
    #endif
#endif


#if TARGET_RT_MAC_MACHO
    #define RunningUnderMacOSX() 	true
    #define RunningUnderClassic() 	false
#endif
