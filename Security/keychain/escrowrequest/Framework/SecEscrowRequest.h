
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

extern NSString* const SecEscrowRequestHavePrecord;
extern NSString* const SecEscrowRequestPendingPasscode;
extern NSString* const SecEscrowRequestPendingCertificate;

@protocol SecEscrowRequestable <NSObject>
+ (id<SecEscrowRequestable> _Nullable)request:(NSError* _Nullable *)error;
- (BOOL)triggerEscrowUpdate:(NSString*)reason
                      error:(NSError**)error;
- (NSDictionary *_Nullable)fetchStatuses:(NSError **)error;

- (bool)pendingEscrowUpload:(NSError**)error;

@end

@interface SecEscrowRequest : NSObject <SecEscrowRequestable>
+ (SecEscrowRequest* _Nullable)request:(NSError* _Nullable *)error;
- (instancetype)initWithConnection:(NSXPCConnection*)connection;


- (BOOL)triggerEscrowUpdate:(NSString*)reason
                      error:(NSError**)error;

- (BOOL)cachePrerecord:(NSString*)uuid
   serializedPrerecord:(NSData*)prerecord
                 error:(NSError**)error;

- (NSData* _Nullable)fetchPrerecord:(NSString*)prerecordUUID
                              error:(NSError**)error;

// Returns a UUIDs of an escrow request which is ready to receive the device passcode.
// For a request to be in this state, sbd must have successfully cached a certificate in beginHSA2PasscodeRequest.
- (NSString* _Nullable)fetchRequestWaitingOnPasscode:(NSError**)error;

// Reset and delete all pending requests.
- (BOOL)resetAllRequests:(NSError**)error;

- (uint64_t)storePrerecordsInEscrow:(NSError**)error;
@end

NS_ASSUME_NONNULL_END
