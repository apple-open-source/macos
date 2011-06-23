//
//  PortStatusGatherer.m
//  USBProber
//
//  Created by Russvogel on 10/14/10.
//  Copyright 2010 Apple. All rights reserved.
//

#import "PortStatusGatherer.h"


@interface PortStatusGatherer (Private)

@end

@implementation PortStatusGatherer

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

/*!
 @enum HubPortStatus
 @discussion Used to decode the Port Status and Change 
 */
enum {
    kHubPortConnection		= 0x0001,
    kHubPortEnabled			= 0x0002,
    kHubPortSuspend			= 0x0004,
    kHubPortOverCurrent		= 0x0008,
    kHubPortBeingReset		= 0x0010,
    kHubPortPower			= 0x0100,
    kHubPortLowSpeed		= 0x0200,
    kHubPortHighSpeed		= 0x0400,
    kHubPortTestMode		= 0x0800,
    kHubPortIndicator		= 0x1000,
	
    // these are the bits which cause the hub port state machine to keep moving
    kHubPortStateChangeMask		= kHubPortConnection | kHubPortEnabled | kHubPortSuspend | kHubPortOverCurrent | kHubPortBeingReset
};

struct IOUSBHubStatus {
    UInt16          statusFlags;
    UInt16          changeFlags;
};
typedef struct IOUSBHubStatus   IOUSBHubStatus;
typedef IOUSBHubStatus *    IOUSBHubStatusPtr;

typedef struct IOUSBHubStatus   IOUSBHubPortStatus;


- initWithListener:(id <PortStatusGathererListener>)listener rootNode:(OutlineViewNode *)rootNode 
{

    if (self = [super init]) 
	{
        _listener = listener;
        _rootNode = [rootNode retain];
	}

	return self;
}

- (void)dealloc 
{
	[_rootNode release];
	[super dealloc];
}

- (NSString *) decodePortStatus:(IOUSBHubPortStatus) portStatus
{
	NSMutableString * portStatusString = [[NSMutableString alloc] initWithCapacity:1];
	UInt16 status = portStatus.statusFlags;
	UInt16 change = portStatus.changeFlags;
	
	[portStatusString setString:@"STATUS(change): "];

	if (status & kHubPortConnection || change & kHubPortConnection)
	{
		NSString * tempIndicator = @"CONNECT ";
		if( change & kHubPortConnection)
			tempIndicator = [tempIndicator lowercaseString];
		[portStatusString appendString:tempIndicator];
	}
	if (status & kHubPortEnabled || change & kHubPortEnabled)
	{
		NSString * tempIndicator = @"ENABLE ";
		if( change & kHubPortEnabled)
			tempIndicator = [tempIndicator lowercaseString];
		[portStatusString appendString:tempIndicator];
	}
	if (status & kHubPortSuspend || change & kHubPortSuspend)
	{
		NSString * tempIndicator = @"SUSPEND ";
		if( change & kHubPortSuspend)
			tempIndicator = [tempIndicator lowercaseString];
		[portStatusString appendString:tempIndicator];
	}
	if (status & kHubPortOverCurrent || change & kHubPortOverCurrent)
	{
		NSString * tempIndicator = @"OVER-I ";
		if( change & kHubPortOverCurrent)
			tempIndicator = [tempIndicator lowercaseString];
		[portStatusString appendString:tempIndicator];
	}
	if (status & kHubPortBeingReset || change & kHubPortBeingReset)
	{
		NSString * tempIndicator = @"RESET ";
		if( change & kHubPortBeingReset)
			tempIndicator = [tempIndicator lowercaseString];
		[portStatusString appendString:tempIndicator];
	}

	if (status & kHubPortPower)
		[portStatusString appendString:@"POWER "];

	// Port Speed
	if (status & kHubPortLowSpeed)
		[portStatusString appendString:@"LowSpeed "];
	else if (status & kHubPortHighSpeed)
		[portStatusString appendString:@"HighSpeed "];
	else 
		[portStatusString appendString:@"FullSpeed "];

	if (status & kHubPortTestMode)
		[portStatusString appendString:@"TEST "];

	if (status & kHubPortIndicator)
		[portStatusString appendString:@"INDICATOR "];

	return portStatusString;
}


