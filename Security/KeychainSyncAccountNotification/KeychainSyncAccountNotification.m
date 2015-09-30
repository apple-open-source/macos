//
//  KeychainSyncAccountNotification.m
//  Security
//
//  Created by keith on 5/2/13.
//
//

#import "KeychainSyncAccountNotification.h"
#import <Accounts/ACLogging.h>
#import <Accounts/Accounts.h>
#import <Accounts/Accounts_Private.h>
#import <AppleAccount/ACAccount+AppleAccount.h>
#import <AccountsDaemon/ACDAccountStore.h>
#import <AccountsDaemon/ACDClientAuthorizationManager.h>
#import <AccountsDaemon/ACDClientAuthorization.h>
#import <Security/SOSCloudCircle.h>

@implementation KeychainSyncAccountNotification

- (BOOL)account:(ACAccount *)account willChangeWithType:(ACAccountChangeType)changeType inStore:(ACDAccountStore *)store oldAccount:(ACAccount *)oldAccount {
    
    if ((changeType == kACAccountChangeTypeDeleted) && [oldAccount.accountType.identifier isEqualToString:ACAccountTypeIdentifierAppleAccount]) {
        if(oldAccount.identifier != NULL && oldAccount.username !=NULL){
            
            if ([oldAccount aa_isPrimaryAccount]) {
                
                CFErrorRef removalError = NULL;
                
                ACLogDebug(@"Performing SOS circle credential removal for account %@: %@", oldAccount.identifier, oldAccount.username);
                
                if (!SOSCCLoggedOutOfAccount(&removalError)) {
                    ACLogError(@"Account %@ could not leave the SOS circle: %@", oldAccount.identifier, removalError);
                }
            } else {
                ACLogDebug(@"NOT performing SOS circle credential removal for secondary account %@: %@", account.identifier, account.username);
            }
        }
        else{
            ACLogDebug(@"Already logged out of account");
            
        }
    }
    
    return YES;
}

- (void)account:(ACAccount *)account didChangeWithType:(ACAccountChangeType)changeType inStore:(ACDAccountStore *)store oldAccount:(ACAccount *)oldAccount {
	if (changeType == kACAccountChangeTypeDeleted) {
        if (oldAccount.identifier != NULL && oldAccount.username != NULL){
            if ([oldAccount aa_isPrimaryAccount]) {
                CFErrorRef removalError = NULL;
                ACLogDebug(@"Performing SOS circle credential removal for account %@: %@", oldAccount.identifier, oldAccount.username);
                if (!SOSCCLoggedOutOfAccount(&removalError)) {
                    ACLogError(@"Account %@ could not leave the SOS circle: %@", oldAccount.identifier, removalError);
                }
            } else {
                ACLogDebug(@"NOT performing SOS circle credential removal for secondary account %@: %@", account.identifier, account.username);
            }
        }
        ACLogDebug(@"Already logged out of account");
    }
}

@end
