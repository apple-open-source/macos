/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import "Node.h"
#import <stdio.h>
#import <unistd.h>                                   // (getopt, ...)
#import <stdlib.h>
#import <IOKit/IOKitLib.h>
#import "NodeOutput.h"

struct Options
{
    char * class;                                     // (-c option)
    UInt32 flags;                                     // (see above)
    char * name;                                      // (-n option)
    char * plane;                                     // (-p option)
    UInt32 width;                                     // (-w option)
};

typedef struct Options Options;

struct context
{
    UInt32 depth;
    UInt64 stackOfBits;
};

@interface IORegistryClass : NodeOutput {
    IBOutlet id ov;
}

+ (Node *)ioregRootNode;
+ (void)doIOReg:(int)plane;  // 0==IOUSB plane, 1==IOService plane
+ (kern_return_t)DumpIOUSBPlane:(mach_port_t)iokitPort options:(Options)options;
+ (void)scan:(io_registry_entry_t)service serviceHasMoreSiblings:(Boolean)serviceHasMoreSiblings serviceDepth:(UInt32)serviceDepth stackOfBits:(UInt64)stackOfBits options:(Options)options;
+ (void)show:(io_registry_entry_t)service serviceDepth:(UInt32)serviceDepth stackOfBits:(UInt64)stackOfBits options:(Options)options;
+ (kern_return_t)ScanUSBDevices:(io_iterator_t)intfIterator options:(Options)options;
kern_return_t FindUSBPCIDevice(mach_port_t masterPort, io_iterator_t *matchingServices);
+ (void)indent:(Boolean)isNode depth:(UInt32)depth stackOfBits:(UInt64)stackOfBits;

- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item;
- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item;
- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item;
- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item;

@end
