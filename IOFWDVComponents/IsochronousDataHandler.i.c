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
     File:       IsochronousDataHandler.i.c
 
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


#include <CoreServices/CoreServices.h>
//#include <CarbonCore/MixedMode.h>
//#include <CarbonCore/Components.h>
#include <DVComponentGlue/IsochronousDataHandler.h>
#if MP_SUPPORT
	#include "MPMixedModeSupport.h"
#endif

#define TOOLBOX_TRAPADDRESS(trapNum) (*(((UniversalProcPtr*)(((trapNum & 0x03FF) << 2) + 0xE00))))
#define OS_TRAPADDRESS(trapNum)      (*(((UniversalProcPtr*)(((trapNum & 0x00FF) << 2) + 0x400))))

#ifndef TRAPGLUE_NO_COMPONENT_CALL
DEFINE_API( ComponentResult ) IDHGetDeviceList(ComponentInstance idh, QTAtomContainer* deviceList)
{
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=mac68k
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(push, 2)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack(2)
	#endif
	struct IDHGetDeviceListGluePB {
		unsigned char                  componentFlags;
		unsigned char                  componentParamSize;
		short                          componentWhat;
		QTAtomContainer*               deviceList;
		ComponentInstance              idh;
	};
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=reset
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(pop)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack()
	#endif

	#if OLD_COMPONENT_GLUE
	struct IDHGetDeviceListGluePB myIDHGetDeviceListGluePB = {
		0,
		4,
		1
	};

	#else
	struct IDHGetDeviceListGluePB myIDHGetDeviceListGluePB;
//	*((unsigned long*)&myIDHGetDeviceListGluePB) = 0x00040001;
	
	myIDHGetDeviceListGluePB.componentFlags = 0;
	myIDHGetDeviceListGluePB.componentParamSize = 4;
	myIDHGetDeviceListGluePB.componentWhat = 1;

	#endif

	myIDHGetDeviceListGluePB.deviceList = deviceList;
	myIDHGetDeviceListGluePB.idh = idh;

	#if TARGET_API_MAC_OS8
		return (ComponentResult)CallUniversalProc(CallComponentUPP, 0x000000F0, &myIDHGetDeviceListGluePB);
	#else
		return (ComponentResult)CallComponentDispatch( (ComponentParameters*)&myIDHGetDeviceListGluePB );
	#endif
}
#endif


#ifndef TRAPGLUE_NO_COMPONENT_CALL
DEFINE_API( ComponentResult ) IDHGetDeviceConfiguration(ComponentInstance idh, QTAtomSpec* configurationID)
{
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=mac68k
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(push, 2)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack(2)
	#endif
	struct IDHGetDeviceConfigurationGluePB {
		unsigned char                  componentFlags;
		unsigned char                  componentParamSize;
		short                          componentWhat;
		QTAtomSpec*                    configurationID;
		ComponentInstance              idh;
	};
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=reset
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(pop)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack()
	#endif

	#if OLD_COMPONENT_GLUE
	struct IDHGetDeviceConfigurationGluePB myIDHGetDeviceConfigurationGluePB = {
		0,
		4,
		2
	};

	#else
	struct IDHGetDeviceConfigurationGluePB myIDHGetDeviceConfigurationGluePB;
//	*((unsigned long*)&myIDHGetDeviceConfigurationGluePB) = 0x00040002;

	myIDHGetDeviceConfigurationGluePB.componentFlags = 0;
	myIDHGetDeviceConfigurationGluePB.componentParamSize = 4;
	myIDHGetDeviceConfigurationGluePB.componentWhat = 2;
	
	#endif

	myIDHGetDeviceConfigurationGluePB.configurationID = configurationID;
	myIDHGetDeviceConfigurationGluePB.idh = idh;

	#if TARGET_API_MAC_OS8
		return (ComponentResult)CallUniversalProc(CallComponentUPP, 0x000000F0, &myIDHGetDeviceConfigurationGluePB);
	#else
		return (ComponentResult)CallComponentDispatch( (ComponentParameters*)&myIDHGetDeviceConfigurationGluePB );
	#endif
}
#endif


