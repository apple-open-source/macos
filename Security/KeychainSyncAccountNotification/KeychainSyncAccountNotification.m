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
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnewline-eof"
#import <AppleAccount/ACAccount+AppleAccount.h>
#pragma clang diagnostic pop
#import <AccountsDaemon/ACDAccountStore.h>
#import <AccountsDaemon/ACDClientAuthorizationManager.h>
#import <AccountsDaemon/ACDClientAuthorization.h>
#import <Security/SOSCloudCircle.h>

@implementation KeychainSyncAccountNotification

- (BOOL)account:(ACAccount *)account willChangeWithType:(ACAccountChangeType)changeType inStore:(ACDAccountStore *)store oldAccount:(ACAccount *)oldAccount {
    if ((changeType == kACAccountChangeTypeDeleted) && [oldAccount.accountType.identifier isEqualToString:ACAccountTypeIdentifierAppleAccount]) {
        if ([account aa_isPrimaryAccount]) {
            
            CFErrorRef removalError = NULL;
            
            ACLogDebug(@"Performing SOS circle credential removal for account %@: %@", oldAccount.identifier, oldAccount.username);
            
            if (!SOSCCRemoveThisDeviceFromCircle(&removalError)) {
                ACLogError(@"Account %@ could not leave the SOS circle: %@", oldAccount.identifier, removalError);
            }
        } else {
            ACLogDebug(@"NOT performing SOS circle credential removal for secondary account %@: %@", account.identifier, account.username);
        }
    }
    
    return YES;
}

- (void)account:(ACAccount *)account didChangeWithType:(ACAccountChangeType)changeType inStore:(ACDAccountStore *)store oldAccount:(ACAccount *)oldAccount {
    if ((changeType == kACAccountChangeTypeAdded || changeType == kACAccountChangeTypeModified) && [account.accountType.identifier isEqualToString:ACAccountTypeIdentifierAppleAccount]) {
        if ([account aa_isPrimaryAccount]) {
            NSError *errObject;
            ACAccountCredential *accountCred = [store credentialForAccount:account error:&errObject];
            if (accountCred != NULL) {
                CFErrorRef authenticateError = NULL;
                if (accountCred.password != NULL) {
                    const char *accountPassword = [accountCred.password cStringUsingEncoding:NSUTF8StringEncoding];
                    CFDataRef passwordData = CFDataCreate(kCFAllocatorDefault, (const uint8_t *)accountPassword, strlen(accountPassword));
                    if (NULL != passwordData) {
                        ACLogDebug(@"Performing SOS circle credential set for account %@: %@", account.identifier, account.username);
                        if (!SOSCCSetUserCredentials((__bridge CFStringRef)(account.username), passwordData, &authenticateError)) {
                            ACLogError(@"Unable to set SOS circle credentials for account %@: %@", account.identifier, authenticateError);
                            if (NULL != authenticateError) {
                                CFRelease(authenticateError);
                            }
                        }
                        CFRelease(passwordData);
                    }
                } else {
                    if (!SOSCCCanAuthenticate(&authenticateError)) {
                        ACLogError(@"Account %@ did not present a password and we could not authenticate the SOS circle: %@", account.identifier, authenticateError);
                        if (NULL != authenticateError) {
                            CFRelease(authenticateError);
                        }
                    }
                }
            } else {
                ACLogError(@"Account %@ did not present a credential for SOS circle: %@", account.identifier, errObject);
            }
        } else {
            ACLogDebug(@"NOT performing SOS circle credential set for secondary account %@: %@", account.identifier, account.username);
        }
    }
}

@end
