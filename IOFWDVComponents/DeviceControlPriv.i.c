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
     File:       DeviceControlPriv.i.c
 
     Contains:   xxx put contents here xxx
 
     Version:    xxx put version here xxx
 
     DRI:        xxx put dri here xxx
 
     Copyright:  © 1999-2001 by Apple Computer, Inc., all rights reserved.
 
     Warning:    *** APPLE INTERNAL USE ONLY ***
                 This file contains unreleased SPI's
 
     BuildInfo:  Built by:            wgulland
                 On:                  Tue Mar 12 16:49:05 2002
                 With Interfacer:     3.0d35   (Mac OS X for PowerPC)
                 From:                DeviceControlPriv.i
                     Revision:        3
                     Dated:           6/15/99
                     Last change by:  KW
                     Last comment:    fix screwup
 
     Bugs:       Report bugs to Radar component "System Interfaces", "Latest"
                 List the version information (from above) in the Problem Description.
 
*/

#include <CoreServices/CoreServices.h>
//#include <CarbonCore/MixedMode.h>
//#include <CarbonCore/Components.h>
#include <DeviceControlPriv.h>
#if MP_SUPPORT
	#include "MPMixedModeSupport.h"
#endif

#define TOOLBOX_TRAPADDRESS(trapNum) (*(((UniversalProcPtr*)(((trapNum & 0x03FF) << 2) + 0xE00))))
#define OS_TRAPADDRESS(trapNum)      (*(((UniversalProcPtr*)(((trapNum & 0x00FF) << 2) + 0x400))))

#ifndef TRAPGLUE_NO_COMPONENT_CALL
DEFINE_API( ComponentResult ) DeviceControlEnableAVCTransactions(ComponentInstance instance)
{
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=mac68k
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(push, 2)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack(2)
	#endif
	struct DeviceControlEnableAVCTransactionsGluePB {
		unsigned char                  componentFlags;
		unsigned char                  componentParamSize;
		short                          componentWhat;
		ComponentInstance              instance;
	};
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=reset
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(pop)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack()
	#endif

	#if OLD_COMPONENT_GLUE
	struct DeviceControlEnableAVCTransactionsGluePB myDeviceControlEnableAVCTransactionsGluePB = {
		0,
		0,
		256
	};

	#else
	struct DeviceControlEnableAVCTransactionsGluePB myDeviceControlEnableAVCTransactionsGluePB;
	*((unsigned long*)&myDeviceControlEnableAVCTransactionsGluePB) = 0x00000100;
	
	myDeviceControlEnableAVCTransactionsGluePB.componentFlags = 0;
	myDeviceControlEnableAVCTransactionsGluePB.componentParamSize = 0;
	myDeviceControlEnableAVCTransactionsGluePB.componentWhat = 256;
		
	#endif

	myDeviceControlEnableAVCTransactionsGluePB.instance = instance;

	#if TARGET_API_MAC_OS8
		return (ComponentResult)CallUniversalProc(CallComponentUPP, 0x000000F0, &myDeviceControlEnableAVCTransactionsGluePB);
	#else
		return (ComponentResult)CallComponentDispatch( (ComponentParameters*)&myDeviceControlEnableAVCTransactionsGluePB );
	#endif
}
#endif


#ifndef TRAPGLUE_NO_COMPONENT_CALL
DEFINE_API( ComponentResult ) DeviceControlDisableAVCTransactions(ComponentInstance instance)
{
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=mac68k
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(push, 2)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack(2)
	#endif
	struct DeviceControlDisableAVCTransactionsGluePB {
		unsigned char                  componentFlags;
		unsigned char                  componentParamSize;
		short                          componentWhat;
		ComponentInstance              instance;
	};
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=reset
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(pop)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack()
	#endif

	#if OLD_COMPONENT_GLUE
	struct DeviceControlDisableAVCTransactionsGluePB myDeviceControlDisableAVCTransactionsGluePB = {
		0,
		0,
		257
	};

	#else
	struct DeviceControlDisableAVCTransactionsGluePB myDeviceControlDisableAVCTransactionsGluePB;
//	*((unsigned long*)&myDeviceControlDisableAVCTransactionsGluePB) = 0x00000101;
	
	myDeviceControlDisableAVCTransactionsGluePB.componentFlags = 0;
	myDeviceControlDisableAVCTransactionsGluePB.componentParamSize = 0;
	myDeviceControlDisableAVCTransactionsGluePB.componentWhat = 257;
	
	#endif

	myDeviceControlDisableAVCTransactionsGluePB.instance = instance;

	#if TARGET_API_MAC_OS8
		return (ComponentResult)CallUniversalProc(CallComponentUPP, 0x000000F0, &myDeviceControlDisableAVCTransactionsGluePB);
	#else
		return (ComponentResult)CallComponentDispatch( (ComponentParameters*)&myDeviceControlDisableAVCTransactionsGluePB );
	#endif
}
#endif