#ifndef TRAPGLUE_NO_COMPONENT_CALL
DEFINE_API( ComponentResult ) IDHSetDeviceConfiguration(ComponentInstance idh, const QTAtomSpec* configurationID)
{
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=mac68k
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(push, 2)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack(2)
	#endif
	struct IDHSetDeviceConfigurationGluePB {
		unsigned char                  componentFlags;
		unsigned char                  componentParamSize;
		short                          componentWhat;
		const QTAtomSpec*              configurationID;
		ComponentInstance              idh;
	};
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=reset
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(pop)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack()
	#endif

	#if OLD_COMPONENT_GLUE
	struct IDHSetDeviceConfigurationGluePB myIDHSetDeviceConfigurationGluePB = {
		0,
		4,
		3
	};

	#else
	struct IDHSetDeviceConfigurationGluePB myIDHSetDeviceConfigurationGluePB;
	//*((unsigned long*)&myIDHSetDeviceConfigurationGluePB) = 0x00040003;
	
	myIDHSetDeviceConfigurationGluePB.componentFlags = 0;
	myIDHSetDeviceConfigurationGluePB.componentParamSize = 4;
	myIDHSetDeviceConfigurationGluePB.componentWhat = 3;
	
	#endif

	myIDHSetDeviceConfigurationGluePB.configurationID = configurationID;
	myIDHSetDeviceConfigurationGluePB.idh = idh;

	#if TARGET_API_MAC_OS8
		return (ComponentResult)CallUniversalProc(CallComponentUPP, 0x000000F0, &myIDHSetDeviceConfigurationGluePB);
	#else
		return (ComponentResult)CallComponentDispatch( (ComponentParameters*)&myIDHSetDeviceConfigurationGluePB );
	#endif
}
#endif


#ifndef TRAPGLUE_NO_COMPONENT_CALL
DEFINE_API( ComponentResult ) IDHGetDeviceStatus(ComponentInstance idh, const QTAtomSpec* configurationID, IDHDeviceStatus* status)
{
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=mac68k
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(push, 2)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack(2)
	#endif
	struct IDHGetDeviceStatusGluePB {
		unsigned char                  componentFlags;
		unsigned char                  componentParamSize;
		short                          componentWhat;
		IDHDeviceStatus*               status;
		const QTAtomSpec*              configurationID;
		ComponentInstance              idh;
	};
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=reset
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(pop)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack()
	#endif

	#if OLD_COMPONENT_GLUE
	struct IDHGetDeviceStatusGluePB myIDHGetDeviceStatusGluePB = {
		0,
		8,
		4
	};

	#else
	struct IDHGetDeviceStatusGluePB myIDHGetDeviceStatusGluePB;
//	*((unsigned long*)&myIDHGetDeviceStatusGluePB) = 0x00080004;

	myIDHGetDeviceStatusGluePB.componentFlags = 0;
	myIDHGetDeviceStatusGluePB.componentParamSize = 8;
	myIDHGetDeviceStatusGluePB.componentWhat = 4;
	
	#endif

	myIDHGetDeviceStatusGluePB.status = status;
	myIDHGetDeviceStatusGluePB.configurationID = configurationID;
	myIDHGetDeviceStatusGluePB.idh = idh;

	#if TARGET_API_MAC_OS8
		return (ComponentResult)CallUniversalProc(CallComponentUPP, 0x000000F0, &myIDHGetDeviceStatusGluePB);
	#else
		return (ComponentResult)CallComponentDispatch( (ComponentParameters*)&myIDHGetDeviceStatusGluePB );
	#endif
}
#endif


