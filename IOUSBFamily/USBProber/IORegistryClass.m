/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#import "IORegistryClass.h"

static Node *ioregRootNode = nil;

const UInt32 	kIORegFlagShowBold       = (1 << 0);     // (-b option)
const UInt32 	kIORegFlagShowProperties = (1 << 1);     // (-l option)
const UInt32 	kIORegFlagShowState      = (1 << 2);     // (-s option)
mach_port_t	iokitPort;
kern_return_t	kernResult = KERN_SUCCESS;
io_iterator_t	intfIterator;
struct Options  options;

///////////////////////////////////////////////////////////////////////////

@implementation IORegistryClass

+ (Node *)ioregRootNode
{
    return ioregRootNode;
}

+ (void)doIOReg:(int)plane  // 0==IOUSB plane, 1==IOService plane
{
    NSAutoreleasePool *pool=[[NSAutoreleasePool alloc] init];
    
    [ioregRootNode clearNode];

    [ioregRootNode setItemName: NULL];
    [ioregRootNode setItemValue: @"IORegistry"];
    
    // Obtain the command-line arguments.

    options.class = 0;
    options.flags = 0;
    options.name  = 0;
    options.plane = kIOServicePlane;
    options.width = 132;
    kernResult = IOMasterPort(bootstrap_port, &iokitPort);
    switch (plane) {
        case 0:
            options.plane = "IOUSB";
            kernResult = [self DumpIOUSBPlane:iokitPort options:options];
            break;
        default:
            //kernResult = FindUSBPCIDevice(iokitPort, &intfIterator);
            kernResult = FindUSBPCIDevice(iokitPort, &intfIterator);

            if (KERN_SUCCESS != kernResult)
                printf("FindUSBNode returned 0x%08x\n", kernResult);
            else
            {
                //kernResult = ScanUSBDevices(intfIterator, options);
                kernResult = [self ScanUSBDevices:intfIterator options:options];
                if (KERN_SUCCESS != kernResult)
                    printf("GetMACAddress returned 0x%08x\n", kernResult);
            }
                IOObjectRelease(intfIterator);
            break;
    }
    [pool release];
}

+ (kern_return_t)DumpIOUSBPlane:(mach_port_t)iokitPort options:(Options)options
{
    io_object_t 	service;

    // Obtain the I/O Kit root service.

    service = IORegistryGetRootEntry(iokitPort);
    //assertion(service, "can't obtain I/O Kit's root service");


    [self scan:service serviceHasMoreSiblings:FALSE serviceDepth:0 stackOfBits:0 options:options];
//   scan(   service,	/* service                */
//        FALSE,		/* serviceHasMoreSiblings */
//        0,			/* serviceDepth           */
//        0,			/* stackOfBits            */
//        options 		/* options 		  */
//        );

return KERN_SUCCESS;
}

+ (void)scan:(io_registry_entry_t)service serviceHasMoreSiblings:(Boolean)serviceHasMoreSiblings serviceDepth:(UInt32)serviceDepth stackOfBits:(UInt64)stackOfBits options:(Options)options
{
    io_registry_entry_t child       = 0; // (needs release)
    io_registry_entry_t childUpNext = 0; // (don't release)
    io_iterator_t       children    = 0; // (needs release)
    kern_return_t       status      = KERN_SUCCESS;

    status = IORegistryEntryGetChildIterator(service, options.plane, &children);

    childUpNext = IOIteratorNext(children);

    if (serviceHasMoreSiblings)
        stackOfBits |=  (1 << serviceDepth);
    else
        stackOfBits &= ~(1 << serviceDepth);

    if (childUpNext)
        stackOfBits |=  (2 << serviceDepth);
    else
        stackOfBits &= ~(2 << serviceDepth);
    [self show:service serviceDepth:serviceDepth stackOfBits:stackOfBits options:options];
    //show(service, serviceDepth, stackOfBits, options);

    while (childUpNext)
    {
        child       = childUpNext;
        childUpNext = IOIteratorNext(children);

        [self scan:child serviceHasMoreSiblings:((childUpNext) ? TRUE : FALSE) serviceDepth:(serviceDepth + 1) stackOfBits:stackOfBits options:options];
//        scan( /* service                */ child,
//              /* serviceHasMoreSiblings */ (childUpNext) ? TRUE : FALSE,
//              /* serviceDepth           */ serviceDepth + 1,
//              /* stackOfBits            */ stackOfBits,
//              /* options                */ options );

        IOObjectRelease(child); child = 0;
    }

    IOObjectRelease(children); children = 0;
}

