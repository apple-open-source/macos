
#if OCTAGON

#import <Foundation/Foundation.h>
#import <CloudKit/CloudKit.h>

#import "keychain/ckks/CKKSExternalTLKClient.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSTLKShare.h"
#import "keychain/ckks/CKKSTLKShareRecord.h"

NS_ASSUME_NONNULL_BEGIN

@interface CKKSExternalKey (CKKSTranslation)
- (instancetype)initWithViewName:(NSString*)viewName
                             tlk:(CKKSKey*)tlk;

- (CKKSKey* _Nullable)makeCKKSKey:(CKRecordZoneID*)zoneID error:(NSError**)error;

// The CKKS cloudkit plugin ensures that there is a classA and classC 'key' in the key hierarchy. Fake it.
- (CKKSKey* _Nullable)makeFakeCKKSClassKey:(CKKSKeyClass*)keyclass zoneiD:(CKRecordZoneID*)zoneID error:(NSError**)error;
@end

@interface CKKSExternalTLKShare (CKKSTranslation)
- (instancetype)initWithViewName:(NSString*)viewName
                        tlkShare:(CKKSTLKShare*)share;

- (CKKSTLKShareRecord* _Nullable)makeTLKShareRecord:(CKRecordZoneID*)zoneID;
@end

NS_ASSUME_NONNULL_END

#endif
