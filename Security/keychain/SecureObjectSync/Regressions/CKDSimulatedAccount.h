//
//  CKDSimulatedAccount.m
//  Security
//

#import "CKDAccount.h"

#import <Foundation/Foundation.h>

@interface CKDSimulatedAccount : NSObject<CKDAccount>

@property (readwrite) NSMutableSet<NSString*>* keysToNotHandle;
@property (readwrite) NSMutableSet<NSString*>* peersToNotSyncWith;
@property (readwrite) NSMutableSet<NSString*>* backupPeersToNotSyncWith;
@property (readwrite) NSError* peerRegistrationFailureReason;

+ (instancetype) account;
- (instancetype) init;

- (NSSet*) keysChanged: (NSDictionary<NSString*, NSObject*>*) keyValues error: (NSError**) error;
- (bool) ensurePeerRegistration: (NSError**) error;

- (NSSet<NSString*>*) syncWithPeers: (NSSet<NSString*>*) peerIDs backups: (NSSet<NSString*>*) backupPeerIDs error: (NSError**) error;
- (bool) syncWithAllPeers: (NSError**) error;

- (NSDictionary<NSString*, NSObject*>*) extractKeyChanges;
- (NSSet<NSString*>*) extractPeerChanges;
- (NSSet<NSString*>*) extractBackupPeerChanges;

- (BOOL) extractRegistrationEnsured;

@end
