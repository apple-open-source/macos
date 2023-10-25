
#if OCTAGON

#import <Foundation/Foundation.h>

#import "keychain/ot/OTPersonaAdapter.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperSpecificUser.h"

#import <AppleAccount/AppleAccount.h>

#import <AppleAccount/AppleAccount_Private.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#import <AppleAccount/ACAccount+AppleAccount.h>
#pragma clang diagnostic pop

NS_ASSUME_NONNULL_BEGIN

@protocol OTAccountsAdapter
- (TPSpecificUser* _Nullable)findAccountForCurrentThread:(id<OTPersonaAdapter>)personaAdapter
                                         optionalAltDSID:(NSString* _Nullable)altDSID
                                   cloudkitContainerName:(NSString*)cloudkitContainerName
                                        octagonContextID:(NSString*)octagonContextID
                                                   error:(NSError**)error;

- (NSArray<TPSpecificUser*>* _Nullable)inflateAllTPSpecificUsers:(NSString*)cloudkitContainerName
                                                octagonContextID:(NSString*)octagonContextID;

//test only
- (void)setAccountStore:(ACAccountStore*)store;

@end

@interface OTAccountsActualAdapter : NSObject <OTAccountsAdapter>
@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON
