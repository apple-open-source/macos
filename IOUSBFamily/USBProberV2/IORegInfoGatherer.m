/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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


#import "IORegInfoGatherer.h"

void dumpIOUSBPlane(mach_port_t iokitPort, OutlineViewNode *rootNode, char * plane);
void scan(io_registry_entry_t service, Boolean serviceHasMoreSiblings, UInt32 serviceDepth, UInt64 stackOfBits, OutlineViewNode * rootNode, char * plane);
void show(io_registry_entry_t service, UInt32 serviceDepth, UInt64 stackOfBits, OutlineViewNode * rootNode, char * plane);
kern_return_t findUSBPCIDevice(mach_port_t masterPort, io_iterator_t *matchingServices);
void scanUSBDevices(io_iterator_t intfIterator, OutlineViewNode * rootNode, char * plane);


@implementation IORegInfoGatherer

static void DeviceAdded(void *refCon, io_iterator_t iterator)
{
    io_service_t ioDeviceObj = IO_OBJECT_NULL;
    
    while( ioDeviceObj = IOIteratorNext( iterator) )
    {
        IOObjectRelease( ioDeviceObj );
    }
    [(IORegInfoGatherer *)refCon refreshData:false];
}

static void DeviceRemoved(void *refCon, io_iterator_t iterator)
{
    io_service_t ioDeviceObj = IO_OBJECT_NULL;
    
    while( (ioDeviceObj = IOIteratorNext( iterator)))
    {
        IOObjectRelease( ioDeviceObj );
    }
    [(IORegInfoGatherer *)refCon refreshData:false];
}

- initWithListener:(id <IORegGathererListener>)listener rootNode:(OutlineViewNode *)rootNode plane:(int)plane {
    if (self = [super init]) {
        _runLoopSource = NULL;
        _listener = listener;
        _plane = plane;
        _rootNode = [rootNode retain];
        
        if (listener == nil) {
            [NSException raise:@"IORegInfoGathererBadListener" format:@"Listener must be non-nil"];
            [self dealloc];
            self = nil;
        } else if (! [self registerForUSBNotifications]) {
            NSLog(@"USB Prober: IORegInfoGatherer was unable to register for USB notifications");
            [self dealloc];
            self = nil;
        } else {
            // no problems
            [self refreshData:true];
        }
    }
    return self;
}

- (void)dealloc {
    [self unregisterForUSBNotifications];
    [_rootNode release];
    [super dealloc];
}

- (BOOL)registerForUSBNotifications
{
    kern_return_t kr;
    mach_port_t masterPort;
    IONotificationPortRef gNotifyPort;
    io_iterator_t  gAddedIter,gRemovedIter;
    
    kr = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if (kr != KERN_SUCCESS) return NO;
    
    gNotifyPort = IONotificationPortCreate(masterPort);
    _runLoopSource = IONotificationPortGetRunLoopSource(gNotifyPort);
    CFRunLoopAddSource([[NSRunLoop currentRunLoop] getCFRunLoop], _runLoopSource, kCFRunLoopDefaultMode);
    kr = IOServiceAddMatchingNotification(gNotifyPort,
                                          kIOFirstMatchNotification,
                                          IOServiceMatching(kIOUSBDeviceClassName),
                                          DeviceAdded,
                                          self,
                                          &gAddedIter);
    if (kr != KERN_SUCCESS) return NO;
    
    kr = IOServiceAddMatchingNotification(gNotifyPort,
                                          kIOTerminatedNotification,
                                          IOServiceMatching(kIOUSBDeviceClassName),
                                          DeviceRemoved,
                                          self,
                                          &gRemovedIter);
    if (kr != KERN_SUCCESS) return NO;
    
    DeviceAdded(self, gAddedIter);
    DeviceRemoved(self, gRemovedIter);
    mach_port_deallocate(mach_task_self(), masterPort);
    masterPort = 0;
    return YES;
}

- (void)unregisterForUSBNotifications {
    if (_runLoopSource != NULL)
    {
        CFRunLoopSourceInvalidate(_runLoopSource);
        CFRelease(_runLoopSource);
    }
}