#ifndef TRAPGLUE_NO_COMPONENT_CALL
DEFINE_API( ComponentResult ) IDHGetDeviceClock(ComponentInstance idh, Component* clock)
{
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=mac68k
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(push, 2)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack(2)
	#endif
	struct IDHGetDeviceClockGluePB {
		unsigned char                  componentFlags;
		unsigned char                  componentParamSize;
		short                          componentWhat;
		Component*                     clock;
		ComponentInstance              idh;
	};
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=reset
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(pop)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack()
	#endif

	#if OLD_COMPONENT_GLUE
	struct IDHGetDeviceClockGluePB myIDHGetDeviceClockGluePB = {
		0,
		4,
		5
	};

	#else
	struct IDHGetDeviceClockGluePB myIDHGetDeviceClockGluePB;
//	*((unsigned long*)&myIDHGetDeviceClockGluePB) = 0x00040005;

	myIDHGetDeviceClockGluePB.componentFlags = 0;
	myIDHGetDeviceClockGluePB.componentParamSize = 4;
	myIDHGetDeviceClockGluePB.componentWhat = 5;
	
	#endif

	myIDHGetDeviceClockGluePB.clock = clock;
	myIDHGetDeviceClockGluePB.idh = idh;

	#if TARGET_API_MAC_OS8
		return (ComponentResult)CallUniversalProc(CallComponentUPP, 0x000000F0, &myIDHGetDeviceClockGluePB);
	#else
		return (ComponentResult)CallComponentDispatch( (ComponentParameters*)&myIDHGetDeviceClockGluePB );
	#endif
}
#endif


#ifndef TRAPGLUE_NO_COMPONENT_CALL
DEFINE_API( ComponentResult ) IDHOpenDevice(ComponentInstance idh, UInt32 permissions)
{
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=mac68k
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(push, 2)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack(2)
	#endif
	struct IDHOpenDeviceGluePB {
		unsigned char                  componentFlags;
		unsigned char                  componentParamSize;
		short                          componentWhat;
		UInt32                         permissions;
		ComponentInstance              idh;
	};
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=reset
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(pop)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack()
	#endif

	#if OLD_COMPONENT_GLUE
	struct IDHOpenDeviceGluePB myIDHOpenDeviceGluePB = {
		0,
		4,
		6
	};

	#else
	struct IDHOpenDeviceGluePB myIDHOpenDeviceGluePB;
//	*((unsigned long*)&myIDHOpenDeviceGluePB) = 0x00040006;

	myIDHOpenDeviceGluePB.componentFlags = 0;
	myIDHOpenDeviceGluePB.componentParamSize = 4;
	myIDHOpenDeviceGluePB.componentWhat = 6;
	
	#endif

	myIDHOpenDeviceGluePB.permissions = permissions;
	myIDHOpenDeviceGluePB.idh = idh;

	#if TARGET_API_MAC_OS8
		return (ComponentResult)CallUniversalProc(CallComponentUPP, 0x000000F0, &myIDHOpenDeviceGluePB);
	#else
		return (ComponentResult)CallComponentDispatch( (ComponentParameters*)&myIDHOpenDeviceGluePB );
	#endif
}
#endif


#ifndef TRAPGLUE_NO_COMPONENT_CALL
DEFINE_API( ComponentResult ) IDHCloseDevice(ComponentInstance idh)
{
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=mac68k
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(push, 2)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack(2)
	#endif
	struct IDHCloseDeviceGluePB {
		unsigned char                  componentFlags;
		unsigned char                  componentParamSize;
		short                          componentWhat;
		ComponentInstance              idh;
	};
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=reset
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(pop)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack()
	#endif

	#if OLD_COMPONENT_GLUE
	struct IDHCloseDeviceGluePB myIDHCloseDeviceGluePB = {
		0,
		0,
		7
	};

	#else
	struct IDHCloseDeviceGluePB myIDHCloseDeviceGluePB;
	//*((unsigned long*)&myIDHCloseDeviceGluePB) = 0x00000007;
	
	myIDHCloseDeviceGluePB.componentFlags = 0;
	myIDHCloseDeviceGluePB.componentParamSize = 0;
	myIDHCloseDeviceGluePB.componentWhat = 7;
	
	#endif

	myIDHCloseDeviceGluePB.idh = idh;

	#if TARGET_API_MAC_OS8
		return (ComponentResult)CallUniversalProc(CallComponentUPP, 0x000000F0, &myIDHCloseDeviceGluePB);
	#else
		return (ComponentResult)CallComponentDispatch( (ComponentParameters*)&myIDHCloseDeviceGluePB );
	#endif
}
#endif


