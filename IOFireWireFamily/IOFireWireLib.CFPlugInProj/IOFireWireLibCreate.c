/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  IOFireWireLibCreate.h
 *  IOFireWireLib
 *
 *  Created by NWG on Thu May 25 2000.
 *  Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 */

#include <CoreFoundation/CoreFoundation.h>

// ============================================================
// IOFireWireLibCreate()
//
//	USAGE NOTE:
//		This function does all the work of loading and 
//		initializing the CFPlugIn containing the user client
//		code and upon success, returns a pointer to a new 
//		IOFireWireLibInterface object. Note that the returned 
//		object is not strictly an IOFireWireLibInterface object,
//		rather it is a subclass of class IOFireWireLibInterface
//		which contains the actual user client implementation.
//
//		See also: IOFireWireLibRelease() below.
// ============================================================

#include "IOFireWireLib.h"

IOFireWireInterfaceRef IOFireWireLibCreate()
{
	IOFireWireInterfaceRef	interface	= 0 ;
	CFPlugInRef				plugin		= 0 ;
	
	CFURLRef		url = CFURLCreateWithFileSystemPath(
								kCFAllocatorDefault,
								kIOFireWireLibPlugInPath, 
								kCFURLPOSIXPathStyle 	/* kCFUrlPathStyle */,
								true 					/* isDirectory */);
								
	
	#ifdef __FWUserClientDebug__
	::fprintf(stderr, "FWUserClientCreate says:\n") ;
	::fprintf(stderr, "\turl=%08lX\n", (UInt32) url) ;
	#endif

	plugin  = CFPlugInCreate(
						kCFAllocatorDefault 	/* allocator */,
						url 					/* plugInURL */);
						
	#ifdef __FWUserClientDebug__
	::fprintf(stderr, "FWUserClientCreate says:\n") ;
	::fprintf(stderr, "\tplugin=%08lX\n", (UInt32) plugin) ;
	#endif
	
	if ( nil != plugin )
	{
		// get the factory function from the plugin.
		CFArrayRef	factories	= CFPlugInFindFactoriesForPlugInTypeInPlugIn(
										kIOFireWireLibIUnknownTypeID, plugin) ;

		#ifdef __FWUserClientDebug__
		::fprintf(stderr, "\tfactories=%08lX\n", (UInt32)factories) ;
		#endif
		
		if ( nil != factories )						//
			if ( CFArrayGetCount(factories) > 0 )	// check that we have a factory
			{
				// get the ID for the factory
				CFUUIDRef factoryID	=
						(CFUUIDRef) CFArrayGetValueAtIndex(factories, 0) ;
						
				#ifdef __FWUserClientDebug__
					char 				tempStr[256] ;
					CFIndex				size 			= 255 ;
					CFStringRef			factoryIDString = CFUUIDCreateString(kCFAllocatorDefault, factoryID) ;
					
					::fprintf(
							stderr, 
							"\tfactoryID=%s\n", 
							CFStringGetCString(factoryIDString, tempStr, size, kCFStringEncodingMacRoman) ? tempStr : "???") ;
				#endif

				// instantiate an instance of the plugin and
				// get it's IUnknown interface
				IUnknown* iUnknown	= (IUnknown *) CFPlugInInstanceCreate(
											kCFAllocatorDefault,
											factoryID,
											kIOFireWireLibIUnknownTypeID) ;
				
				#ifdef __FWUserClientDebug__
				::fprintf(stderr, "\tiUnknown=%08lX\n", (UInt32)iUnknown) ;
				#endif

				if ( nil != iUnknown )
				{
					IOFireWireDeviceInterface*	tempInterface	= nil ;
					
					// ask the IUnknown to create a new instance of a subclass
					// of IOFireWireLibInterface
					#ifdef __cplusplus
					(iUnknown)->QueryInterface(
								CFUUIDGetUUIDBytes(kIOFireWireLibInterfaceID),
                                (LPVOID *)(& tempInterface) ) ;
					
					#else
					(iUnknown)->QueryInterface(iUnknown,
                                CFUUIDGetUUIDBytes(kIOFireWireLibInterfaceID),
                                (LPVOID *)(& tempInterface) ) ;
					#endif
	
					interface = tempInterface ;
				}
			}
	}
	
	#ifdef __FWUserClientDebug__
	::fprintf(stderr, "IOFireWireLibCreate returned %08lX\n", (UInt32) interface) ;
	#endif
	
	return interface;
}

/*// ============================================================
// IOFireWireLibRelease()
//
//	USAGE NOTE:
//		Use this function to deallocate/release an
//		IOFireWireLibInterface allocated by IOFireWireLibCreate().
// ============================================================

void IOFireWireLibRelease(IOFireWireInterfaceRef	inInterface)
{
	inInterface->header->ReleaseRef(inInterface) ;
}
*/
