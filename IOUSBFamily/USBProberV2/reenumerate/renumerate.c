/*
 * Copyright © 2012 Apple Inc. All rights reserved.
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

#include <CoreFoundation/CoreFoundation.h>

#include <getopt.h>
#include <AssertMacros.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

#include "IOUSBFamilyInfoPlist.pch"

#define	vlog(x...)					if ( gVerbose ) { fprintf(stdout,x); }
#define	elog(x...)					fprintf(stderr, x)

//—————————————————————————————————————————————————————————————————————————————
//	Globals
//—————————————————————————————————————————————————————————————————————————————

const char *		gProgramName				= NULL;
boolean_t			gVerbose					= FALSE;
boolean_t			gDoReenumeration			= TRUE;
boolean_t			gDoSetConfiguration			= FALSE;
boolean_t			gDoSuspend					= FALSE;
boolean_t			gDoResume					= FALSE;
boolean_t			gDoReset					= FALSE;
boolean_t           gDoLocationID               = FALSE;
UInt8				gConfiguration				= 0;

//———————————————————————————————————————————————————————————————————————————
//	Prototypes
//———————————————————————————————————————————————————————————————————————————

static void ParseArguments ( int argc, const char * argv[] );
void PrintUsage ( void );

//———————————————————————————————————————————————————————————————————————————
//	PrintUsage
//———————————————————————————————————————————————————————————————————————————

void
PrintUsage ( void )
{
	elog ( "\n");
	elog ( "Usage: %s [OPTIONS] [vendor_id,product_id [vendor_id,product_id] [locationID [location ID]]...\n", gProgramName );
	elog ( "\n");
	
	elog ( "OPTIONS\n");
	elog ( "\tThe available options are as follows.  If no option is specified the values after the options are assumed to be a\n");
    elog ( "\tsequence of vendorID,productID pairs (in hex).  If the -l option is specified, the values after the options are assumed\n");
    elog ( "\tto be locationID's (in hex).  If no action option is specitied, a reenumerate command will be sent to the device(s):\n");
	elog ( "\n");
	
	
	elog ( "\t--locationID, -l\n");
	elog ( "\t\t The values after the options are locationIDs, instead of vendorID,productID.\n");

    elog ( "\t--configuration, -c\n");
	elog ( "\t\t Set the USB configuration to the value specified.\n");

	elog ( "\t--resume, -r\n");
	elog ( "\t\t Send a USB Resume to the device.\n");
	
	elog ( "\t--suspend, -s\n");
	elog ( "\t\t Send a USB Suspend to the device.\n");
	
	elog ( "\t--reset, -R\n");
	elog ( "\t\t Send a USB ResetDevice to the device.\n");
	
	elog ( "\t--verbose, -v\n");
	elog ( "\t\t Verbose mode.\n");
	
	elog ( "\t--version, -V\n");
	elog ( "\t\t Print version.\n");
	
	elog ( "\t--help, -h, -?\n");
	elog ( "\t\t Show this help.\n");
	
	elog ( "\n");
	
	elog ( "EXAMPLES\n");
    elog ( "\tSet the configuration of the device at vid: 0x05ac, pid: 0x1126 to 1:\n\n");
    elog ( "\t$ reenumerate -v -c 1 0x05ac,0x1126\n\n");
    elog ( "\tReenumerate the devices at locationIDs 0xfa144300 and 0xfd141310\n\n");
    elog ( "\t$ reenumerate -v -l 0xfa144300 0xfd141310\n");
	elog ( "\n");
	
	elog ( "EXAMPLES\n");
    elog ( "\tSet the configuration of the device at vid: 0x05ac, pid: 0x1126 to 1:\n\n");
    elog ( "\t$ reenumerate -v -c 1 0x05ac,0x1126\n\n");
    elog ( "\tReenumerate the devices at locationIDs 0xfa144300 and 0xfd141310\n\n");
    elog ( "\t$ reenumerate -v -l 0xfa144300 0xfd141310\n");
	exit ( 0 );
	
}


//———————————————————————————————————————————————————————————————————————————
//	ParseArguments
//———————————————————————————————————————————————————————————————————————————

static void
ParseArguments ( int argc, const char * argv[] )
{
	int 					c;
	struct option 			long_options[] =
	{
		{ "reset",			no_argument,		0, 'R' },
		{ "suspend",		no_argument,		0, 's' },
		{ "resume",			no_argument,		0, 'r' },
		{ "configuration",  required_argument,	0, 'c' },
		
        { "locationID",		no_argument,        0, 'l' },
		
		{ "verbose",		no_argument,		0, 'v' },
		{ "version",		no_argument,		0, 'V' },
		{ "help",			no_argument,		0, 'h' },
		{ 0, 0, 0, 0 }
	};
	
	if ( argc == 1 )
	{
        PrintUsage();
		return;
	}
	
    while ( ( c = getopt_long ( argc, ( char * const * ) argv , "Rsrc:lvVh?", long_options, NULL  ) ) != -1 )
	{
		switch ( c )
		{
			case 'R':
				gDoReset = TRUE;
				gDoReenumeration = FALSE;
				break;
				
			case 's':
				gDoSuspend = TRUE;
				gDoReenumeration = FALSE;
				break;
				
			case 'r':
				gDoResume = TRUE;
				gDoReenumeration = FALSE;
				break;
				
			case 'c':
				gDoSetConfiguration = TRUE;
				gConfiguration = (uint32_t)strtoul(optarg, NULL, 0);	// is this safe?
				break;
				
			case 'l':
				gDoLocationID = TRUE;
				break;
				
			case 'v':
				gVerbose = TRUE;
				vlog( "Verbose mode ON\n");
				break;
			case 'V':
				fprintf(stdout,"%s version:  %s\n", gProgramName, QUOTEDSTRING(USBTRACE_VERSION));
				break;
				
				
			case 'h':
				PrintUsage ( );
				break;
				
			case '?':
				PrintUsage ( );
				break;
				
			default:
				break;
		}
	}
}

void ProcessDevice(io_service_t aDevice)
{
    IOCFPlugInInterface			**plugInInterface;
	IOUSBDeviceInterface187		**deviceInterface;
    SInt32						score;
    HRESULT						res;
	CFNumberRef					numberObj;
	io_name_t					name;
	uint32_t					locationID = 0;
	IOReturn					kr;
	
	kr = IORegistryEntryGetName(aDevice, name);
	if ( kr != kIOReturnSuccess)
		return;
	
	numberObj = IORegistryEntryCreateCFProperty(aDevice, CFSTR("locationID"), kCFAllocatorDefault, 0);
	if ( numberObj )
	{
		CFNumberGetValue(numberObj, kCFNumberSInt32Type, &locationID);
		CFRelease(numberObj);
		
		vlog("Found \"%s\" @ 0x%8.8x\n", name, locationID);
		
	}
	
	kr = IOCreatePlugInInterfaceForService(aDevice, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plugInInterface, &score);
	IOObjectRelease(aDevice);
	if ((kIOReturnSuccess != kr) || !plugInInterface) {
		elog("IOCreatePlugInInterfaceForService returned 0x%08x.\n", kr);
		return;
	}
	
	// Use the plugin interface to retrieve the device interface.
	res = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID187), (LPVOID *)&deviceInterface);
	
	// Now done with the plugin interface.
	(*plugInInterface)->Release(plugInInterface);
	
	if (res || deviceInterface == NULL) {
		fprintf(stderr, "QueryInterface returned %d.\n", (int) res);
		return;
	}
	
	kr = (*deviceInterface)->USBDeviceOpen(deviceInterface);
	if(kr == kIOReturnSuccess)
	{
		if (gDoSetConfiguration)
		{
			vlog("Calling SetConfiguration(%d)\n", gConfiguration);
			kr = (*deviceInterface)->SetConfiguration(deviceInterface, gConfiguration);
			vlog("SetConfiguration(%d) returns 0x%8.8x\n", (uint32_t)gConfiguration, kr);
		}
		else if (gDoSuspend)
		{
			vlog("Calling USBDeviceSuspend(TRUE)\n");
			kr = (*deviceInterface)->USBDeviceSuspend(deviceInterface, TRUE);
			vlog("USBDeviceSuspend(TRUE) returns 0x%8.8x\n", kr);
		}
		else if (gDoResume)
		{
			vlog("Calling USBDeviceSuspend(FALSE)\n");
			kr = (*deviceInterface)->USBDeviceSuspend(deviceInterface, FALSE);
			vlog("USBDeviceSuspend(FALSE) returns 0x%8.8x\n", kr);
		}
		else if(gDoReset)
		{
			vlog("Calling ResetDevice\n");
			kr = (*deviceInterface)->ResetDevice(deviceInterface);
			vlog("ResetDevice returns 0x%8.8x\n", kr);
		}
		else
		{
			vlog("Calling USBDeviceReEnumerate\n");
			kr = (*deviceInterface)->USBDeviceReEnumerate(deviceInterface, 0);
			vlog("USBDeviceReEnumerate returns 0x%8.8x\n", kr);
		}
	}
	
	(void) (*deviceInterface)->USBDeviceClose(deviceInterface);
	
	(*deviceInterface)->Release(deviceInterface);
}

//================================================================================================
//	main
//================================================================================================
int main(int argc, const char *argv[] )
{
    CFMutableDictionaryRef 	matchingDict            = NULL;
   	CFMutableDictionaryRef  propertyMatchingDict    = NULL;
    CFNumberRef				numberRef;
	CFNumberRef                 locationIDRef           = NULL;
    kern_return_t			kr;
    uint32_t					usbVendor;
    uint32_t					usbProduct;
    uint32_t					deviceLocationID;
    const char*				param;
    char*					param2;
	int						paramIndex = 1;
	io_iterator_t			foundDevices;
	io_object_t				aDevice;
	
 	gProgramName = argv[0];
	
	// Get program arguments.
	ParseArguments ( argc, argv );
	
	paramIndex = optind;
	
	for( ; paramIndex < argc; paramIndex++ )
	{
		param = argv[paramIndex];
		
        matchingDict = IOServiceMatching(kIOUSBDeviceClassName);	// Interested in instances of class
        // IOUSBDevice and its subclasses
        if (matchingDict == NULL) {
            elog("IOServiceMatching returned NULL.\n");
            return -1;
        }
        
        if (!gDoLocationID)
        {
            usbVendor = (uint32_t) strtoul(param, &param2, 0);
            usbProduct = *param2++ ? (uint32_t) strtoul(param2, 0, 0) : 0;
            
            vlog("Looking for vid: 0x%x, pid: 0x%x\n", (uint32_t)usbVendor, (uint32_t)usbProduct);
            
            numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usbVendor);
            CFDictionarySetValue(matchingDict, CFSTR(kUSBVendorID), numberRef);
            CFRelease(numberRef);
            
            numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usbProduct);
            CFDictionarySetValue(matchingDict, CFSTR(kUSBProductID),  numberRef);
            CFRelease(numberRef);
        }
        else
        {
            deviceLocationID = (uint32_t) strtoul(param, &param2, 0);
            
            vlog("Looking for locationID: 0x%x\n", (uint32_t)deviceLocationID);
            
           propertyMatchingDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            __Require(propertyMatchingDict != NULL, ErrorExit);
            
            // Set the value in the dictionary of the property with the given key, or add the key
            // to the dictionary if it doesn't exist. This call retains the value object passed in.
            locationIDRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &deviceLocationID);
            __Require(locationIDRef != NULL, ErrorExit);
            CFDictionarySetValue(propertyMatchingDict, CFSTR("locationID"), locationIDRef);
			CFRelease(locationIDRef);
            
            // Now add the dictionary containing the matching value to our main
            // matching dictionary. This call will retain propertyMatchDict,
            CFDictionarySetValue(matchingDict, CFSTR(kIOPropertyMatchKey), propertyMatchingDict);
            
            // IOServiceGetMatchingServices retains the returned iterator, so release the iterator when we're done with it.
            // IOServiceGetMatchingServices also consumes a reference on the matching dictionary so we don't need to release
            // the dictionary explicitly.
        
        }
        
		kr = IOServiceGetMatchingServices(kIOMasterPortDefault,matchingDict, &foundDevices);	//consumes matchingDict reference
		matchingDict = NULL;
		if(kr)
		{
			elog("Error 0x%x trying to find matching services\n", kr);
			continue;
		}
		
		
		while ( (aDevice = IOIteratorNext(foundDevices)))
		{
			
			ProcessDevice(aDevice);
		}
		
	}
ErrorExit:
	
	if (matchingDict)
		CFRelease(matchingDict);
	
    return 0;
}