#ifndef TRAPGLUE_NO_COMPONENT_CALL
DEFINE_API( ComponentResult ) IDHRead(ComponentInstance idh, IDHParameterBlock* pb)
{
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=mac68k
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(push, 2)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack(2)
	#endif
	struct IDHReadGluePB {
		unsigned char                  componentFlags;
		unsigned char                  componentParamSize;
		short                          componentWhat;
		IDHParameterBlock*             pb;
		ComponentInstance              idh;
	};
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=reset
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(pop)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack()
	#endif

	#if OLD_COMPONENT_GLUE
	struct IDHReadGluePB myIDHReadGluePB = {
		0,
		4,
		8
	};

	#else
	struct IDHReadGluePB myIDHReadGluePB;
	//*((unsigned long*)&myIDHReadGluePB) = 0x00040008;
	
	myIDHReadGluePB.componentFlags = 0;
	myIDHReadGluePB.componentParamSize = 4;
	myIDHReadGluePB.componentWhat = 8;
	
	#endif

	myIDHReadGluePB.pb = pb;
	myIDHReadGluePB.idh = idh;

	#if TARGET_API_MAC_OS8
		return (ComponentResult)CallUniversalProc(CallComponentUPP, 0x000000F0, &myIDHReadGluePB);
	#else
		return (ComponentResult)CallComponentDispatch( (ComponentParameters*)&myIDHReadGluePB );
	#endif
}
#endif


#ifndef TRAPGLUE_NO_COMPONENT_CALL
DEFINE_API( ComponentResult ) IDHWrite(ComponentInstance idh, IDHParameterBlock* pb)
{
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=mac68k
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(push, 2)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack(2)
	#endif
	struct IDHWriteGluePB {
		unsigned char                  componentFlags;
		unsigned char                  componentParamSize;
		short                          componentWhat;
		IDHParameterBlock*             pb;
		ComponentInstance              idh;
	};
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=reset
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(pop)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack()
	#endif

	#if OLD_COMPONENT_GLUE
	struct IDHWriteGluePB myIDHWriteGluePB = {
		0,
		4,
		9
	};

	#else
	struct IDHWriteGluePB myIDHWriteGluePB;
	*((unsigned long*)&myIDHWriteGluePB) = 0x00040009;
	
	myIDHWriteGluePB.componentFlags = 0;
	myIDHWriteGluePB.componentParamSize = 4;
	myIDHWriteGluePB.componentWhat = 9;
	
	#endif

	myIDHWriteGluePB.pb = pb;
	myIDHWriteGluePB.idh = idh;

	#if TARGET_API_MAC_OS8
		return (ComponentResult)CallUniversalProc(CallComponentUPP, 0x000000F0, &myIDHWriteGluePB);
	#else
		return (ComponentResult)CallComponentDispatch( (ComponentParameters*)&myIDHWriteGluePB );
	#endif
}
#endif


#ifndef TRAPGLUE_NO_COMPONENT_CALL
DEFINE_API( ComponentResult ) IDHNewNotification(ComponentInstance idh, IDHDeviceID deviceID, IDHNotificationUPP notificationProc, void* userData, IDHNotificationID* notificationID)
{
	//printf( "IDHNewNotification\n" );
	
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=mac68k
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(push, 2)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack(2)
	#endif
	struct IDHNewNotificationGluePB {
		unsigned char                  componentFlags;
		unsigned char                  componentParamSize;
		short                          componentWhat;
		IDHNotificationID*             notificationID;
		void*                          userData;
		IDHNotificationUPP             notificationProc;
		IDHDeviceID                    deviceID;
		ComponentInstance              idh;
	};
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=reset
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(pop)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack()
	#endif

	#if OLD_COMPONENT_GLUE
	struct IDHNewNotificationGluePB myIDHNewNotificationGluePB = {
		0,
		16,
		10
	};

	#else
	struct IDHNewNotificationGluePB myIDHNewNotificationGluePB;
	//*((unsigned long*)&myIDHNewNotificationGluePB) = 0x0010000A;
	
	myIDHNewNotificationGluePB.componentFlags = 0;
	myIDHNewNotificationGluePB.componentParamSize = 16;
	myIDHNewNotificationGluePB.componentWhat = 10;
	#endif

	myIDHNewNotificationGluePB.notificationID = notificationID;
	myIDHNewNotificationGluePB.userData = userData;
	myIDHNewNotificationGluePB.notificationProc = notificationProc;
	myIDHNewNotificationGluePB.deviceID = deviceID;
	myIDHNewNotificationGluePB.idh = idh;

	#if TARGET_API_MAC_OS8
		return (ComponentResult)CallUniversalProc(CallComponentUPP, 0x000000F0, &myIDHNewNotificationGluePB);
	#else
		return (ComponentResult)CallComponentDispatch( (ComponentParameters*)&myIDHNewNotificationGluePB );
	#endif
}
#endif