- (NSString *) PrintNameForPortAtLocation:(int) port withLocationID:(uint32_t) parentLocationID
{
	io_iterator_t				matchingServicesIterator;
    kern_return_t				kernResult; 
    CFMutableDictionaryRef		matchingDict;
    CFMutableDictionaryRef		propertyMatchDict;
    io_service_t				usbDeviceRef;
	uint32_t					locationID = 0;
	int							nibble = 5;
	NSMutableString *			returnString = [[NSMutableString alloc] initWithCapacity:1] ;
	[returnString setString:@""];
    
	// First, create the locationID for the port
	// Start looking at the nibble at the 3rd nibble for a 0
	while ( parentLocationID & (0xf << (4 * nibble)) )
	{
		nibble--;
	}
	
	locationID = parentLocationID | (port << (4*nibble));
									 
    // IOServiceMatching is a convenience function to create a dictionary with the key kIOProviderClassKey and 
    // the specified value.
    matchingDict = IOServiceMatching(kIOUSBDeviceClassName);
	
    if (NULL == matchingDict) 
	{
        NSLog(@"IOServiceMatching returned a NULL dictionary.\n");
		goto ErrorExit;
    }
    else 
	{
		propertyMatchDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0,
												  &kCFTypeDictionaryKeyCallBacks,
												  &kCFTypeDictionaryValueCallBacks);
    }
	
	if (NULL == propertyMatchDict)
	{
		NSLog(@"CFDictionaryCreateMutable returned a NULL dictionary.\n");
		goto ErrorExit;
	}
	else 
	{
		// Set the value in the dictionary of the property with the given key, or add the key 
		// to the dictionary if it doesn't exist. This call retains the value object passed in.
		CFNumberRef locationIDRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &locationID);
		CFDictionarySetValue(propertyMatchDict, CFSTR("locationID"), locationIDRef); 
		
		// Now add the dictionary containing the matching value to our main
		// matching dictionary. This call will retain propertyMatchDict, so we can release our reference 
		// on propertyMatchDict after adding it to matchingDict.
		CFDictionarySetValue(matchingDict, CFSTR(kIOPropertyMatchKey), propertyMatchDict);
		CFRelease(propertyMatchDict);
		CFRelease(locationIDRef);
	}
		
    // IOServiceGetMatchingServices retains the returned iterator, so release the iterator when we're done with it.
    // IOServiceGetMatchingServices also consumes a reference on the matching dictionary so we don't need to release
    // the dictionary explicitly.
    kernResult = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, &matchingServicesIterator);    
    if (KERN_SUCCESS != kernResult) 
	{
        NSLog(@"IOServiceGetMatchingServices returned 0x%08x\n", kernResult);
		goto ErrorExit;
    }

    while ( (usbDeviceRef = IOIteratorNext(matchingServicesIterator)) )
    {
		io_name_t	name;
		kernResult = IORegistryEntryGetName(usbDeviceRef, name);
		if (KERN_SUCCESS != kernResult) 
		{
			[returnString appendString: [NSString stringWithFormat:@"  Unknown"]];
		}
		else
		{
			[returnString appendString: [NSString stringWithFormat:@"  %s", name]];
		}
		IOObjectRelease(usbDeviceRef);			// no longer need this reference
    }
	
ErrorExit:
	return returnString;
}

