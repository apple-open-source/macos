//
//  SOSAuthKitHelpers.h
//  Security
//

#ifndef SOSAuthKitHelpers_h
#define SOSAuthKitHelpers_h

#include <Security/SecureObjectSync/SOSAccount.h>

NS_ASSUME_NONNULL_BEGIN

@interface SOSAuthKitHelpers : NSObject
+ (NSString * _Nullable)machineID;
+ (bool) updateMIDInPeerInfo: (SOSAccount *) account;
+ (bool) peerinfoHasMID: (SOSAccount *) account;

// activeMIDs might block on network
+ (void)activeMIDs:(void(^_Nonnull)(NSSet * _Nullable activeMIDs, NSError * _Nullable error))complete;

@end

NS_ASSUME_NONNULL_END

#endif /* SOSAuthKitHelpers_h */
