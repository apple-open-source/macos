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
     File:       IsochronousDataHandler.k.h
 
     Contains:   Component Manager based Isochronous Data Handler
 
     Copyright:  © 1997-2001 by Apple Computer, Inc., all rights reserved.
 
     Warning:    *** APPLE INTERNAL USE ONLY ***
                 This file may contain unreleased API's
 
     BuildInfo:  Built by:            wgulland
                 On:                  Tue Mar 12 16:49:01 2002
                 With Interfacer:     3.0d35   (Mac OS X for PowerPC)
                 From:                IsochronousDataHandler.i
                     Revision:        1.5
                     Dated:           2001/10/05 16:46:32
                     Last change by:  wgulland
                     Last comment:    Add inputFormat to IDHDeviceStatus structure
 
     Bugs:       Report bugs to Radar component "System Interfaces", "Latest"
                 List the version information (from above) in the Problem Description.
 
*/
#ifndef __ISOCHRONOUSDATAHANDLER_K__
#define __ISOCHRONOUSDATAHANDLER_K__

#include <DVComponentGlue/IsochronousDataHandler.h>
/*
	Example usage:

		#define IDH_BASENAME()	Fred
		#define IDH_GLOBALS()	FredGlobalsHandle
		#include <IsochronousDataHandler.k.h>

	To specify that your component implementation does not use globals, do not #define IDH_GLOBALS
*/
#ifdef IDH_BASENAME
	#ifndef IDH_GLOBALS
		#define IDH_GLOBALS() 
		#define ADD_IDH_COMMA 
	#else
		#define ADD_IDH_COMMA ,
	#endif
	#define IDH_GLUE(a,b) a##b
	#define IDH_STRCAT(a,b) IDH_GLUE(a,b)
	#define ADD_IDH_BASENAME(name) IDH_STRCAT(IDH_BASENAME(),name)

	EXTERN_API( ComponentResult  ) ADD_IDH_BASENAME(GetDeviceList) (IDH_GLOBALS() ADD_IDH_COMMA QTAtomContainer * deviceList);

	EXTERN_API( ComponentResult  ) ADD_IDH_BASENAME(GetDeviceConfiguration) (IDH_GLOBALS() ADD_IDH_COMMA QTAtomSpec * configurationID);

	EXTERN_API( ComponentResult  ) ADD_IDH_BASENAME(SetDeviceConfiguration) (IDH_GLOBALS() ADD_IDH_COMMA const QTAtomSpec * configurationID);

	EXTERN_API( ComponentResult  ) ADD_IDH_BASENAME(GetDeviceStatus) (IDH_GLOBALS() ADD_IDH_COMMA const QTAtomSpec * configurationID, IDHDeviceStatus * status);

	EXTERN_API( ComponentResult  ) ADD_IDH_BASENAME(GetDeviceClock) (IDH_GLOBALS() ADD_IDH_COMMA Component * clock);

	EXTERN_API( ComponentResult  ) ADD_IDH_BASENAME(OpenDevice) (IDH_GLOBALS() ADD_IDH_COMMA UInt32  permissions);

	EXTERN_API( ComponentResult  ) ADD_IDH_BASENAME(CloseDevice) (IDH_GLOBALS());

	EXTERN_API( ComponentResult  ) ADD_IDH_BASENAME(Read) (IDH_GLOBALS() ADD_IDH_COMMA IDHParameterBlock * pb);

	EXTERN_API( ComponentResult  ) ADD_IDH_BASENAME(Write) (IDH_GLOBALS() ADD_IDH_COMMA IDHParameterBlock * pb);

	EXTERN_API( ComponentResult  ) ADD_IDH_BASENAME(NewNotification) (IDH_GLOBALS() ADD_IDH_COMMA IDHDeviceID  deviceID, IDHNotificationUPP  notificationProc, void * userData, IDHNotificationID * notificationID);

	EXTERN_API( ComponentResult  ) ADD_IDH_BASENAME(NotifyMeWhen) (IDH_GLOBALS() ADD_IDH_COMMA IDHNotificationID  notificationID, IDHEvent  events);

	EXTERN_API( ComponentResult  ) ADD_IDH_BASENAME(CancelNotification) (IDH_GLOBALS() ADD_IDH_COMMA IDHNotificationID  notificationID);

	EXTERN_API( ComponentResult  ) ADD_IDH_BASENAME(DisposeNotification) (IDH_GLOBALS() ADD_IDH_COMMA IDHNotificationID  notificationID);

	EXTERN_API( ComponentResult  ) ADD_IDH_BASENAME(ReleaseBuffer) (IDH_GLOBALS() ADD_IDH_COMMA IDHParameterBlock * pb);

	EXTERN_API( ComponentResult  ) ADD_IDH_BASENAME(CancelPendingIO) (IDH_GLOBALS() ADD_IDH_COMMA IDHParameterBlock * pb);

	EXTERN_API( ComponentResult  ) ADD_IDH_BASENAME(GetDeviceControl) (IDH_GLOBALS() ADD_IDH_COMMA ComponentInstance * deviceControl);

	EXTERN_API( ComponentResult  ) ADD_IDH_BASENAME(UpdateDeviceList) (IDH_GLOBALS() ADD_IDH_COMMA QTAtomContainer * deviceList);

	EXTERN_API( ComponentResult  ) ADD_IDH_BASENAME(GetDeviceTime) (IDH_GLOBALS() ADD_IDH_COMMA TimeRecord * deviceTime);

	EXTERN_API( ComponentResult  ) ADD_IDH_BASENAME(SetFormat) (IDH_GLOBALS() ADD_IDH_COMMA UInt32  format);

	EXTERN_API( ComponentResult  ) ADD_IDH_BASENAME(GetFormat) (IDH_GLOBALS() ADD_IDH_COMMA UInt32 * format);


	/* MixedMode ProcInfo constants for component calls */
	enum {
		uppIDHGetDeviceListProcInfo = 0x000003F0,
		uppIDHGetDeviceConfigurationProcInfo = 0x000003F0,
		uppIDHSetDeviceConfigurationProcInfo = 0x000003F0,
		uppIDHGetDeviceStatusProcInfo = 0x00000FF0,
		uppIDHGetDeviceClockProcInfo = 0x000003F0,
		uppIDHOpenDeviceProcInfo = 0x000003F0,
		uppIDHCloseDeviceProcInfo = 0x000000F0,
		uppIDHReadProcInfo = 0x000003F0,
		uppIDHWriteProcInfo = 0x000003F0,
		uppIDHNewNotificationProcInfo = 0x0000FFF0,
		uppIDHNotifyMeWhenProcInfo = 0x00000FF0,
		uppIDHCancelNotificationProcInfo = 0x000003F0,
		uppIDHDisposeNotificationProcInfo = 0x000003F0,
		uppIDHReleaseBufferProcInfo = 0x000003F0,
		uppIDHCancelPendingIOProcInfo = 0x000003F0,
		uppIDHGetDeviceControlProcInfo = 0x000003F0,
		uppIDHUpdateDeviceListProcInfo = 0x000003F0,
		uppIDHGetDeviceTimeProcInfo = 0x000003F0,
		uppIDHSetFormatProcInfo = 0x000003F0,
		uppIDHGetFormatProcInfo = 0x000003F0
	};

#endif	/* IDH_BASENAME */


#endif /* __ISOCHRONOUSDATAHANDLER_K__ */