- (void)refreshData:(BOOL)shouldForce {
	NSAutoreleasePool	*pool = [[NSAutoreleasePool alloc] init];
    mach_port_t         iokitPort;
    kern_return_t	kernResult = KERN_SUCCESS;
    io_iterator_t	intfIterator;
    
    if (!shouldForce && [[NSUserDefaults standardUserDefaults] boolForKey:@"IORegistryDontAutoRefresh"] == YES) {
        return;
    }
    [_rootNode removeAllChildren];
    
    kernResult = IOMasterPort(bootstrap_port, &iokitPort);
    
    if (KERN_SUCCESS != kernResult)
        NSLog(@"USB Prober: IOMasterPort returned 0x%08x\n", kernResult);
    else {
        switch (_plane) {
            case kIOUSB_Plane:
                dumpIOUSBPlane(iokitPort, _rootNode, kIOUSBPlane);
                break;
            case kIOService_Plane:
                kernResult = findUSBPCIDevice(iokitPort, &intfIterator);
                
                if (KERN_SUCCESS != kernResult)
                    NSLog(@"USB Prober: FindUSBNode returned 0x%08x\n", kernResult);
                else
                {
                    scanUSBDevices(intfIterator, _rootNode, kIOServicePlane);
                }
                    IOObjectRelease(intfIterator);
                break;
            default:
                break;
        }
    }
    
    [_listener ioRegInfoGathererInformationDidChange:self];
	[pool release];
}

- (void)setPlane:(int)plane {
    _plane = plane;
}

void dumpIOUSBPlane(mach_port_t iokitPort, OutlineViewNode *rootNode, char * plane) {
    io_object_t 	service;
    
    // Obtain the I/O Kit root service.
    service = IORegistryGetRootEntry(iokitPort);
    
    scan(service, FALSE, 0, 0, rootNode, plane);
}

void scan(io_registry_entry_t service, Boolean serviceHasMoreSiblings, UInt32 serviceDepth, UInt64 stackOfBits, OutlineViewNode * rootNode, char * plane)
{
    io_registry_entry_t child       = 0; // (needs release)
    io_registry_entry_t childUpNext = 0; // (don't release)
    io_iterator_t       children    = 0; // (needs release)
    kern_return_t       status      = KERN_SUCCESS;
    
    status = IORegistryEntryGetChildIterator(service, plane, &children);
    
    childUpNext = IOIteratorNext(children);
    
    if (serviceHasMoreSiblings)
        stackOfBits |=  (1 << serviceDepth);
    else
        stackOfBits &= ~(1 << serviceDepth);
    
    if (childUpNext)
        stackOfBits |=  (2 << serviceDepth);
    else
        stackOfBits &= ~(2 << serviceDepth);
	
    show(service, serviceDepth, stackOfBits, rootNode, plane);
    
    while (childUpNext)
    {
        child       = childUpNext;
        childUpNext = IOIteratorNext(children);
        
        scan(child, ((childUpNext) ? TRUE : FALSE), (serviceDepth + 1), stackOfBits, rootNode, plane);
		
        //        scan( /* service                */ child,
        //              /* serviceHasMoreSiblings */ (childUpNext) ? TRUE : FALSE,
        //              /* serviceDepth           */ serviceDepth + 1,
        //              /* stackOfBits            */ stackOfBits,
        //              /* options                */ options );
        
        IOObjectRelease(child); child = 0;
    }
    
    IOObjectRelease(children); children = 0;
}

