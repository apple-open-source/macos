//
//  KeychainSyncAccountNotification.m
//  Security
//

#import "KeychainSyncAccountNotification.h"
#import <Accounts/Accounts.h>
#import <Accounts/Accounts_Private.h>
#if TARGET_OS_IPHONE
#import <AppleAccount/ACAccount+AppleAccount.h>
#else
#import <AOSAccounts/ACAccount+iCloudAccount.h>
#endif
#import <AccountsDaemon/ACDAccountStore.h>
#import <Security/SecureObjectSync/SOSCloudCircle.h>
#if OCTAGON
#import <keychain/ot/OTControl.h>
#include <utilities/SecCFRelease.h>
#endif
#import "utilities/debugging.h"
#import "OT.h"

@implementation KeychainSyncAccountNotification

- (bool)accountIsPrimary:(ACAccount *)account
{
#if TARGET_OS_IPHONE
    return [account aa_isPrimaryAccount];
#else
    return [account icaIsPrimaryAccount];
#endif
}

// this is where we initialize SOS and OT for account sign-in
// the complement to this logic where we turn off SOS and OT is in KeychainDataclassOwner
// in the future we may bring this logic over there and delete KeychainSyncAccountNotification, but accounts people say that's a change that today would require coordination across multiple teams
// was asked to file this radar for accounts: <rdar://problem/40176124> Invoke DataclassOwner when enabling or signing into an account
- (BOOL)account:(ACAccount *)account willChangeWithType:(ACAccountChangeType)changeType inStore:(ACDAccountStore *)store oldAccount:(ACAccount *)oldAccount {

    if((changeType == kACAccountChangeTypeAdded) &&
       [account.accountType.identifier isEqualToString: ACAccountTypeIdentifierAppleAccount] &&
       [self accountIsPrimary:account]) {
#if OCTAGON
        if(SecOTIsEnabled()){
            __block NSError* error = nil;
            NSString *dsid = account.accountProperties[@"personID"];
            OTControl* otcontrol = [OTControl controlObject:&error];
            
            if (nil == otcontrol) {
                secerror("octagon: Failed to get OTControl: %@", error.localizedDescription);
            } else {
                dispatch_semaphore_t sema = dispatch_semaphore_create(0);
                
                [otcontrol signIn:dsid reply:^(BOOL result, NSError * _Nullable signedInError) {
                    if(!result || signedInError){
                        secerror("octagon: error signing in: %s", [[signedInError description] UTF8String]);
                    }
                    else{
                        secnotice("octagon", "signed into octagon trust");
                    }
                    dispatch_semaphore_signal(sema);
                    
                }];
                if (0 != dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60 * 5))) {
                    secerror("octagon: Timed out signing in");
                }
            }
        }else{
            secerror("Octagon not enabled!");
        }
#endif
    }

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

            }
        }
    }

    
    return YES;
}

@end