#ifndef TRAPGLUE_NO_COMPONENT_CALL
DEFINE_API( ComponentResult ) IDHNotifyMeWhen(ComponentInstance idh, IDHNotificationID notificationID, IDHEvent events)
{
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=mac68k
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(push, 2)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack(2)
	#endif
	struct IDHNotifyMeWhenGluePB {
		unsigned char                  componentFlags;
		unsigned char                  componentParamSize;
		short                          componentWhat;
		IDHEvent                       events;
		IDHNotificationID              notificationID;
		ComponentInstance              idh;
	};
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=reset
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(pop)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack()
	#endif

	#if OLD_COMPONENT_GLUE
	struct IDHNotifyMeWhenGluePB myIDHNotifyMeWhenGluePB = {
		0,
		8,
		11
	};

	#else
	struct IDHNotifyMeWhenGluePB myIDHNotifyMeWhenGluePB;
	//*((unsigned long*)&myIDHNotifyMeWhenGluePB) = 0x0008000B;
	
	myIDHNotifyMeWhenGluePB.componentFlags = 0;
	myIDHNotifyMeWhenGluePB.componentParamSize = 8;
	myIDHNotifyMeWhenGluePB.componentWhat = 11;
	
	#endif

	myIDHNotifyMeWhenGluePB.events = events;
	myIDHNotifyMeWhenGluePB.notificationID = notificationID;
	myIDHNotifyMeWhenGluePB.idh = idh;

	#if TARGET_API_MAC_OS8
		return (ComponentResult)CallUniversalProc(CallComponentUPP, 0x000000F0, &myIDHNotifyMeWhenGluePB);
	#else
		return (ComponentResult)CallComponentDispatch( (ComponentParameters*)&myIDHNotifyMeWhenGluePB );
	#endif
}
#endif


#ifndef TRAPGLUE_NO_COMPONENT_CALL
DEFINE_API( ComponentResult ) IDHCancelNotification(ComponentInstance idh, IDHNotificationID notificationID)
{
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=mac68k
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(push, 2)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack(2)
	#endif
	struct IDHCancelNotificationGluePB {
		unsigned char                  componentFlags;
		unsigned char                  componentParamSize;
		short                          componentWhat;
		IDHNotificationID              notificationID;
		ComponentInstance              idh;
	};
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=reset
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(pop)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack()
	#endif

	#if OLD_COMPONENT_GLUE
	struct IDHCancelNotificationGluePB myIDHCancelNotificationGluePB = {
		0,
		4,
		12
	};

	#else
	struct IDHCancelNotificationGluePB myIDHCancelNotificationGluePB;
//	*((unsigned long*)&myIDHCancelNotificationGluePB) = 0x0004000C;
	
	myIDHCancelNotificationGluePB.componentFlags = 0;
	myIDHCancelNotificationGluePB.componentParamSize = 4;
	myIDHCancelNotificationGluePB.componentWhat = 12;
	
	#endif

	myIDHCancelNotificationGluePB.notificationID = notificationID;
	myIDHCancelNotificationGluePB.idh = idh;

	#if TARGET_API_MAC_OS8
		return (ComponentResult)CallUniversalProc(CallComponentUPP, 0x000000F0, &myIDHCancelNotificationGluePB);
	#else
		return (ComponentResult)CallComponentDispatch( (ComponentParameters*)&myIDHCancelNotificationGluePB );
	#endif
}
#endif


