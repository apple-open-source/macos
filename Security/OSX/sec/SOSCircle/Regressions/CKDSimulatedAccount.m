//
//  CKDSimulatedAccount+CKDSimulatedAccount.m
//  Security
//

#import "CKDSimulatedAccount.h"

#import <Foundation/Foundation.h>

@interface CKDSimulatedAccount ()
@property (readwrite) NSMutableDictionary<NSString*, NSObject*>* keyChanges;
@property (readwrite) NSMutableSet<NSString*>* peerChanges;
@property (readwrite) NSMutableSet<NSString*>* backupPeerChanges;
@property (readwrite) BOOL peerRegistrationEnsured;
@end

@implementation CKDSimulatedAccount

+ (instancetype) account {
    return [[CKDSimulatedAccount alloc] init];
}
- (instancetype) init {
    self = [super init];
    if (self) {
        self.keysToNotHandle = [NSMutableSet<NSString*> set];
        self.keyChanges = [NSMutableDictionary<NSString*, NSObject*> dictionary];

        self.peerChanges = [NSMutableSet<NSString*> set];
        self.peersToNotSyncWith = [NSMutableSet<NSString*> set];

        self.backupPeerChanges = [NSMutableSet<NSString*> set];
        self.backupPeersToNotSyncWith = [NSMutableSet<NSString*> set];

        self.peerRegistrationEnsured = NO;
    }
    return self;
}

- (NSSet*) keysChanged: (NSDictionary<NSString*, NSObject*>*) keyValues
                 error: (NSError**) error {

    NSMutableSet<NSString*>* result = [NSMutableSet<NSString*> set];

    [keyValues enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull key, NSObject * _Nonnull obj, BOOL * _Nonnull stop) {
        if (![self.keysToNotHandle containsObject:key]) {
            [self.keyChanges setObject:obj forKey:key];
            [result addObject:key];
        }
    }];

    return result;
}

- (bool) ensurePeerRegistration: (NSError**) error {
    if (self.peerRegistrationFailureReason == nil) {
        self.peerRegistrationEnsured = YES;
        return true;
    } else {
        if (error) {
            *error = self.peerRegistrationFailureReason;
        }
        return false;
    }
}

- (NSSet<NSString*>*) syncWithPeers: (NSSet<NSString*>*) peerIDs
                            backups: (NSSet<NSString*>*) backupPeerIDs
                              error: (NSError**) error {
    NSMutableSet<NSString*>* peerIDsToTake = [peerIDs mutableCopy];
    [peerIDsToTake minusSet:self.peersToNotSyncWith];
    [self.peerChanges unionSet: peerIDsToTake];

    NSMutableSet<NSString*>* backupPeerIDsToTake = [NSMutableSet<NSString*> setWithSet: backupPeerIDs];
    [backupPeerIDsToTake minusSet:self.backupPeersToNotSyncWith];
    [self.backupPeerChanges unionSet: backupPeerIDsToTake];

    // Calculate what we took.
    [peerIDsToTake unionSet:backupPeerIDsToTake];
    return peerIDsToTake;
}

- (bool) syncWithAllPeers: (NSError**) error {
    return true;
}

- (NSDictionary<NSString*, NSObject*>*) extractKeyChanges {
    NSDictionary<NSString*, NSObject*>* result = self.keyChanges;
    self.keyChanges = [NSMutableDictionary<NSString*, NSObject*> dictionary];
    return result;
}

- (NSSet<NSString*>*) extractPeerChanges {
    NSSet<NSString*>* result = self.peerChanges;
    self.peerChanges = [NSMutableSet<NSString*> set];
    return result;
}
- (NSSet<NSString*>*) extractBackupPeerChanges {
    NSSet<NSString*>* result = self.backupPeerChanges;
    self.backupPeerChanges = [NSMutableSet<NSString*> set];
    return result;
}

- (BOOL) extractRegistrationEnsured {
    BOOL result = self.peerRegistrationEnsured;
    self.peerRegistrationEnsured = NO;
    return result;
}

@end
