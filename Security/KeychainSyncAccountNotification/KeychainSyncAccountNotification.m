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
#import <AccountsDaemon/ACDClientAuthorizationManager.h>
#import <AccountsDaemon/ACDClientAuthorization.h>
#import <Security/SecureObjectSync/SOSCloudCircle.h>

#import "utilities/debugging.h"

@implementation KeychainSyncAccountNotification


- (bool)accountIsPrimary:(ACAccount *)account
{
#if TARGET_OS_IPHONE
    return [account aa_isPrimaryAccount];
#else
    return [account icaIsPrimaryAccount];
#endif
}

- (BOOL)account:(ACAccount *)account willChangeWithType:(ACAccountChangeType)changeType inStore:(ACDAccountStore *)store oldAccount:(ACAccount *)oldAccount {
    
    if ((changeType == kACAccountChangeTypeDeleted) && [oldAccount.accountType.identifier isEqualToString:ACAccountTypeIdentifierAppleAccount]) {
        if(oldAccount.identifier != NULL && oldAccount.username !=NULL){
            
            if ([self accountIsPrimary:oldAccount]) {
                
                CFErrorRef removalError = NULL;
                
                secinfo("accounts", "Performing SOS circle credential removal for account %@: %@", oldAccount.identifier, oldAccount.username);
                
                if (!SOSCCLoggedOutOfAccount(&removalError)) {
                    secerror("Account %@ could not leave the SOS circle: %@", oldAccount.identifier, removalError);
                }
            } else {
                secinfo("accounts", "NOT performing SOS circle credential removal for secondary account %@: %@", account.identifier, account.username);
            }
        }
        else{
            secinfo("accounts", "Already logged out of account");
            
        }
    }
    
    return YES;
}

- (void)account:(ACAccount *)account didChangeWithType:(ACAccountChangeType)changeType inStore:(ACDAccountStore *)store oldAccount:(ACAccount *)oldAccount {
	if (changeType == kACAccountChangeTypeDeleted) {
        if (oldAccount.identifier != NULL && oldAccount.username != NULL){

            if ([self accountIsPrimary:oldAccount]) {
                CFErrorRef removalError = NULL;
                secinfo("accounts", "Performing SOS circle credential removal for account %@: %@", oldAccount.identifier, oldAccount.username);
                if (!SOSCCLoggedOutOfAccount(&removalError)) {
                    secerror("Account %@ could not leave the SOS circle: %@", oldAccount.identifier, removalError);
                }
            } else {
                secinfo("accounts", "NOT performing SOS circle credential removal for secondary account %@: %@", account.identifier, account.username);
            }
        }
        secinfo("accounts", "Already logged out of account");
    }
}

@end
