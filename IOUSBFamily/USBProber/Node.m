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

#import "Node.h"

@implementation Node
- (id)init
{
    self = [super init];
    children = [[NSMutableArray alloc] init];
    itemName = [[NSString alloc] init];
    itemValue = [[NSString alloc] init];
    return self;
}

- (void)setItemName:(NSString *)s
{
    [itemName release];
    if (s==nil)
        s = @"";
    itemName = [[NSString alloc] initWithString:s];
}

- (NSString *)itemName
{
    return itemName;
}

- (void)setItemValue:(NSString *)s
{
    [itemValue release];
    if (s==nil)
        s = @"";
    itemValue = [[NSString alloc] initWithString:s];
}

- (NSString *)itemValue
{
    return itemValue;
}

- (void)addChild:(Node *)n
{
    [children addObject:n];
}

- (Node *)childAtIndex:(UInt32)i
{
    if (i >= [children count]) {
        // uh oh, the index was out of bounds. this happens
        // if the devices are being updated while the last refresh
        // is not completed yet.
        // in this case, we return the last object of the array
        // then send a notification so the mainController knows that
        // it needs to reload the data one more time
        [[NSNotificationCenter defaultCenter] postNotificationName:@"com.apple.USBProber.general" object:@"DataNeedsReload"];
        return [children objectAtIndex:[children count]-1];
    }
    return [children objectAtIndex:i];
}

- (int)childrenCount
{
    return [children count];
}

- (BOOL)expandable
{
    return ([children count] > 0);
}

- (NSString *)stringRepresentationWithInitialIndent:(int)startingLevel recurse:(BOOL)recurse
{
    int i;
    NSMutableString *finalText = [[NSMutableString alloc] init];
    for (i=0; i < [self childrenCount]; i++) {
        int counter;
        for (counter=0; counter < startingLevel; counter++)
            [finalText appendString:@"    "];
        if ([[self childAtIndex:i] itemName] == NULL)
            [finalText appendString:[NSString stringWithFormat:@"%@\n",[[self childAtIndex:i] itemValue]]];
        else
            [finalText appendString:[NSString stringWithFormat:@"%@   %@\n",[[self childAtIndex:i] itemName],[[self childAtIndex:i] itemValue]]];
        if (recurse && [[self childAtIndex:i] expandable]) {
            [finalText appendString:[[self childAtIndex:i] stringRepresentationWithInitialIndent:startingLevel+1  recurse:YES]];
        }
    }
    return finalText;
}

- (void)clearNode
{
    [itemName release];
    [itemValue release];
    
    [children release];
    itemName = [[NSString alloc] init];
    itemValue = [[NSString alloc] init];
    children = [[NSMutableArray alloc] init];
}

- (void)dealloc
{
    [itemName release];
    [itemValue release];

    [children release];
    [super dealloc];
}
@end
