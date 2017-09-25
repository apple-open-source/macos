/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#import "TPCircle.h"
#import "TPHash.h"

@interface TPCircle ()
@property (nonatomic, strong) NSString* circleID;
@property (nonatomic, strong) NSSet<NSString*>* includedPeerIDs;
@property (nonatomic, strong) NSSet<NSString*>* excludedPeerIDs;
@end

@implementation TPCircle

+ (instancetype)circleWithIncludedPeerIDs:(NSArray<NSString*> *)includedPeerIDs
                          excludedPeerIDs:(NSArray<NSString*> *)excludedPeerIDs
{
    return [[TPCircle alloc] initWithIncludedPeerIDs:[NSSet setWithArray:includedPeerIDs]
                                     excludedPeerIDs:[NSSet setWithArray:excludedPeerIDs]];
}

+ (instancetype)circleWithID:(NSString *)circleID
             includedPeerIDs:(NSArray<NSString*> *)includedPeerIDs
             excludedPeerIDs:(NSArray<NSString*> *)excludedPeerIDs
{
    TPCircle *circle = [TPCircle circleWithIncludedPeerIDs:includedPeerIDs
                                           excludedPeerIDs:excludedPeerIDs];
    if ([circleID isEqualToString:circle.circleID]) {
        return circle;
    } else {
        return nil;
    }
}

- (instancetype)initWithIncludedPeerIDs:(NSSet<NSString*> *)includedPeerIDs
                        excludedPeerIDs:(NSSet<NSString*> *)excludedPeerIDs
{
    self = [super init];
    if (self) {
        // Copy the sets passed in, so that nobody can mutate them later.
        _includedPeerIDs = [includedPeerIDs copy];
        _excludedPeerIDs = [excludedPeerIDs copy];
        
        NSArray<NSString*>* sortedInc = [[includedPeerIDs allObjects] sortedArrayUsingSelector:@selector(compare:)];
        NSArray<NSString*>* sortedExc = [[excludedPeerIDs allObjects] sortedArrayUsingSelector:@selector(compare:)];

        TPHashBuilder* hasher = [[TPHashBuilder alloc] initWithAlgo:kTPHashAlgoSHA256];
        {
            const char* inc = "include: ";
            [hasher updateWithBytes:inc len:strlen(inc)];
            for (NSString* peerID in sortedInc) {
                [hasher updateWithData:[peerID dataUsingEncoding:NSUTF8StringEncoding]];
            }
        }
        {
            const char* exc = "exclude: ";
            [hasher updateWithBytes:exc len:strlen(exc)];
            for (NSString* peerID in sortedExc) {
                [hasher updateWithData:[peerID dataUsingEncoding:NSUTF8StringEncoding]];
            }
        }
        _circleID = [hasher finalHash];
    }
    return self;
}

- (BOOL)isEqualToCircle:(TPCircle *)other
{
    return [self.includedPeerIDs isEqualToSet:other.includedPeerIDs]
        && [self.excludedPeerIDs isEqualToSet:other.excludedPeerIDs];
}

#pragma mark - NSObject

- (BOOL)isEqual:(id)object
{
    if (self == object) {
        return YES;
    }
    if (![object isKindOfClass:[TPCircle class]]) {
        return NO;
    }
    return [self isEqualToCircle:object];
}

- (NSUInteger)hash
{
    return [self.includedPeerIDs hash] ^ ([self.excludedPeerIDs hash] << 1);
}

static NSString *setDescription(NSSet *set)
{
    return [[[set allObjects] sortedArrayUsingSelector:@selector(compare:)] componentsJoinedByString:@" "];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"{ in: [%@] ex: [%@] }",
            setDescription(self.includedPeerIDs),
            setDescription(self.excludedPeerIDs)];
}

@end