- (IOReturn) dealWithDevice:(io_service_t) usbDeviceRef
{
    IOReturn						err;
    IOCFPlugInInterface				**iodev;		// requires <IOKit/IOCFPlugIn.h>
    IOUSBDeviceInterface245			**dev;
    SInt32							score;
	uint32_t						locationID = 0;
	uint32_t						ports = 0;
	CFNumberRef						numberObj;
	int								port;
    io_name_t						name;
	OutlineViewNode *				aDeviceNode = nil;
	OutlineViewNode *				aPortNode = nil;
	
    err = IOCreatePlugInInterfaceForService(usbDeviceRef, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &iodev, &score);
    if (err || !iodev)
    {
		NSLog(@"dealWithDevice: unable to create plugin. ret = %08x, iodev = %p\n", err, iodev);
    }
    err = (*iodev)->QueryInterface(iodev, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID245), (LPVOID)&dev);
	IODestroyPlugInInterface(iodev);				// done with this

    if (err || !dev)
    {
		NSLog(@"dealWithDevice: unable to create a device interface. ret = %08x, dev = %p\n", err, dev);
    }
	
	err = IORegistryEntryGetName(usbDeviceRef, name);
	numberObj = IORegistryEntryCreateCFProperty(usbDeviceRef, CFSTR("locationID"), kCFAllocatorDefault, 0);
	if ( numberObj )
	{
		CFNumberGetValue(numberObj, kCFNumberSInt32Type, &locationID);
		CFRelease(numberObj);

		// If the "Ports" property exists, use that.  If not, default to 7.
		numberObj = IORegistryEntryCreateCFProperty(usbDeviceRef, CFSTR("Ports"), kCFAllocatorDefault, 0);
		if ( numberObj )
		{
			CFNumberGetValue(numberObj, kCFNumberSInt32Type, &ports);
			CFRelease(numberObj);
		}
		else 
		{
			ports = 7;
		}

		aDeviceNode  =  [[OutlineViewNode alloc] initWithName:@"DeviceInfo" value:[NSString stringWithFormat:@"Hub (%s) @ 0x%8.8x with %d ports", name, locationID, ports]];
		[_rootNode addChild:aDeviceNode ];
		[aDeviceNode release];
	}	
			
	// Iterate through all the ports and get the status/change info
	for ( port = 1; port <= ports ; port++ )
	{
		IOUSBDevRequest		request;
		IOUSBHubPortStatus	status;
		
		usleep(1000);
		request.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBClass, kUSBOther);
		request.bRequest = kUSBRqGetStatus;
		request.wValue = 0;
		request.wIndex = port;
		request.wLength = sizeof(IOUSBHubPortStatus);
		request.pData = &status;
		request.wLenDone = 0;
		
		err = (*dev)->DeviceRequest(dev, &request);
		
		NSMutableString * portString = [[NSMutableString alloc] initWithCapacity:1];
		[portString setString: [NSString stringWithFormat:@"Port %d:  ", port]];
		
		if ( err == kIOReturnSuccess)
		{
			// Get things the right way round.
			status.statusFlags = USBToHostWord(status.statusFlags);
			status.changeFlags = USBToHostWord(status.changeFlags);
			[portString appendString: [NSString stringWithFormat:@"Status: 0x%4.4x  Change: 0x%4.4x", status.statusFlags, status.changeFlags]];

			if ( status.statusFlags & kHubPortConnection )
			{
				NSString * portAppendString = [self PrintNameForPortAtLocation:port withLocationID:locationID];
				[portString appendString: portAppendString];
				[portAppendString release];
			}
		}
		else if ( err == kIOReturnNotResponding )
		{
			[portString appendString:[NSString stringWithFormat:@" Not Responding"]];
			}
			else
			{
			[portString appendString: [NSString stringWithFormat:@"(*dev)->DeviceRequest err: 0x%4.4x ", err]];
			}								

		aPortNode  =  [[OutlineViewNode alloc] initWithName:@"PortInfo" value:portString];
		[aDeviceNode addChild:aPortNode ];
		[aPortNode release];
		[portString release];

		if ( err == kIOReturnSuccess)
		{
		
			NSString *bitStatusChangeString = [self decodePortStatus: status];
			
			if( [bitStatusChangeString length] > 0 )
			{
				OutlineViewNode *aNode  =  [[OutlineViewNode alloc] initWithName:@"StatusBits" value:bitStatusChangeString];
				[aPortNode addChild:aNode ];
				[aNode release];
				[bitStatusChangeString release];
			}
		}
	}
	
	err = (*dev)->Release(dev);
    if (err)
    {
		NSLog( @"dealWithDevice: error releasing device - %08x\n", err);
}
	
	return err;
}



- (IOReturn) gatherStatus
{
    IOReturn			err = kIOReturnSuccess;
    CFMutableDictionaryRef 	matchingDictionary = 0;		// requires <IOKit/IOKitLib.h>
    SInt32					bDeviceClass = 9;
    SInt32					bDeviceSubClass = 0;
    CFNumberRef				numberRef;
    io_iterator_t			iterator = 0;
    io_service_t			usbDeviceRef;
	
    [_rootNode removeAllChildren];
	
    
    matchingDictionary = IOServiceMatching(kIOUSBDeviceClassName);	// requires <IOKit/usb/IOUSBLib.h>
    if (!err && !matchingDictionary)
    {
        NSLog(@"DumpHubPortStatus: could not create matching dictionary\n");
        err = kIOReturnError;
    }
    numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &bDeviceClass);
    if (!err && !numberRef)
    {
        NSLog(@"DumpHubPortStatus: could not create CFNumberRef for vendor\n");
        err = kIOReturnError;
    }
	if( !err )
	{
		CFDictionaryAddValue(matchingDictionary, CFSTR(kUSBDeviceClass), numberRef);
		CFRelease(numberRef);
		numberRef = 0;
		numberRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &bDeviceSubClass);
		if (!numberRef)
		{
			NSLog(@"DumpHubPortStatus: could not create CFNumberRef for product\n");
			err = kIOReturnError;
		}
    }
	if( !err )
	{
		CFDictionaryAddValue(matchingDictionary, CFSTR(kUSBDeviceSubClass), CFSTR("*"));
		CFRelease(numberRef);
		numberRef = 0;
		
		err = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDictionary, &iterator);
		matchingDictionary = 0;			// this was consumed by the above call
    }
	if( !err )
	{
    
		while ( (usbDeviceRef = IOIteratorNext(iterator)) )
		{
			[self dealWithDevice:usbDeviceRef];
			IOObjectRelease(usbDeviceRef);			// no longer need this reference
		}
    }
	
    IOObjectRelease(iterator);
    iterator = 0;
    
    return err;
}

@end