void show(io_registry_entry_t service, UInt32 serviceDepth, UInt64 stackOfBits, OutlineViewNode * rootNode, char * plane)
{
    io_name_t       class;          // (don't release)
                                    //struct context  context    = { serviceDepth, stackOfBits };
    io_name_t       name;           // (don't release)
                                    //CFDictionaryRef properties = 0; // (needs release)
    kern_return_t   status     = KERN_SUCCESS;
    io_name_t       location;       // (don't release)
    static char	buf[350], tempbuf[350];
	CFNumberRef		address;
	
	
 	address = IORegistryEntryCreateCFProperty(service, CFSTR("USB Address"), kCFAllocatorDefault, kNilOptions);
	if (address)  
	{
		UInt32	addr = 0;
		CFNumberGetValue((CFNumberRef)address, kCFNumberLongType, &addr);
        sprintf((char *)tempbuf, "%d: ", (uint32_t)addr);
        strcat(buf,tempbuf); 
		CFRelease(address);
		address = NULL;
    }
	
 	address = IORegistryEntryCreateCFProperty(service, CFSTR("USBBusNumber"), kCFAllocatorDefault, kNilOptions);
	if (address)  
	{
		UInt32	addr = 0;
		CFNumberGetValue((CFNumberRef)address, kCFNumberLongType, &addr);
        sprintf((char *)tempbuf, "0x%x: ", (uint32_t)addr);
        strcat(buf,tempbuf); 
		CFRelease(address);
		address = NULL;
    }
	
	status = IORegistryEntryGetNameInPlane(service, plane, name);
    if (status == KERN_SUCCESS)  {
        sprintf((char *)tempbuf, "%s", name);
        strcat(buf,tempbuf); 
    }
    
    status = IORegistryEntryGetLocationInPlane(service, plane, location);
    if (status == KERN_SUCCESS)  {
        sprintf((char *)tempbuf, "@%s", location);
        strcat(buf,tempbuf); 
    }
    
	
    status = IOObjectGetClass(service, class);
    
    sprintf((char *)tempbuf, "  <class %s>", class);
    strcat(buf,tempbuf);
	
    //[rootNode addNodeWithName:"" value:buf atDepth:serviceDepth];
    // IOObjectRetain(service);
    IORegOutlineViewNode *aNode  =  [[IORegOutlineViewNode alloc] initWithName:@"" value:[NSString stringWithCString:buf encoding:NSUTF8StringEncoding]];
    [rootNode addNode:aNode atDepth:serviceDepth];
    [aNode setRepresentedDevice:service];
    [aNode release];
	
    tempbuf[0]=0;
    buf[0]=0;
}

kern_return_t findUSBPCIDevice(mach_port_t masterPort, io_iterator_t * matchingServices)
{
    kern_return_t		kernResult;
    CFMutableDictionaryRef	classesToMatch;
    
    // USB controllers are of type IOPCIDevice
    //
    classesToMatch = IOServiceMatching("IOPCIDevice");
    
    if (classesToMatch == NULL)
        NSLog(@"USB Prober: IOServiceMatching returned a NULL dictionary.\n");
    
    kernResult = IOServiceGetMatchingServices(masterPort, classesToMatch, matchingServices);
    if (KERN_SUCCESS != kernResult)
        NSLog(@"USB Prober: IOServiceGetMatchingServices returned %d\n", kernResult);
    
    return kernResult;
}

void scanUSBDevices(io_iterator_t intfIterator, OutlineViewNode * rootNode, char * plane)
{
    io_object_t		intfService;
    
    while ( (intfService = IOIteratorNext(intfIterator)) )
    {
        CFDataRef					asciiClass     = 0; // (don't release)
        const UInt8 *				bytes;
        CFIndex						length;
		UInt32						classCode = 0;
        
        // Obtain the service's properties.
        asciiClass = (CFDataRef) IORegistryEntryCreateCFProperty(intfService,
																 CFSTR( "class-code" ),
																 kCFAllocatorDefault,
																 kNilOptions);
		
        if ( asciiClass && (CFGetTypeID(asciiClass) == CFDataGetTypeID()) )
        {
            length = CFDataGetLength(asciiClass);
            bytes  = CFDataGetBytePtr(asciiClass);
            if ( length == 4 )
			{
				classCode = * (UInt32 *) bytes;
			}
			CFRelease(asciiClass);
        }

		if ( (classCode == (0x000c0310)) || (classCode == (0x000c0320)) || (classCode == (0x000c0300)) || 
			 (classCode == (0x10030c00)) || (classCode == (0x20030c00)) || (classCode == (0x00030c00)) )
        {
            scan(intfService, FALSE, 0, 0, rootNode,plane);
        }
    }
}

@end
