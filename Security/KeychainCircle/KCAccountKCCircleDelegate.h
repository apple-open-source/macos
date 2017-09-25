//
//  KCAccountKCCircleDelegate.h
//  Security
//

#import <KeychainCircle/KCJoiningSession.h>

@interface KCJoiningRequestAccountCircleDelegate : NSObject < KCJoiningRequestCircleDelegate>
/*!
 Get this devices peer info (As Application)

 @result
 SOSPeerInfoRef object or NULL if we had an error.
 */
- (SOSPeerInfoRef) copyPeerInfoError: (NSError**) error;

/*!
 Handle recipt of confirmed circleJoinData over the channel

 @parameter circleJoinData
 Data the acceptor made to allow us to join the circle.
 @parameter version
 Piggybacking version, let's secd know it should expect more data
 */
- (bool) processCircleJoinData: (NSData*) circleJoinData version:(PiggyBackProtocolVersion)version error: (NSError**)error;

+ (instancetype) delegate;

@end

@interface KCJoiningAcceptAccountCircleDelegate : NSObject < KCJoiningAcceptCircleDelegate>
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
                        error: (NSError**) error;
-(NSData*) circleGetInitialSyncViews: (NSError**) error;
    
+ (instancetype) delegate;

@end
