/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
     File:       DeviceControlPriv.k.h
 
     Contains:   xxx put contents here xxx
 
     Version:    xxx put version here xxx
 
     DRI:        xxx put dri here xxx
 
     Copyright:  © 1999-2001 by Apple Computer, Inc., all rights reserved.
 
     Warning:    *** APPLE INTERNAL USE ONLY ***
                 This file contains unreleased SPI's
 
     BuildInfo:  Built by:            wgulland
                 On:                  Tue Mar 12 16:49:09 2002
                 With Interfacer:     3.0d35   (Mac OS X for PowerPC)
                 From:                DeviceControlPriv.i
                     Revision:        3
                     Dated:           6/15/99
                     Last change by:  KW
                     Last comment:    fix screwup
 
     Bugs:       Report bugs to Radar component "System Interfaces", "Latest"
                 List the version information (from above) in the Problem Description.
 
*/
#ifndef __DEVICECONTROLPRIV_K__
#define __DEVICECONTROLPRIV_K__

#include <DeviceControlPriv.h>

/*
	Example usage:

		#define DEVICECONTROL_BASENAME()	Fred
		#define DEVICECONTROL_GLOBALS()	FredGlobalsHandle
		#include <DeviceControlPriv.k.h>

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

	EXTERN_API( ComponentResult  ) ADD_DEVICECONTROL_BASENAME(EnableAVCTransactions) (DEVICECONTROL_GLOBALS());

	EXTERN_API( ComponentResult  ) ADD_DEVICECONTROL_BASENAME(DisableAVCTransactions) (DEVICECONTROL_GLOBALS());

	EXTERN_API( ComponentResult  ) ADD_DEVICECONTROL_BASENAME(SetDeviceConnectionID) (DEVICECONTROL_GLOBALS() ADD_DEVICECONTROL_COMMA DeviceConnectionID  connectionID);

	EXTERN_API( ComponentResult  ) ADD_DEVICECONTROL_BASENAME(GetDeviceConnectionID) (DEVICECONTROL_GLOBALS() ADD_DEVICECONTROL_COMMA DeviceConnectionID * connectionID);


	/* MixedMode ProcInfo constants for component calls */
	enum {
		uppDeviceControlEnableAVCTransactionsProcInfo = 0x000000F0,
		uppDeviceControlDisableAVCTransactionsProcInfo = 0x000000F0,
		uppDeviceControlSetDeviceConnectionIDProcInfo = 0x000003F0,
		uppDeviceControlGetDeviceConnectionIDProcInfo = 0x000003F0
	};

#endif	/* DEVICECONTROL_BASENAME */


#endif /* __DEVICECONTROLPRIV_K__ */

