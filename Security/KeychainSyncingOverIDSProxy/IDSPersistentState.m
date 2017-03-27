/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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

//
//  IDSPersistentState.m
//

#import <Security/Security.h>
#import <Foundation/NSPropertyList.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSPropertyList.h>
#import <Foundation/NSData.h>
#import <Foundation/NSDictionary.h>
#import <utilities/debugging.h>
#import <utilities/SecFileLocations.h>

#import "IDSPersistentState.h"

#if ! __has_feature(objc_arc)
#error This file must be compiled with ARC. Either turn on ARC for the project or use -fobjc-arc flag
#endif

static CFStringRef kRegistrationFileName = CFSTR("com.apple.security.keychainsyncingoveridsproxy.unhandledMessages.plist");

@implementation KeychainSyncingOverIDSProxyPersistentState

+ (BOOL)write:(NSURL *)path data:(id)plist error:(NSError **)error
{
    if (![NSPropertyListSerialization propertyList: plist isValidForFormat: NSPropertyListXMLFormat_v1_0])
    {
        secerror("can't save PersistentState as XML");
        return false;
    }
    
    NSData *data = [NSPropertyListSerialization dataWithPropertyList: plist
                                                              format: NSPropertyListXMLFormat_v1_0 options: 0 error: error];
    if (data == nil)
    {
        secerror("error serializing PersistentState to xml: %@", *error);
        return false;
    }
    
    BOOL writeStatus = [data writeToURL: path options: NSDataWritingAtomic error: error];
    if (!writeStatus)
        secerror("error writing PersistentState to file: %@", *error);
    
    return writeStatus;
}

+ (id)read: (NSURL *)path error:(NSError **)error
{
    NSData *data = [NSData dataWithContentsOfURL: path options: 0 error: error];
    if (data == nil)
    {
        secdebug("unhandledmessages", "error reading PersistentState from %@: %@", path, *error);
        return nil;
    }
    
    // Now the deserializing:
    
    NSPropertyListFormat format;
    id plist = [NSPropertyListSerialization propertyListWithData: data
                                                         options: NSPropertyListMutableContainersAndLeaves format: &format error: error];
    
    if (plist == nil)
        secerror("could not deserialize PersistentState from %@: %@", path, *error);
    
    return plist;
}

+ (NSURL *)registrationFileURL
{
    return (NSURL *)CFBridgingRelease(SecCopyURLForFileInPreferencesDirectory(kRegistrationFileName));
}

+ (NSString *)dictionaryDescription: (NSDictionary *)state
{
    NSMutableArray *elements = [NSMutableArray array];
    [state enumerateKeysAndObjectsUsingBlock: ^(NSString *key, id obj, BOOL *stop) {
        [elements addObject: [key stringByAppendingString: @":"]];
        if ([obj isKindOfClass:[NSArray class]]) {
            [elements addObject: [(NSArray *)obj componentsJoinedByString: @" "]];
        } else {
            [elements addObject: [NSString stringWithFormat:@"%@", obj]];
        }
    }];
    return [elements componentsJoinedByString: @" "];
}

+ (NSMutableDictionary *)idsState
{
    NSError *error = NULL;
    id stateDictionary = [KeychainSyncingOverIDSProxyPersistentState read:[[self class] registrationFileURL] error:&error];
    secdebug("keyregister", "Read registeredKeys: <%@>", [self dictionaryDescription: stateDictionary]);
    // Ignore older states with an NSArray
    if (![stateDictionary isKindOfClass:[NSDictionary class]])
        return NULL;
    return [NSMutableDictionary dictionaryWithDictionary:stateDictionary];
}

+ (void)setUnhandledMessages: (NSDictionary *)unhandledMessages
{
    NSError *error = NULL;
    secdebug("IDS unhandled message", "Write unhandled Messages and monitor state: <%@>", [self dictionaryDescription: unhandledMessages]);
    [KeychainSyncingOverIDSProxyPersistentState write:[[self class] registrationFileURL] data:unhandledMessages error:&error];
}

@end
