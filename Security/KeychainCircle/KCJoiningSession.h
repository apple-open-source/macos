//
//  KCJoiningSession.h
//  KeychainCircle
//
//

#import <KeychainCircle/KCSRPContext.h>
#import <KeychainCircle/KCAESGCMDuplexSession.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>

NS_ASSUME_NONNULL_BEGIN

@protocol KCJoiningRequestCircleDelegate
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

 */
- (bool) processCircleJoinData: (NSData*) circleJoinData error: (NSError**)error;

@end

@protocol KCJoiningRequestSecretDelegate
/*!
 Get the shared secret for this session.
 Not called during creation or initialMessage: to allow the initial message to be sent before
 we know the secret.
 Called during message processing.

 @result
 String containing shared secret for session
 */
- (NSString*) secret;

/*!
 Handle verification failure
 @result 
 NULL if we should give up. Secret to use on retry, if not.
 */
- (NSString*) verificationFailed: (bool) codeChanged;

/*!
 Handle recipt of confirmed accountCode over the channel

 @parameter accountCode
 Data the acceptor made to allow us to join the circle.
 */
- (bool) processAccountCode: (NSString*) accountCode error: (NSError**)error;

@end

@interface KCJoiningRequestSecretSession : NSObject
@property (nullable, readonly) KCAESGCMDuplexSession* session;

- (bool) isDone;

- (nullable NSData*) initialMessage: (NSError**) error;
- (nullable NSData*) processMessage: (NSData*) incomingMessage error: (NSError**) error;

+ (nullable instancetype)sessionWithSecretDelegate: (NSObject<KCJoiningRequestSecretDelegate>*) secretDelegate
                                              dsid: (uint64_t)dsid
                                             error: (NSError**) error;

- (nullable instancetype)initWithSecretDelegate: (NSObject<KCJoiningRequestSecretDelegate>*) secretDelegate
                                           dsid: (uint64_t)dsid
                                          error: (NSError**)error;

- (nullable instancetype)initWithSecretDelegate: (NSObject<KCJoiningRequestSecretDelegate>*) secretDelegate
                                           dsid: (uint64_t)dsid
                                            rng: (struct ccrng_state *)rng
                                          error: (NSError**)error NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end


@interface KCJoiningRequestCircleSession : NSObject

- (bool) isDone;

- (nullable NSData*) initialMessage: (NSError**) error;
- (nullable NSData*) processMessage: (NSData*) incomingMessage error: (NSError**) error;

+ (instancetype) sessionWithCircleDelegate: (NSObject<KCJoiningRequestCircleDelegate>*) circleDelegate
                                   session: (KCAESGCMDuplexSession*) session
                                     error: (NSError**) error;

- (instancetype) initWithCircleDelegate: (NSObject<KCJoiningRequestCircleDelegate>*) circleDelegate
                                session: (KCAESGCMDuplexSession*) session
                                  error: (NSError**) error NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
@end


@protocol KCJoiningAcceptCircleDelegate
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
@end

typedef enum {
    kKCRetryError = 0,
    kKCRetryWithSameChallenge,
    kKCRetryWithNewChallenge
} KCRetryOrNot;

@protocol KCJoiningAcceptSecretDelegate
/*!
    Get the shared secret for this session
    @result
        String containing shared secret for session
*/
- (NSString*) secret;
/*!
    Get the code the other device can use to access the account
    @result
        String containing code to access the account
*/
- (NSString*) accountCode;

/*!
 Handle verification failure
 @result
 NULL if we should permit retry with the same secret. New secret if we've changed it.
 */
- (KCRetryOrNot) verificationFailed: (NSError**) error;

@end


@interface KCJoiningAcceptSession : NSObject
/*!
    create an appropriate joining session given the initial message.

    @parameter message
        initial message received from the requestor
    @parameter delegate
        delegate which will provide data and processing (see KCJoiningAcceptSecretDelegate protocol
    @parameter error
        failures to find a session for the initial message
    @result
        KCJoiningAcceptSession that can handle the data from the peer

 */
+ (nullable instancetype) sessionWithInitialMessage: (NSData*) message
                                     secretDelegate: (NSObject<KCJoiningAcceptSecretDelegate>*) delegate
                                     circleDelegate: (NSObject<KCJoiningAcceptCircleDelegate>*) delegate
                                               dsid: (uint64_t) dsid
                                              error: (NSError**) error;


- (nullable instancetype)initWithSecretDelegate: (NSObject<KCJoiningAcceptSecretDelegate>*) delegate
                                 circleDelegate: (NSObject<KCJoiningAcceptCircleDelegate>*) delegate
                                           dsid: (uint64_t) dsid
                                            rng: (struct ccrng_state *)rng
                                          error: (NSError**) error NS_DESIGNATED_INITIALIZER;

/*!
 create an appropriate joining session given the initial message.

 @parameter incomingMessage
    message received from the requestor
 @parameter error
    failures parse the message
 @result
    Data to send to the requestor, or NULL if we had an error.
    Calling this function when we are done results in an error return.
 */
- (nullable NSData*) processMessage: (NSData*) incomingMessage error: (NSError**) error;

- (bool) isDone;

- (id)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