#ifndef TRAPGLUE_NO_COMPONENT_CALL
DEFINE_API( ComponentResult ) IDHDisposeNotification(ComponentInstance idh, IDHNotificationID notificationID)
{
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=mac68k
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(push, 2)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack(2)
	#endif
	struct IDHDisposeNotificationGluePB {
		unsigned char                  componentFlags;
		unsigned char                  componentParamSize;
		short                          componentWhat;
		IDHNotificationID              notificationID;
		ComponentInstance              idh;
	};
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=reset
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(pop)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack()
	#endif

	#if OLD_COMPONENT_GLUE
	struct IDHDisposeNotificationGluePB myIDHDisposeNotificationGluePB = {
		0,
		4,
		13
	};

	#else
	struct IDHDisposeNotificationGluePB myIDHDisposeNotificationGluePB;
//	*((unsigned long*)&myIDHDisposeNotificationGluePB) = 0x0004000D;
	
	myIDHDisposeNotificationGluePB.componentFlags = 0;
	myIDHDisposeNotificationGluePB.componentParamSize = 4;
	myIDHDisposeNotificationGluePB.componentWhat = 13;
	
	#endif

	myIDHDisposeNotificationGluePB.notificationID = notificationID;
	myIDHDisposeNotificationGluePB.idh = idh;

	#if TARGET_API_MAC_OS8
		return (ComponentResult)CallUniversalProc(CallComponentUPP, 0x000000F0, &myIDHDisposeNotificationGluePB);
	#else
		return (ComponentResult)CallComponentDispatch( (ComponentParameters*)&myIDHDisposeNotificationGluePB );
	#endif
}
#endif


#ifndef TRAPGLUE_NO_COMPONENT_CALL
DEFINE_API( ComponentResult ) IDHReleaseBuffer(ComponentInstance idh, IDHParameterBlock* pb)
{
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=mac68k
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(push, 2)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack(2)
	#endif
	struct IDHReleaseBufferGluePB {
		unsigned char                  componentFlags;
		unsigned char                  componentParamSize;
		short                          componentWhat;
		IDHParameterBlock*             pb;
		ComponentInstance              idh;
	};
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=reset
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(pop)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack()
	#endif

	#if OLD_COMPONENT_GLUE
	struct IDHReleaseBufferGluePB myIDHReleaseBufferGluePB = {
		0,
		4,
		14
	};

	#else
	struct IDHReleaseBufferGluePB myIDHReleaseBufferGluePB;
//	*((unsigned long*)&myIDHReleaseBufferGluePB) = 0x0004000E;
	
	myIDHReleaseBufferGluePB.componentFlags = 0;
	myIDHReleaseBufferGluePB.componentParamSize = 4;
	myIDHReleaseBufferGluePB.componentWhat = 14;
	
	#endif

	myIDHReleaseBufferGluePB.pb = pb;
	myIDHReleaseBufferGluePB.idh = idh;

	#if TARGET_API_MAC_OS8
		return (ComponentResult)CallUniversalProc(CallComponentUPP, 0x000000F0, &myIDHReleaseBufferGluePB);
	#else
		return (ComponentResult)CallComponentDispatch( (ComponentParameters*)&myIDHReleaseBufferGluePB );
	#endif
}
#endif


#ifndef TRAPGLUE_NO_COMPONENT_CALL
DEFINE_API( ComponentResult ) IDHCancelPendingIO(ComponentInstance idh, IDHParameterBlock* pb)
{
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=mac68k
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(push, 2)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack(2)
	#endif
	struct IDHCancelPendingIOGluePB {
		unsigned char                  componentFlags;
		unsigned char                  componentParamSize;
		short                          componentWhat;
		IDHParameterBlock*             pb;
		ComponentInstance              idh;
	};
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=reset
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(pop)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack()
	#endif

	#if OLD_COMPONENT_GLUE
	struct IDHCancelPendingIOGluePB myIDHCancelPendingIOGluePB = {
		0,
		4,
		15
	};

	#else
	struct IDHCancelPendingIOGluePB myIDHCancelPendingIOGluePB;
//	*((unsigned long*)&myIDHCancelPendingIOGluePB) = 0x0004000F;
	
	myIDHCancelPendingIOGluePB.componentFlags = 0;
	myIDHCancelPendingIOGluePB.componentParamSize = 4;
	myIDHCancelPendingIOGluePB.componentWhat = 15;
	
	#endif

	myIDHCancelPendingIOGluePB.pb = pb;
	myIDHCancelPendingIOGluePB.idh = idh;

	#if TARGET_API_MAC_OS8
		return (ComponentResult)CallUniversalProc(CallComponentUPP, 0x000000F0, &myIDHCancelPendingIOGluePB);
	#else
		return (ComponentResult)CallComponentDispatch( (ComponentParameters*)&myIDHCancelPendingIOGluePB );
	#endif
}
#endif


