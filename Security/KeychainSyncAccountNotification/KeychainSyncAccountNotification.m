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

#if OCTAGON

static bool SecOTIsEnabled(void)
{
    bool userDefaultsShouldBottledPeer = true;
    CFBooleanRef enabled = (CFBooleanRef)CFPreferencesCopyValue(CFSTR("EnableOTRestore"),
                                                                CFSTR("com.apple.security"),
                                                                kCFPreferencesAnyUser, kCFPreferencesAnyHost);
    if(enabled && CFGetTypeID(enabled) == CFBooleanGetTypeID()){
        if(enabled == kCFBooleanFalse){
            secnotice("octagon", "Octagon Restore Disabled");
            userDefaultsShouldBottledPeer = false;
        }
        if(enabled == kCFBooleanTrue){
            secnotice("octagon", "Octagon Restore Enabled");
            userDefaultsShouldBottledPeer = true;
        }
    }
    
    CFReleaseNull(enabled);
    return userDefaultsShouldBottledPeer;
}

#endif

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
    NSString* oldAccountIdentifier = oldAccount.identifier;
    NSString* accountIdentifier = account.identifier;
    
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
        if(oldAccountIdentifier != NULL && oldAccount.username !=NULL) {
            if ([self accountIsPrimary:oldAccount]) {
                CFErrorRef removalError = NULL;
                
                secinfo("accounts", "Performing SOS circle credential removal for account %@: %@", oldAccountIdentifier, oldAccount.username);
                
                if (!SOSCCLoggedOutOfAccount(&removalError)) {
                    secerror("Account %@ could not leave the SOS circle: %@", oldAccountIdentifier, removalError);
                }
#if OCTAGON
                if(SecOTIsEnabled()){
                    __block NSError* error = nil;
                    OTControl* otcontrol = [OTControl controlObject:&error];
                   
                    if (nil == otcontrol) {
                        secerror("octagon: Failed to get OTControl: %@", error.localizedDescription);
                    } else {
                        dispatch_semaphore_t sema = dispatch_semaphore_create(0);
                        
                        [otcontrol signOut:^(BOOL result, NSError * _Nullable signedOutError) {
                            if(!result || signedOutError){
                                secerror("octagon: error signing out: %s", [[signedOutError description] UTF8String]);
                            }
                            else{
                                secnotice("octagon", "signed out of octagon trust");
                            }
                            dispatch_semaphore_signal(sema);
                        }];
                        if (0 != dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60 * 5))) {
                            secerror("octagon: Timed out signing out");
                        }
                    }
                }
                else{
                    secerror("Octagon not enabled!");
                }
#endif
            } else {
                secinfo("accounts", "NOT performing SOS circle credential removal for secondary account %@: %@", accountIdentifier, account.username);
            }
        } else{
            secinfo("accounts", "Already logged out of account");
        }
    }
    
    return YES;
}

@end