+ (void)show:(io_registry_entry_t)service serviceDepth:(UInt32)serviceDepth stackOfBits:(UInt64)stackOfBits options:(Options)options
{
    io_name_t       class;          // (don't release)
                                    //struct context  context    = { serviceDepth, stackOfBits };
    int             integer    = 0; // (don't release)
    io_name_t       name;           // (don't release)
                                    //CFDictionaryRef properties = 0; // (needs release)
    kern_return_t   status     = KERN_SUCCESS;
    io_name_t       location;       // (don't release)
    static char	buf[350], tempbuf[350];

    // printf out the name of the service.

    status = IORegistryEntryGetNameInPlane(service, options.plane, name);

    //indent(TRUE, serviceDepth, stackOfBits);
    //[self indent:TRUE depth:serviceDepth stackOfBits:stackOfBits];

    sprintf((char *)tempbuf, (char *)name);
    strcat(buf,tempbuf);

    status = IORegistryEntryGetLocationInPlane(service, options.plane, location);
    if (status == KERN_SUCCESS)  {
        sprintf((char *)tempbuf, "@%s", location);
        strcat(buf,tempbuf); 
    }
    

    status = IOObjectGetClass(service, class);

    sprintf((char *)tempbuf, "  <class %s", class);
    strcat(buf,tempbuf);
    
    if (options.flags & kIORegFlagShowState)
    {
        if (IOObjectConformsTo(service, "IOService"))
        {
            status = IOServiceGetBusyState(service, &integer);

            sprintf((char *)tempbuf, ", busy %d", integer);
            strcat(buf,tempbuf);
        }

        integer = IOObjectGetRetainCount(service);

        sprintf((char *)tempbuf, ", retain count %d", integer);
        strcat(buf,tempbuf);
    }
    
    sprintf((char *)tempbuf, ">");
    strcat(buf,tempbuf);
    [self PrintVal:buf atDepth:serviceDepth forNode:ioregRootNode];
    tempbuf[0]=0;
    buf[0]=0;
}

+ (kern_return_t)ScanUSBDevices:(io_iterator_t)intfIterator options:(Options)options
{

    io_object_t		intfService;
    kern_return_t	kernResult = KERN_FAILURE;

    while ( (intfService = IOIteratorNext(intfIterator)) )
    {

        io_name_t		deviceName;
        CFMutableDictionaryRef properties = 0; // (needs release)
        CFDataRef     		asciiClass     = 0; // (don't release)
        const UInt8 * bytes;
        CFIndex       length;
        UInt32			classCode = 0;

        // Obtain the service's properties.

        kernResult = IORegistryEntryCreateCFProperties(intfService,
                                                       &properties,
                                                       kCFAllocatorDefault,
                                                       kNilOptions);

        asciiClass = (CFDataRef) CFDictionaryGetValue( properties, CFSTR( "class-code" ) );
        if ( asciiClass && (CFGetTypeID(asciiClass) == CFDataGetTypeID()) )
        {
            length = CFDataGetLength(asciiClass);
            bytes  = CFDataGetBytePtr(asciiClass);
            if ( length == 4 )
                classCode = * ((UInt32 *) bytes);
        }
        CFRelease(properties);

        // Get the service name to see if it's "usb"
        //
        kernResult = IORegistryEntryGetName(intfService, deviceName);
        if (KERN_SUCCESS != kernResult)
        {
            deviceName[0] = '\0';
        }


        //if ( !strcmp( deviceName, "usb") )
        if ( (classCode == 0x000c0310) || (classCode == 0x000c0320) )
        {
            [self scan:intfService serviceHasMoreSiblings:FALSE serviceDepth:0 stackOfBits:0 options:options];
        }
    }

return KERN_SUCCESS;
}

kern_return_t FindUSBPCIDevice(mach_port_t masterPort, io_iterator_t *matchingServices)
{
    kern_return_t		kernResult;
    CFMutableDictionaryRef	classesToMatch;

    // USB controllers are of type IOPCIDevice
    //
    classesToMatch = IOServiceMatching("IOPCIDevice");

    if (classesToMatch == NULL)
        printf("IOServiceMatching returned a NULL dictionary.\n");

    kernResult = IOServiceGetMatchingServices(masterPort, classesToMatch, &intfIterator);
    if (KERN_SUCCESS != kernResult)
        printf("IOServiceGetMatchingServices returned %d\n", kernResult);

    return kernResult;
}


+ (void)indent:(Boolean)isNode depth:(UInt32)depth stackOfBits:(UInt64)stackOfBits
{
    // stackOfBits representation, given current zero-based depth is n:
    //   bit n+1             = does depth n have children?       1=yes, 0=no
    //   bit [n, .. i .., 0] = does depth i have more siblings?  1=yes, 0=no

    UInt32 index;

    if (isNode)
    {
        for (index = 0; index < depth; index++)
            printf( (stackOfBits & (1 << index)) ? "| " : "  " );

        printf("+-o ");
    }
    else // if (!isNode)
    {
        for (index = 0; index <= depth + 1; index++)
            printf( (stackOfBits & (1 << index)) ? "| " : "  " );
    }
}

- init
{
    [super init];
    ioregRootNode = [[Node alloc] init];
    return self;
}


- (id)outlineView:(NSOutlineView *)ov child:(int)index ofItem:(id)item
{
    if (item)
        return [item childAtIndex:index];
    else
        return ioregRootNode;
}

- (BOOL)outlineView:(NSOutlineView *)ov isItemExpandable:(id)item
{
    return [item expandable];
}

- (int)outlineView:(NSOutlineView *)ov numberOfChildrenOfItem:(id)item
{
    if (item == nil) {
        return 1;
    }
    return [item childrenCount];
}

- (id)outlineView:(NSOutlineView *)ov objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
    return [item itemValue];

}

- (void)dealloc
{
    [ioregRootNode release];
    [super dealloc];
}

@end
