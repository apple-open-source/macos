/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

/*!
 * @header GetMACAddress
 * Implements the extraction of the MAC Address from the en0 interface to be
 * used as an identifier. Adapted from sample code at Apple.
 */

#include "GetMACAddress.h"

static kern_return_t FindEthernetInterfaces(io_iterator_t *matchingServices);
static kern_return_t GetMACAddress(io_iterator_t intfIterator, UInt8 *MACAddress);

// Returns an iterator across all known Ethernet interfaces. Caller is responsible for
// releasing the iterator when iteration is complete.
static kern_return_t FindEthernetInterfaces(io_iterator_t *matchingServices)
{
    kern_return_t			kernResult		= KERN_FAILURE; 
    mach_port_t				masterPort		= MACH_PORT_NULL;
    CFMutableDictionaryRef	classesToMatch	= NULL;

    kernResult = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if ( kernResult == KERN_SUCCESS )
	{
		// Ethernet interfaces are instances of class kIOEthernetInterfaceClass
		classesToMatch = IOServiceMatching(kIOEthernetInterfaceClass);
		if ( classesToMatch != NULL )
		{
			kernResult = IOServiceGetMatchingServices(masterPort, classesToMatch, matchingServices);    
		}
	}
	
    return kernResult;
}
    
// Given an iterator across a set of Ethernet interfaces, return the MAC address of the first one.
// If no interfaces are found the MAC address is set to an empty string.
static kern_return_t GetMACAddress(io_iterator_t intfIterator, UInt8 *MACAddress)
{
    io_object_t		intfService			= MACH_PORT_NULL;
    io_object_t		controllerService	= MACH_PORT_NULL;
    kern_return_t	kernResult			= KERN_FAILURE;
    
	intfService = IOIteratorNext(intfIterator);
	if ( intfService != MACH_PORT_NULL )
    {
        CFTypeRef	MACAddressAsCFData = NULL;        

        // IONetworkControllers can't be found directly by the IOServiceGetMatchingServices call, 
        // matching mechanism. So we've found the IONetworkInterface and will get its parent controller
        // by asking for it specifically.
        
        kernResult = IORegistryEntryGetParentEntry( intfService,
                                                    kIOServicePlane,
                                                    &controllerService );

        if ( kernResult == KERN_SUCCESS )
		{
            MACAddressAsCFData = IORegistryEntryCreateCFProperty( controllerService,
                                                                  CFSTR(kIOMACAddress),
                                                                  kCFAllocatorDefault,
                                                                  0);
            if (MACAddressAsCFData != NULL)
            {
                CFDataGetBytes(MACAddressAsCFData, CFRangeMake(0, kIOEthernetAddressSize), MACAddress);
                CFRelease(MACAddressAsCFData);
                
            }
            (void) IOObjectRelease(controllerService);
        }
        
        // after use release it now.
        (void) IOObjectRelease(intfService);
    }
        
    return kernResult;
}


sInt32 GetEthernetAddress( CFStringRef *theLZMACAddress, CFStringRef *theNLZMACAddress)
{
    kern_return_t	kernResult		= KERN_SUCCESS;
    io_iterator_t	intfIterator	= MACH_PORT_NULL;
    unsigned char	macAddress[ kIOEthernetAddressSize ];
 
    kernResult = FindEthernetInterfaces(&intfIterator);
    if (kernResult == KERN_SUCCESS)
    {
        kernResult = GetMACAddress(intfIterator, &macAddress[0]);
		if (kernResult == KERN_SUCCESS)
		{
			if (theLZMACAddress != nil)
				*theLZMACAddress	= GetMACAddressFormattedStr(&macAddress[0], true);
			if (theNLZMACAddress != nil)
				*theNLZMACAddress	= GetMACAddressFormattedStr(&macAddress[0], false);
		}

		IOObjectRelease(intfIterator);	// Release the iterator
    }
    
    return kernResult;
}

CFStringRef GetMACAddressFormattedStr(unsigned char* addr, bool bLeadingZeros)
{
    // will return a string of the form "00:11:aa:bb:cc:22"
	//32 should be more than large enough
    char			buffer[32];
    char		   *p			= buffer;
    short			i;
    CFStringRef 	outString;
    

	if (bLeadingZeros)
	{
		for (i = 0; i < 6; ++i )
			p += sprintf( p, "%2.2x:", addr[i] );
	}
	else
	{
		for (i = 0; i < 6; ++i )
			p += sprintf( p, "%x:", addr[i] );
	}

    //  overwrite final ":" with a NULL terminator
    *(--p) = '\0';

    outString = CFStringCreateWithCString(kCFAllocatorDefault, buffer, kCFStringEncodingMacRoman);

    return outString;
}

