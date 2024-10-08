#if OCTAGON

#import <SecurityFoundation/SecurityFoundation.h>

#import "keychain/ckks/CKKSCuttlefishAdapter.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperSpecificUser.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/CuttlefishXPCWrapper.h"

NS_ASSUME_NONNULL_BEGIN

@implementation CKKSCuttlefishAdapter

- (instancetype)initWithConnection:(id<NSXPCProxyCreating>)cuttlefishXPCConnection {
    if((self = [super init])) {
        _cuttlefishXPCWrapper = [[CuttlefishXPCWrapper alloc] initWithCuttlefishXPCConnection:cuttlefishXPCConnection];
    }
    return self;
}

- (void)fetchCurrentItem:(TPSpecificUser* __nullable)activeAccount
                   items:(nonnull NSArray<CuttlefishCurrentItemSpecifier *> *)items
                   reply:(nonnull void (^)(NSArray<CuttlefishCurrentItem *> * _Nullable, NSArray<CKRecord *> * _Nullable, NSError * _Nullable))reply
{
    
    [self.cuttlefishXPCWrapper fetchCurrentItemWithSpecificUser:activeAccount
                                                          items:items
                                                          reply:^(NSArray<CuttlefishCurrentItem *> * _Nullable currentItems, NSArray<CKRecord *> * _Nullable syncKeys, NSError * _Nullable error) {
        if (error) {
            ckkserror_global("ckks-cuttlefish", "error fetching current item: %@", error);
            reply(nil, nil, error);
        } else {
            ckksnotice_global("ckks-cuttlefish", "fetched current items for CIPs: %@", items);
            reply(currentItems, syncKeys, nil);
        }
    }];
}

- (void)fetchPCSIdentityByKey:(TPSpecificUser* __nullable)activeAccount
                  pcsservices:(nonnull NSArray<CuttlefishPCSServiceIdentifier *> *)pcsservices
                        reply:(nonnull void (^)(NSArray<CuttlefishPCSIdentity *> * _Nullable, NSArray<CKRecord *> * _Nullable, NSError * _Nullable))reply
{
    [self.cuttlefishXPCWrapper fetchPCSIdentityByPublicKeyWithSpecificUser:activeAccount
                                                               pcsservices:pcsservices
                                                                     reply:^(NSArray<CuttlefishPCSIdentity *> * _Nullable pcsIdentities, NSArray<CKRecord *> * _Nullable syncKeys, NSError * _Nullable error) {
        if (error) {
            ckkserror_global("ckks-cuttlefish", "error fetching pcs identity: %@", error);
            reply(nil, nil, error);
        } else {
            ckksnotice_global("ckks-cuttlefish", "fetched pcs identities for the following services: %@", pcsservices);
            reply(pcsIdentities, syncKeys, nil);
        }
    }];
}

NS_ASSUME_NONNULL_END

@end

#endif // OCTAGON