#ifndef TRAPGLUE_NO_COMPONENT_CALL
DEFINE_API( ComponentResult ) IDHGetDeviceControl(ComponentInstance idh, ComponentInstance* deviceControl)
{
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=mac68k
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(push, 2)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack(2)
	#endif
	struct IDHGetDeviceControlGluePB {
		unsigned char                  componentFlags;
		unsigned char                  componentParamSize;
		short                          componentWhat;
		ComponentInstance*             deviceControl;
		ComponentInstance              idh;
	};
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=reset
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(pop)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack()
	#endif

	#if OLD_COMPONENT_GLUE
	struct IDHGetDeviceControlGluePB myIDHGetDeviceControlGluePB = {
		0,
		4,
		16
	};

	#else
	struct IDHGetDeviceControlGluePB myIDHGetDeviceControlGluePB;
//	*((unsigned long*)&myIDHGetDeviceControlGluePB) = 0x00040010;
	
	myIDHGetDeviceControlGluePB.componentFlags = 0;
	myIDHGetDeviceControlGluePB.componentParamSize = 4;
	myIDHGetDeviceControlGluePB.componentWhat = 16;
		
	#endif

	myIDHGetDeviceControlGluePB.deviceControl = deviceControl;
	myIDHGetDeviceControlGluePB.idh = idh;

	#if TARGET_API_MAC_OS8
		return (ComponentResult)CallUniversalProc(CallComponentUPP, 0x000000F0, &myIDHGetDeviceControlGluePB);
	#else
		return (ComponentResult)CallComponentDispatch( (ComponentParameters*)&myIDHGetDeviceControlGluePB );
	#endif
}
#endif


#ifndef TRAPGLUE_NO_COMPONENT_CALL
DEFINE_API( ComponentResult ) IDHUpdateDeviceList(ComponentInstance idh, QTAtomContainer* deviceList)
{
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=mac68k
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(push, 2)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack(2)
	#endif
	struct IDHUpdateDeviceListGluePB {
		unsigned char                  componentFlags;
		unsigned char                  componentParamSize;
		short                          componentWhat;
		QTAtomContainer*               deviceList;
		ComponentInstance              idh;
	};
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=reset
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(pop)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack()
	#endif

	#if OLD_COMPONENT_GLUE
	struct IDHUpdateDeviceListGluePB myIDHUpdateDeviceListGluePB = {
		0,
		4,
		17
	};

	#else
	struct IDHUpdateDeviceListGluePB myIDHUpdateDeviceListGluePB;
//	*((unsigned long*)&myIDHUpdateDeviceListGluePB) = 0x00040011;

	myIDHUpdateDeviceListGluePB.componentFlags = 0;
	myIDHUpdateDeviceListGluePB.componentParamSize = 4;
	myIDHUpdateDeviceListGluePB.componentWhat = 17;
	
	#endif

	myIDHUpdateDeviceListGluePB.deviceList = deviceList;
	myIDHUpdateDeviceListGluePB.idh = idh;

	#if TARGET_API_MAC_OS8
		return (ComponentResult)CallUniversalProc(CallComponentUPP, 0x000000F0, &myIDHUpdateDeviceListGluePB);
	#else
		return (ComponentResult)CallComponentDispatch( (ComponentParameters*)&myIDHUpdateDeviceListGluePB );
	#endif
}
#endif


