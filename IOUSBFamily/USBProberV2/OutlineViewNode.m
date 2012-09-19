/*
 * Copyright © 1998-2012 Apple Inc.  All rights reserved.
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

#import "OutlineViewNode.h"


@implementation OutlineViewNode

- init {
    return [self initWithName:@"" value:@""];
}

- initWithName:(NSString *)name value:(NSString *)value {
    if (self = [super init]) {
        _name = [name retain];
        _value = [value retain];
        _children = [[NSMutableArray alloc] init];
    }
    return self;
}

- (void)dealloc {
    [_name release];
    [_value release];
    [_children release];
    [super dealloc];
}


- (NSString *)name 
{
    if (_name == nil)
    {
        [self setName:@""];
    }
    return _name;
}

- (NSString *)value 
{
    if (_value == nil)
    {
        [self setValue:@""];
    }
    return _value;
}

- (void)setName:(NSString *)aString {
    [_name release];
    if (aString == nil) {
        _name = [@"" retain];
    } else {
        _name = [aString retain];
    }
}

- (void)setValue:(NSString *)aString {
    [_value release];
    if (aString == nil) {
        _value = [@"" retain];
    } else {
        _value = [aString retain];
    }
}

- (void)addChild:(OutlineViewNode *)aNode {
    if (aNode != nil) {
        [_children addObject:aNode]; 
    }
}

- (int)childrenCount {
    return [_children count];
}

- (NSArray *)children {
    return _children;
}

- (OutlineViewNode *)childAtIndex:(int)i {
    if (i >= [_children count]) {
        return nil;
    } else {
        return [_children objectAtIndex:i];
    }
}

- (void)removeAllChildren {
    [_children removeAllObjects];
}

- (BOOL)isExpandable {
    return [_children count] != 0;
}

- (void)addNode:(OutlineViewNode *)aNode atDepth:(int)depth {
    OutlineViewNode *walker;
    int counter=0;
    
    walker = self;
    for (counter=0; counter < depth; counter++) {
        walker = [walker childAtIndex:[walker childrenCount]-1];
    }
    
//    NSLog(@"Attaching %@ to %@",[aNode name],[walker value]);
    [walker addChild:aNode];
}

- (NSString*)stringRepresentation:(NSString*)name:(int)startingLevel
{
    int i;
    NSMutableString *finalText = [[NSMutableString alloc] init];
    OutlineViewNode * childNode;
    int childrenCount = [self childrenCount];
    
    for (i=0; i < childrenCount; i++) {
        childNode = [self childAtIndex:i];
		
		if ( [[childNode name] caseInsensitiveCompare:name] == NSOrderedSame )
		{
			[finalText appendFormat:@"%@   %@",[childNode name],[childNode value]];
		}
        
        if ([childNode isExpandable]) {
            [finalText appendString:[childNode stringRepresentation:name:startingLevel+1]];
        }
    }
    return [finalText autorelease];
}

- (void)addNodeWithName:(char *)name value:(char *)value atDepth:(int)depth {
    OutlineViewNode *aNode;
    
    _tempName = [[NSString alloc] initWithCString:name encoding:NSUTF8StringEncoding];
    _tempValue = [[NSString alloc] initWithCString:value encoding:NSUTF8StringEncoding];
    aNode  =  [[OutlineViewNode alloc] initWithName:_tempName value:_tempValue];
   
    [self addNode:aNode atDepth:depth];
    
    [aNode release];
    [_tempName release];
    [_tempValue release];
}

- (OutlineViewNode *)deepestChild {
    OutlineViewNode *retNode = self;
    while ([retNode isExpandable]) {
        retNode = [retNode childAtIndex:[retNode childrenCount]-1];
    }
    return retNode;
}

- (NSString *)stringRepresentation:(int)startingLevel {
    int i;
    NSMutableString *finalText = [[NSMutableString alloc] init];
    OutlineViewNode * childNode;
    int childrenCount = [self childrenCount];
   
    for (i=0; i < childrenCount; i++) {
        int counter;
        for (counter=0; counter <= startingLevel; counter++)
            [finalText appendString:@"    "];
        
        childNode = [self childAtIndex:i];
        [finalText appendFormat:@"%@   %@\n",[childNode name],[childNode value]];
        
        if ([childNode isExpandable]) {
            [finalText appendString:[childNode stringRepresentation:startingLevel+1]];
        }
    }
    return [finalText autorelease];
}

- (NSString *)stringRepresentationOfValues:(int)startingLevel {
    int i;
    NSMutableString *finalText = [[NSMutableString alloc] init];
    OutlineViewNode * childNode;
    int childrenCount = [self childrenCount];
    
    for (i=0; i < childrenCount; i++) {
        int counter;
        for (counter=0; counter < startingLevel; counter++)
            [finalText appendString:@"    "];
        
        childNode = [self childAtIndex:i];
        [finalText appendFormat:@"%@\n",[childNode value]];
        
        if ([childNode isExpandable]) {
            [finalText appendString:[childNode stringRepresentationOfValues:startingLevel+1]];
        }
    }
    return [finalText autorelease];
}

- (NSMutableDictionary *)dictionaryVersionOfMe
{
    NSMutableDictionary *returnDict = [NSMutableDictionary dictionary];
    [returnDict setObject:[self name] forKey:@"nodeName"];
    [returnDict setObject:[self value] forKey:@"nodeValue"];
    [returnDict setObject:[NSMutableArray array] forKey:@"children"];
    NSEnumerator *childObjectEnum = [[self children] objectEnumerator];
    OutlineViewNode *eachChild;
    while ((eachChild = [childObjectEnum nextObject]))
    {
        if ([eachChild childrenCount] > 0)
        {
            [[returnDict objectForKey:@"children"] addObject:[eachChild dictionaryVersionOfMe]];
        }
        else
        {
            if ([eachChild name] == nil || [[eachChild name] isEqualToString:@""])
            {
                [[returnDict objectForKey:@"children"] addObject:[eachChild value]];
            }
            else
            {
                [[returnDict objectForKey:@"children"] addObject:[NSMutableDictionary dictionaryWithObject:[eachChild value] forKey:[eachChild name]]];
            }
        }
    }
    return returnDict;
}

@end
