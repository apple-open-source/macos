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

#import <Foundation/Foundation.h>


@interface BusProbeClass : NSObject {
    UInt8   _classNum;
    UInt8   _subclassNum;
    UInt8   _protocolNum;
    NSString *  _className;
    NSString *  _subclassName;
    NSString *  _protocolName;
}

+ (BusProbeClass *)withClass:(UInt8)classNum subclass:(UInt8)subclassNum protocol:(UInt8)protocolNum;
- (UInt8)classNum;
- (void)setClassNum:(UInt8)classNum;
- (UInt8)subclassNum;
- (void)setSubclassNum:(UInt8)subclassNum;
- (UInt8)protocolNum;
- (void)setProtocolNum:(UInt8)protocolNum;
- (NSString *)className;
- (void)setClassName:(NSString *)deviceClass;
- (NSString *)subclassName;
- (void)setSubclassName:(NSString *)deviceSubclass;
- (NSString *)protocolName;
- (void)setProtocolName:(NSString *)deviceProtocol;
- (NSString *)classDescription;
- (NSString *)subclassDescription;
- (NSString *)protocolDescription;

-(NSMutableDictionary *)dictionaryVersionOfMe;

@end
