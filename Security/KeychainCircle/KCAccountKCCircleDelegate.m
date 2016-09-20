//
//  KCAccountKCCircleDelegate.m
//  Security
//
//  Created by Mitch Adler on 4/11/16.
//
//

#import <KeychainCircle/KCAccountKCCircleDelegate.h>

#include <Security/SecureObjectSync/SOSCloudCircle.h>


@implementation KCJoiningRequestAccountCircleDelegate
/*!
 Get this devices peer info (As Application)

 @result
 SOSPeerInfoRef object or NULL if we had an error.
 */
- (SOSPeerInfoRef) copyPeerInfoError: (NSError**) error {
    CFErrorRef failure = NULL;
    SOSPeerInfoRef result = SOSCCCopyApplication(error ? &failure : NULL);
    if (failure != NULL && error != nil) {
        *error = (__bridge_transfer NSError*) failure;
    }
    return result;
}

/*!
 Handle recipt of confirmed circleJoinData over the channel

 @parameter circleJoinData
 Data the acceptor made to allow us to join the circle.

 */
- (bool) processCircleJoinData: (NSData*) circleJoinData error: (NSError**)error {
    CFErrorRef failure = NULL;
    bool result = SOSCCJoinWithCircleJoiningBlob((__bridge CFDataRef) circleJoinData, &failure);
    if (failure != NULL && error != nil) {
        *error = (__bridge_transfer NSError*) failure;
    }
    return result;
}

+ (instancetype) delegate {
    return [[KCJoiningRequestAccountCircleDelegate alloc] init];
}

@end

@implementation KCJoiningAcceptAccountCircleDelegate
/*!
 Handle the request's peer info and get the blob they can use to get in circle
 @param peer
 SOSPeerInfo sent from requestor to apply to the circle
 @param error
 Error resulting in looking at peer and trying to produce circle join data
 @result
 Data containing blob the requestor can use to get in circle
 */
- (NSData*) circleJoinDataFor: (SOSPeerInfoRef) peer
                        error: (NSError**) error {
    CFErrorRef failure = NULL;
    CFDataRef result = SOSCCCopyCircleJoiningBlob(peer, &failure);
    if (failure != NULL && error != nil) {
        *error = (__bridge_transfer NSError*) failure;
    }
    return (__bridge_transfer NSData*) result;
}

+ (instancetype) delegate {
    return [[KCJoiningAcceptAccountCircleDelegate alloc] init];
}

@end