#ifndef TRAPGLUE_NO_COMPONENT_CALL
DEFINE_API( ComponentResult ) DeviceControlSetDeviceConnectionID(ComponentInstance instance, DeviceConnectionID connectionID)
{
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=mac68k
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(push, 2)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack(2)
	#endif
	struct DeviceControlSetDeviceConnectionIDGluePB {
		unsigned char                  componentFlags;
		unsigned char                  componentParamSize;
		short                          componentWhat;
		DeviceConnectionID             connectionID;
		ComponentInstance              instance;
	};
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=reset
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(pop)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack()
	#endif

	#if OLD_COMPONENT_GLUE
	struct DeviceControlSetDeviceConnectionIDGluePB myDeviceControlSetDeviceConnectionIDGluePB = {
		0,
		4,
		258
	};

	#else
	struct DeviceControlSetDeviceConnectionIDGluePB myDeviceControlSetDeviceConnectionIDGluePB;
//	*((unsigned long*)&myDeviceControlSetDeviceConnectionIDGluePB) = 0x00040102;

	myDeviceControlSetDeviceConnectionIDGluePB.componentFlags = 0;
	myDeviceControlSetDeviceConnectionIDGluePB.componentParamSize = 4;
	myDeviceControlSetDeviceConnectionIDGluePB.componentWhat = 258;
		
	#endif

	myDeviceControlSetDeviceConnectionIDGluePB.connectionID = connectionID;
	myDeviceControlSetDeviceConnectionIDGluePB.instance = instance;

	#if TARGET_API_MAC_OS8
		return (ComponentResult)CallUniversalProc(CallComponentUPP, 0x000000F0, &myDeviceControlSetDeviceConnectionIDGluePB);
	#else
		return (ComponentResult)CallComponentDispatch( (ComponentParameters*)&myDeviceControlSetDeviceConnectionIDGluePB );
	#endif
}
#endif


#ifndef TRAPGLUE_NO_COMPONENT_CALL
DEFINE_API( ComponentResult ) DeviceControlGetDeviceConnectionID(ComponentInstance instance, DeviceConnectionID* connectionID)
{
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=mac68k
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(push, 2)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack(2)
	#endif
	struct DeviceControlGetDeviceConnectionIDGluePB {
		unsigned char                  componentFlags;
		unsigned char                  componentParamSize;
		short                          componentWhat;
		DeviceConnectionID*            connectionID;
		ComponentInstance              instance;
	};
	#if PRAGMA_STRUCT_ALIGN
	  #pragma options align=reset
	#elif PRAGMA_STRUCT_PACKPUSH
	  #pragma pack(pop)
	#elif PRAGMA_STRUCT_PACK
	  #pragma pack()
	#endif

	#if OLD_COMPONENT_GLUE
	struct DeviceControlGetDeviceConnectionIDGluePB myDeviceControlGetDeviceConnectionIDGluePB = {
		0,
		4,
		259
	};

	#else
	struct DeviceControlGetDeviceConnectionIDGluePB myDeviceControlGetDeviceConnectionIDGluePB;
//	*((unsigned long*)&myDeviceControlGetDeviceConnectionIDGluePB) = 0x00040103;
	
	myDeviceControlGetDeviceConnectionIDGluePB.componentFlags = 0;
	myDeviceControlGetDeviceConnectionIDGluePB.componentParamSize = 4;
	myDeviceControlGetDeviceConnectionIDGluePB.componentWhat = 259;
		
	#endif

	myDeviceControlGetDeviceConnectionIDGluePB.connectionID = connectionID;
	myDeviceControlGetDeviceConnectionIDGluePB.instance = instance;

	#if TARGET_API_MAC_OS8
		return (ComponentResult)CallUniversalProc(CallComponentUPP, 0x000000F0, &myDeviceControlGetDeviceConnectionIDGluePB);
	#else
		return (ComponentResult)CallComponentDispatch( (ComponentParameters*)&myDeviceControlGetDeviceConnectionIDGluePB );
	#endif
}
#endif


