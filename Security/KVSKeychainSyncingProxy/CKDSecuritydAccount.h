//
//  CKDSecuritydAccount.h
//  Security
//
//


#include "CKDAccount.h"
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>

@interface CKDSecuritydAccount : NSObject<CKDAccount>

+ (instancetype) securitydAccount;

- (NSSet*) keysChanged: (NSDictionary<NSString*, NSObject*>*) keyValues error: (NSError**) error;
- (bool) ensurePeerRegistration: (NSError**) error;

- (NSSet<NSString*>*) syncWithPeers: (NSSet<NSString*>*) peerIDs backups: (NSSet<NSString*>*) backupPeerIDs error: (NSError**) error;
- (SyncWithAllPeersReason) syncWithAllPeers: (NSError**) error;

@end
