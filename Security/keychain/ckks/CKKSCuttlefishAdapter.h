#if OCTAGON

#import <SecurityFoundation/SecurityFoundation.h>

#import "keychain/ckks/CKKSCuttlefishAdapter.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperSpecificUser.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/CuttlefishXPCWrapper.h"

NS_ASSUME_NONNULL_BEGIN

@protocol CKKSCuttlefishAdapterProtocol
- (void)fetchCurrentItem:(TPSpecificUser* __nullable)activeAccount
                   items:(nonnull NSArray<CuttlefishCurrentItemSpecifier *> *)items
                   reply:(nonnull void (^)(NSArray<CuttlefishCurrentItem *> * _Nullable, NSArray<CKRecord *> * _Nullable, NSError * _Nullable))reply;

- (void)fetchPCSIdentityByKey:(TPSpecificUser* __nullable)activeAccount
                  pcsservices:(nonnull NSArray<CuttlefishPCSServiceIdentifier *> *)pcsservices
                        reply:(nonnull void (^)(NSArray<CuttlefishPCSIdentity *> * _Nullable, NSArray<CKRecord *> * _Nullable, NSError * _Nullable))reply;
@end


@interface CKKSCuttlefishAdapter : NSObject<CKKSCuttlefishAdapterProtocol>

@property CuttlefishXPCWrapper* cuttlefishXPCWrapper;

- (instancetype)initWithConnection:(id<NSXPCProxyCreating>)cuttlefishXPCConnection;

NS_ASSUME_NONNULL_END

@end

#else   // !OCTAGON
#import <Foundation/Foundation.h>
@interface CKKSCuttlefishAdapter : NSObject
{
    
}
@end
#endif  // OCTAGON
