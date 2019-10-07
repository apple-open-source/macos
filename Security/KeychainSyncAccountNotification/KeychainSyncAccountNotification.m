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
            __block NSError* error = nil;

            NSString* altDSID =  [account aa_altDSID];

            OTControl* otcontrol = [OTControl controlObject:&error];
            
            if (nil == otcontrol) {
                secerror("octagon: Failed to get OTControl: %@", error.localizedDescription);
            } else {
                dispatch_semaphore_t sema = dispatch_semaphore_create(0);
                
                [otcontrol signIn:altDSID container:nil context:OTDefaultContext reply:^(NSError * _Nullable signedInError) {
                    if(signedInError) {
                        secerror("octagon: error signing in: %s", [[signedInError description] UTF8String]);
                    } else {
                        secnotice("octagon", "account now signed in for octagon operation");
                    }
                    dispatch_semaphore_signal(sema);
                    
                }];
                if (0 != dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60 * 5))) {
                    secerror("octagon: Timed out signing in");
                }
            }
        }else{
            secerror("Octagon not enabled; not signing in");
        }
#endif
    }

    // If there is any change to any AuthKit account's security level, notify octagon

#if OCTAGON
    if([account.accountType.identifier isEqualToString: ACAccountTypeIdentifierIDMS]) {
        secnotice("octagon-authkit", "Received an IDMS account modification");

        AKAccountManager *manager = [AKAccountManager sharedInstance];

        AKAppleIDSecurityLevel oldSecurityLevel = [manager securityLevelForAccount:oldAccount];
        AKAppleIDSecurityLevel newSecurityLevel = [manager securityLevelForAccount:account];

        if(oldSecurityLevel != newSecurityLevel) {
            NSString* identifier = account.identifier;
            secnotice("octagon-authkit", "IDMS security level has now moved to %ld for %@", (unsigned long)newSecurityLevel, identifier);

            __block NSError* error = nil;
            OTControl* otcontrol = [OTControl controlObject:&error];
            if(!otcontrol || error) {
                secerror("octagon-authkit: Failed to get OTControl: %@", error);
            } else {
                dispatch_semaphore_t sema = dispatch_semaphore_create(0);

                 [otcontrol notifyIDMSTrustLevelChangeForContainer:nil context:OTDefaultContext reply:^(NSError * _Nullable idmsError) {
                    if(idmsError) {
                        secerror("octagon-authkit: error with idms trust level change in: %s", [[idmsError description] UTF8String]);
                    } else {
                        secnotice("octagon-authkit", "informed octagon of IDMS trust level change");
                    }
                    dispatch_semaphore_signal(sema);
                }];

                if (0 != dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 5))) {
                    secerror("octagon-authkit: Timed out altering IDMS change in");
                }
            }

        } else {
            secnotice("octagon-authkit", "No change to IDMS security level");
        }
    }
#endif

    if ((changeType == kACAccountChangeTypeDeleted) && [oldAccount.accountType.identifier isEqualToString:ACAccountTypeIdentifierAppleAccount]) {

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

                    OTControl* otcontrol = [OTControl controlObject:&error];

                    if (nil == otcontrol) {
                        secerror("octagon: Failed to get OTControl: %@", error.localizedDescription);
                    } else {
                        dispatch_semaphore_t sema = dispatch_semaphore_create(0);

                        [otcontrol signOut:nil context:OTDefaultContext reply:^(NSError * _Nullable signedInError) {
                            if(signedInError) {
                                secerror("octagon: error signing out: %s", [[signedInError description] UTF8String]);
                            } else {
                                secnotice("octagon", "signed out of octagon trust");
                            }
                            dispatch_semaphore_signal(sema);

                        }];
                        if (0 != dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60 * 5))) {
                            secerror("octagon: Timed out signing out");
                        }
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
