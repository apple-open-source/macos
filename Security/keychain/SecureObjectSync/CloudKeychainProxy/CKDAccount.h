//
//  CKDAccount.h
//  Security
//
//

#import <Foundation/Foundation.h>

@protocol CKDAccount

- (NSSet*) keysChanged: (NSDictionary<NSString*, NSObject*>*) keyValues error: (NSError**) error;
- (bool) ensurePeerRegistration: (NSError**) error;

- (NSSet<NSString*>*) syncWithPeers: (NSSet<NSString*>*) peerIDs backups: (NSSet<NSString*>*) backupPeerIDs error: (NSError**) error;
- (bool) syncWithAllPeers: (NSError**) error;

@end