#ifndef TRAPGLUE_NO_COMPONENT_CALL
DEFINE_API( ComponentResult ) IDHGetDeviceTime(ComponentInstance idh, TimeRecord* deviceTime)
{
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=mac68k
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(push, 2)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack(2)
	#endif
	struct IDHGetDeviceTimeGluePB {
		unsigned char                  componentFlags;
		unsigned char                  componentParamSize;
		short                          componentWhat;
		TimeRecord*                    deviceTime;
		ComponentInstance              idh;
	};
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=reset
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(pop)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack()
	#endif

	#if OLD_COMPONENT_GLUE
	struct IDHGetDeviceTimeGluePB myIDHGetDeviceTimeGluePB = {
		0,
		4,
		18
	};

	#else
	struct IDHGetDeviceTimeGluePB myIDHGetDeviceTimeGluePB;
//	*((unsigned long*)&myIDHGetDeviceTimeGluePB) = 0x00040012;
	
	myIDHGetDeviceTimeGluePB.componentFlags = 0;
	myIDHGetDeviceTimeGluePB.componentParamSize = 4;
	myIDHGetDeviceTimeGluePB.componentWhat = 18;
		
	#endif

	myIDHGetDeviceTimeGluePB.deviceTime = deviceTime;
	myIDHGetDeviceTimeGluePB.idh = idh;

	#if TARGET_API_MAC_OS8
		return (ComponentResult)CallUniversalProc(CallComponentUPP, 0x000000F0, &myIDHGetDeviceTimeGluePB);
	#else
		return (ComponentResult)CallComponentDispatch( (ComponentParameters*)&myIDHGetDeviceTimeGluePB );
	#endif
}
#endif


#ifndef TRAPGLUE_NO_COMPONENT_CALL
DEFINE_API( ComponentResult ) IDHSetFormat(ComponentInstance idh, UInt32 format)
{
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=mac68k
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(push, 2)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack(2)
	#endif
	struct IDHSetFormatGluePB {
		unsigned char                  componentFlags;
		unsigned char                  componentParamSize;
		short                          componentWhat;
		UInt32                         format;
		ComponentInstance              idh;
	};
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=reset
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(pop)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack()
	#endif

	#if OLD_COMPONENT_GLUE
	struct IDHSetFormatGluePB myIDHSetFormatGluePB = {
		0,
		4,
		19
	};

	#else
	struct IDHSetFormatGluePB myIDHSetFormatGluePB;
//	*((unsigned long*)&myIDHSetFormatGluePB) = 0x00040013;
	
	myIDHSetFormatGluePB.componentFlags = 0;
	myIDHSetFormatGluePB.componentParamSize = 4;
	myIDHSetFormatGluePB.componentWhat = 19;
		
	#endif

	myIDHSetFormatGluePB.format = format;
	myIDHSetFormatGluePB.idh = idh;

	#if TARGET_API_MAC_OS8
		return (ComponentResult)CallUniversalProc(CallComponentUPP, 0x000000F0, &myIDHSetFormatGluePB);
	#else
		return (ComponentResult)CallComponentDispatch( (ComponentParameters*)&myIDHSetFormatGluePB );
	#endif
}
#endif


#ifndef TRAPGLUE_NO_COMPONENT_CALL
DEFINE_API( ComponentResult ) IDHGetFormat(ComponentInstance idh, UInt32* format)
{
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=mac68k
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(push, 2)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack(2)
	#endif
	struct IDHGetFormatGluePB {
		unsigned char                  componentFlags;
		unsigned char                  componentParamSize;
		short                          componentWhat;
		UInt32*                        format;
		ComponentInstance              idh;
	};
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=reset
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(pop)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack()
	#endif

	#if OLD_COMPONENT_GLUE
	struct IDHGetFormatGluePB myIDHGetFormatGluePB = {
		0,
		4,
		20
	};

	#else
	struct IDHGetFormatGluePB myIDHGetFormatGluePB;
//	*((unsigned long*)&myIDHGetFormatGluePB) = 0x00040014;

	myIDHGetFormatGluePB.componentFlags = 0;
	myIDHGetFormatGluePB.componentParamSize = 4;
	myIDHGetFormatGluePB.componentWhat = 20;	
	
	#endif

	myIDHGetFormatGluePB.format = format;
	myIDHGetFormatGluePB.idh = idh;

	#if TARGET_API_MAC_OS8
		return (ComponentResult)CallUniversalProc(CallComponentUPP, 0x000000F0, &myIDHGetFormatGluePB);
	#else
		return (ComponentResult)CallComponentDispatch( (ComponentParameters*)&myIDHGetFormatGluePB );
	#endif
}
#endif


