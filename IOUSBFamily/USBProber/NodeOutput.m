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

#import "NodeOutput.h"


@implementation NodeOutput

+(void)PrintKeyVal:(char *)name val:(char *)val  forDevice:(int)deviceNumber atDepth:(int)depth forNode:(Node *)rootnode
{
    NSAutoreleasePool *pool=[[NSAutoreleasePool alloc] init];
    Node *aNode;
    Node *walker;
    int counter=0;

    aNode  =  [[Node alloc] init];
    //[aNode setItemName:[NSString stringWithFormat:@"%-20.20s", name]];
    [aNode setItemName:[NSString stringWithFormat:@"%s", name]];
    [aNode setItemValue:[NSString stringWithFormat:@"%s", val]];

    walker = [rootnode childAtIndex:deviceNumber];
    for (counter=0; counter < depth; counter++) {
        walker = [walker childAtIndex:[walker childrenCount]-1];
    }
    [walker addChild:aNode];
    [aNode release];
    [pool release];
    return;
}

+(void)PrintVal:(char *)val atDepth:(int)depth forNode:(Node *)rootnote
{
    NSAutoreleasePool *pool=[[NSAutoreleasePool alloc] init];
    Node *aNode;
    Node *walker;
    int counter=0;

    aNode  =  [[Node alloc] init];
    [aNode setItemName: NULL];
    [aNode setItemValue:[NSString stringWithFormat:@"%s", val]];

    walker = rootnote;
    for (counter=0; counter < depth; counter++) {
        walker = [walker childAtIndex:[walker childrenCount]-1];
    }
    [walker addChild:aNode];
    [aNode release];
    [pool release];
    return;
}

@end
