/*
     File:       DeviceControl.k.h
 
     Contains:   Component API for doing AVC transactions.
 
     Version:    xxx put version here xxx
 
     DRI:        Jed (George) Wilson
 
     Copyright:  © 1999-2001 by Apple Computer, Inc., all rights reserved.
 
     Warning:    *** APPLE INTERNAL USE ONLY ***
                 This file may contain unreleased API's
 
     BuildInfo:  Built by:            wgulland
                 On:                  Tue Mar 12 16:49:01 2002
                 With Interfacer:     3.0d35   (Mac OS X for PowerPC)
                 From:                DeviceControl.i
                     Revision:        3
                     Dated:           6/16/99
                     Last change by:  GDW
                     Last comment:    Changed AVC struct name to DVC for people that include
 
     Bugs:       Report bugs to Radar component "System Interfaces", "Latest"
                 List the version information (from above) in the Problem Description.
 
*/
#ifndef __DEVICECONTROL_K__
#define __DEVICECONTROL_K__

#include <DVComponentGlue/DeviceControl.h>
/*
	Example usage:

		#define DEVICECONTROL_BASENAME()	Fred
		#define DEVICECONTROL_GLOBALS()	FredGlobalsHandle
		#include <DeviceControl.k.h>

	To specify that your component implementation does not use globals, do not #define DEVICECONTROL_GLOBALS
*/
#ifdef DEVICECONTROL_BASENAME
	#ifndef DEVICECONTROL_GLOBALS
		#define DEVICECONTROL_GLOBALS() 
		#define ADD_DEVICECONTROL_COMMA 
	#else
		#define ADD_DEVICECONTROL_COMMA ,
	#endif
	#define DEVICECONTROL_GLUE(a,b) a##b
	#define DEVICECONTROL_STRCAT(a,b) DEVICECONTROL_GLUE(a,b)
	#define ADD_DEVICECONTROL_BASENAME(name) DEVICECONTROL_STRCAT(DEVICECONTROL_BASENAME(),name)

	EXTERN_API( ComponentResult  ) ADD_DEVICECONTROL_BASENAME(DoAVCTransaction) (DEVICECONTROL_GLOBALS() ADD_DEVICECONTROL_COMMA DVCTransactionParams * params);


	/* MixedMode ProcInfo constants for component calls */
	enum {
		uppDeviceControlDoAVCTransactionProcInfo = 0x000003F0
	};

#endif	/* DEVICECONTROL_BASENAME */


#endif /* __DEVICECONTROL_K__ */

