/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
 * IOSerialKeys.h
 *
 * 2000-10-21	gvdl	Initial real change to IOKit serial family.
 *
 */

/*
Sample Matching dictionary
{
    IOProviderClass = kIOSerialBSDServiceValue;
    kIOSerialBSDTypeKey = kIOSerialBSDAllTypes
			| kIOSerialBSDModemType
			| kIOSerialBSDRS232Type;
    kIOTTYDeviceKey = <Raw Unique Device Name>;
    kIOTTYBaseNameKey = <Raw Unique Device Name>;
    kIOTTYSuffixKey = <Raw Unique Device Name>;
    kIOCalloutDeviceKey = <Callout Device Name>;
    kIODialinDeviceKey = <Dialin Device Name>;
}

Note only the IOProviderClass is mandatory.  The other keys allow the searcher to reduce the size of the set of matching devices.
*/

/* Service Matching That is the 'IOProviderClass' */
#define kIOSerialBSDServiceValue	"IOSerialBSDClient"

/* Matching keys */
#define kIOSerialBSDTypeKey		"IOSerialBSDClientType"

/* Currently possible kIOSerialBSDTypeKey values. */
#define kIOSerialBSDAllTypes		"IOSerialStream"
#define kIOSerialBSDModemType		"IOModemSerialStream"
#define kIOSerialBSDRS232Type		"IORS232SerialStream"

// Properties that resolve to a /dev device node to open for
// a particular service
#define kIOTTYDeviceKey			"IOTTYDevice"
#define kIOTTYBaseNameKey		"IOTTYBaseName"
#define kIOTTYSuffixKey			"IOTTYSuffix"

#define kIOCalloutDeviceKey		"IOCalloutDevice"
#define kIODialinDeviceKey		"IODialinDevice"

// Property 'ioctl' wait for the tty device to go idle.
#define kIOTTYWaitForIdleKey		"IOTTYWaitForIdle"

#if KERNEL
extern const OSSymbol *gIOSerialBSDServiceValue;
extern const OSSymbol *gIOSerialBSDTypeKey;
extern const OSSymbol *gIOSerialBSDAllTypes;
extern const OSSymbol *gIOSerialBSDModemType;
extern const OSSymbol *gIOSerialBSDRS232Type;
extern const OSSymbol *gIOTTYDeviceKey;
extern const OSSymbol *gIOTTYBaseNameKey;
extern const OSSymbol *gIOTTYSuffixKey;
extern const OSSymbol *gIOCalloutDeviceKey;
extern const OSSymbol *gIODialinDeviceKey;
#endif /* KERNEL */
