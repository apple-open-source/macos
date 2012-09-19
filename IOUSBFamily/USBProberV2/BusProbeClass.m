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

#import "BusProbeClass.h"


@implementation BusProbeClass

- init {
    if (self = [super init]) {
        _classNum = -1;
        _subclassNum = -1;
        _className = [@"unknown" retain];
        _subclassName = [@"unknown" retain];
    }
    return self;
}

- (void)dealloc {
    [_className release];
    [_subclassName release];
    [super dealloc];
}

+ (BusProbeClass *)withClass:(UInt8)classNum subclass:(UInt8)subclassNum protocol:(UInt8)protocolNum{
    BusProbeClass *thisClass = [[BusProbeClass alloc] init];
    [thisClass setClassNum:classNum];
    [thisClass setSubclassNum:subclassNum];
    [thisClass setProtocolNum:protocolNum];
    return [thisClass autorelease];
}

- (UInt8)classNum {
    return _classNum;
}

- (void)setClassNum:(UInt8)classNum {
    _classNum = classNum;
}

- (UInt8)subclassNum {
    return _subclassNum;
}

- (void)setSubclassNum:(UInt8)subclassNum {
    _subclassNum = subclassNum;
}

- (UInt8)protocolNum {
    return _protocolNum;
}

- (void)setProtocolNum:(UInt8)protocolNum {
    _protocolNum = protocolNum;
}

- (NSString *)className 
{
    if (_className == nil)
    {
        return @"";
    }
    return _className;
}
- (void)setClassName:(NSString *)deviceClass {
    [_className release];
    _className = [deviceClass retain];
}

- (NSString *)subclassName 
{
    if (_subclassName == nil)
    {
        return @"";
    }
    return _subclassName;
}

- (void)setSubclassName:(NSString *)deviceSubclass {
    [_subclassName release];
    _subclassName = [deviceSubclass retain];
}

- (NSString *)protocolName 
{
    if (_protocolName == nil)
    {
        return @"";
    }
    return _protocolName;
}

- (void)setProtocolName:(NSString *)deviceProtocol {
    [_protocolName release];
    _protocolName = [deviceProtocol retain];
}

- (NSString *)classDescription {
    if ([_className isEqualToString:@""])
        return [NSString stringWithFormat:@"%d",_classNum];
    else
        return [NSString stringWithFormat:@"%d   (%@)",_classNum,_className];
}

- (NSString *)subclassDescription {
    if ([_subclassName isEqualToString:@""])
        return [NSString stringWithFormat:@"%d",_subclassNum];
    else
        return [NSString stringWithFormat:@"%d   (%@)",_subclassNum,_subclassName];
}

- (NSString *)protocolDescription {
    if ([_protocolName isEqualToString:@""])
        return [NSString stringWithFormat:@"%d",_protocolNum];
    else
        return [NSString stringWithFormat:@"%d   (%@)",_protocolNum,_protocolName];
}

-(NSMutableDictionary *)dictionaryVersionOfMe
{
    NSMutableDictionary *returnDict = [NSMutableDictionary dictionary];
    [returnDict setObject:[NSNumber numberWithInt:_classNum] forKey:@"classNum"];
    [returnDict setObject:[NSNumber numberWithInt:_subclassNum] forKey:@"subclassNum"];
    [returnDict setObject:[NSNumber numberWithInt:_protocolNum] forKey:@"protocolNum"];
    [returnDict setObject:[self className] forKey:@"className"];
    [returnDict setObject:[self subclassName] forKey:@"subclassName"];
    [returnDict setObject:[self protocolName] forKey:@"protocolName"];
    return returnDict;
}

@end
