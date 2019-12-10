//
//  KeychainSyncAccountNotification.m
//  Security
//

#import "KeychainSyncAccountNotification.h"
#import <Accounts/Accounts.h>
#import <Accounts/Accounts_Private.h>
#import <AppleAccount/ACAccount+AppleAccount.h>
#import <AccountsDaemon/ACDAccountStore.h>
#import <Security/SecureObjectSync/SOSCloudCircle.h>
#import <AuthKit/AuthKit.h>
#import <AuthKit/AuthKit_Private.h>
#if OCTAGON
#import <keychain/ot/OTControl.h>
#include <utilities/SecCFRelease.h>
#endif
#import "utilities/debugging.h"
#import "OT.h"

@implementation KeychainSyncAccountNotification

- (bool)accountIsPrimary:(ACAccount *)account
{
    return [account aa_isAccountClass:AAAccountClassPrimary];
}

// this is where we initialize SOS and OT for account sign-in
// in the future we may bring this logic over to KeychainDataclassOwner and delete KeychainSyncAccountNotification, but accounts people say that's a change that today would require coordination across multiple teams
// was asked to file this radar for accounts: <rdar://problem/40176124> Invoke DataclassOwner when enabling or signing into an account
- (void)account:(ACAccount *)account didChangeWithType:(ACAccountChangeType)changeType inStore:(ACDAccountStore *)store oldAccount:(ACAccount *)oldAccount {

    if((changeType == kACAccountChangeTypeAdded || changeType == kACAccountChangeTypeModified) &&
       [account.accountType.identifier isEqualToString: ACAccountTypeIdentifierAppleAccount] &&
       [self accountIsPrimary:account]) {

#if OCTAGON
        if(OctagonIsEnabled()){
            NSString* altDSID =  [account aa_altDSID];
            secnotice("octagon-account", "Received an primary Apple account modification (altDSID %@)", altDSID);

            __block NSError* error = nil;

            // Use asynchronous XPC here for speed and just hope it works
            OTControl* otcontrol = [OTControl controlObject:false error:&error];
            
            if (nil == otcontrol) {
                secerror("octagon-account: Failed to get OTControl: %@", error.localizedDescription);
            } else {
                [otcontrol signIn:altDSID container:nil context:OTDefaultContext reply:^(NSError * _Nullable signedInError) {
                    // take a retain on otcontrol so it won't invalidate the connection
                    (void)otcontrol;

                    if(signedInError) {
                        secerror("octagon-account: error signing in: %s", [[signedInError description] UTF8String]);
                    } else {
                        secnotice("octagon-account", "account now signed in for octagon operation");
                    }
                }];
            }
        }else{
            secerror("Octagon not enabled; not signing in");
        }
#endif
    }

    // If there is any change to any AuthKit account's security level, notify octagon

#if OCTAGON
    if([account.accountType.identifier isEqualToString: ACAccountTypeIdentifierIDMS]) {
        NSString* altDSID = [account aa_altDSID];;
        secnotice("octagon-authkit", "Received an IDMS account modification (altDSID: %@)", altDSID);

        AKAccountManager *manager = [AKAccountManager sharedInstance];

        AKAppleIDSecurityLevel oldSecurityLevel = [manager securityLevelForAccount:oldAccount];
        AKAppleIDSecurityLevel newSecurityLevel = [manager securityLevelForAccount:account];

        if(oldSecurityLevel != newSecurityLevel) {
            secnotice("octagon-authkit", "IDMS security level has now moved to %ld for altDSID %@", (unsigned long)newSecurityLevel, altDSID);

            __block NSError* error = nil;
            // Use an asynchronous otcontrol for Speed But Not Necessarily Correctness
            OTControl* otcontrol = [OTControl controlObject:false error:&error];
            if(!otcontrol || error) {
                secerror("octagon-authkit: Failed to get OTControl: %@", error);
            } else {
                 [otcontrol notifyIDMSTrustLevelChangeForContainer:nil context:OTDefaultContext reply:^(NSError * _Nullable idmsError) {
                     // take a retain on otcontrol so it won't invalidate the connection
                     (void)otcontrol;

                     if(idmsError) {
                         secerror("octagon-authkit: error with idms trust level change in: %s", [[idmsError description] UTF8String]);
                     } else {
                         secnotice("octagon-authkit", "informed octagon of IDMS trust level change");
                     }
                }];
            }

        } else {
            secnotice("octagon-authkit", "No change to IDMS security level (%lu) for altDSID %@", (unsigned long)newSecurityLevel, altDSID);
        }
    }
#endif

    if ((changeType == kACAccountChangeTypeDeleted) && [oldAccount.accountType.identifier isEqualToString:ACAccountTypeIdentifierAppleAccount]) {
        NSString* altDSID =  [oldAccount aa_altDSID];
        secnotice("octagon-account", "Received an Apple account deletion (altDSID %@)", altDSID);

        NSString *accountIdentifier = oldAccount.identifier;
        NSString *username = oldAccount.username;

        if(accountIdentifier != NULL && username !=NULL) {
            if ([self accountIsPrimary:oldAccount]) {
                CFErrorRef removalError = NULL;

                secinfo("accounts", "Performing SOS circle credential removal for account %@: %@", accountIdentifier, username);

                if (!SOSCCLoggedOutOfAccount(&removalError)) {
                    secerror("Account %@ could not leave the SOS circle: %@", accountIdentifier, removalError);
                }

#if OCTAGON
                if(OctagonIsEnabled()){
                    __block NSError* error = nil;

                    // Use an asynchronous control for Speed
                    OTControl* otcontrol = [OTControl controlObject:false error:&error];

                    if (nil == otcontrol) {
                        secerror("octagon-account: Failed to get OTControl: %@", error.localizedDescription);
                    } else {
                        [otcontrol signOut:nil context:OTDefaultContext reply:^(NSError * _Nullable signedInError) {
                            // take a retain on otcontrol so it won't invalidate the connection
                            (void)otcontrol;

                            if(signedInError) {
                                secerror("octagon-account: error signing out: %s", [[signedInError description] UTF8String]);
                            } else {
                                secnotice("octagon-account", "signed out of octagon trust");
                            }
                        }];
                    }
                } else {
                    secerror("Octagon not enabled; not signing out");
                }
#endif
            }
        }
    }
}

@end
