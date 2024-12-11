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
#import "keychain/ot/OTControl.h"
#include "utilities/SecCFRelease.h"

#import <KeychainCircle/SecurityAnalyticsConstants.h>
#import <KeychainCircle/SecurityAnalyticsReporterRTC.h>
#import <KeychainCircle/AAFAnalyticsEvent+Security.h>

#import "ipc/securityd_client.h"
#if KEYCHAIN_SUPPORTS_PERSONA_MULTIUSER
#import <UserManagement/UserManagement.h>
#endif // KEYCHAIN_SUPPORTS_PERSONA_MULTIUSER

#endif // OCTAGON

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

    if((changeType == kACAccountChangeTypeAdded || changeType == kACAccountChangeTypeModified || changeType == kACAccountChangeTypeWarmingUp) &&
       [account.accountType.identifier isEqualToString: ACAccountTypeIdentifierAppleAccount] &&
       (OctagonSupportsPersonaMultiuser() || [self accountIsPrimary:account])) {

        SOSCCLoggedIntoAccount(NULL);

#if OCTAGON
        NSString* altDSID = [account aa_altDSID];

#if KEYCHAIN_SUPPORTS_PERSONA_MULTIUSER
        UMUserPersona * persona = [[UMUserManager sharedManager] currentPersona];
        
        bool isPrimary = [self accountIsPrimary:account];
        secnotice("octagon-account", "Received an %@ Apple account modification (altDSID %@, persona %@(%d))", isPrimary ? @"primary" : @"guest", altDSID, persona.userPersonaUniqueString, (int)persona.userPersonaType);
#else
        secnotice("octagon-account", "Received an primary Apple account modification (altDSID %@)", altDSID);
#endif  // KEYCHAIN_SUPPORTS_PERSONA_MULTIUSER

        __block NSError* error = nil;

        // Use asynchronous XPC here for speed and just hope it works
        OTControl* otcontrol = [OTControl controlObject:false error:&error];
        
        if (nil == otcontrol) {
            secerror("octagon-account: Failed to get OTControl: %@", error.localizedDescription);
        } else {
            OTControlArguments* arguments = [[OTControlArguments alloc] initWithAltDSID:altDSID];

            AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                                         altDSID:altDSID
                                                                                                       eventName:kSecurityRTCEventNamePrimaryAccountAdded category:kSecurityRTCEventCategoryAccountDataAccessRecovery];

            [otcontrol appleAccountSignedIn:arguments reply:^(NSError * _Nullable signedInError) {
                // take a retain on otcontrol so it won't invalidate the connection
                (void)otcontrol;

                if(signedInError) {
                    secerror("octagon-account: error signing in: %s", [[signedInError description] UTF8String]);
                    [eventS addMetrics:@{kSecurityRTCFieldOctagonSignInResult : @(NO)}];
                    [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:signedInError];
                } else {
                    secnotice("octagon-account", "account now signed in for octagon operation");
                    [eventS addMetrics:@{kSecurityRTCFieldOctagonSignInResult : @(YES)}];
                    [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
                }
            }];
        }
#endif
    }

    // If there is any change to any AuthKit account's security level, notify octagon

#if OCTAGON
    if([account.accountType.identifier isEqualToString: ACAccountTypeIdentifierIDMS]) {
        NSString* altDSID = [account aa_altDSID];

#if KEYCHAIN_SUPPORTS_PERSONA_MULTIUSER
        UMUserPersona * persona = [[UMUserManager sharedManager] currentPersona];
        secnotice("octagon-account", "Received an IDMS account modification (altDSID: %@, persona %@(%d))", altDSID, persona.userPersonaUniqueString, (int)persona.userPersonaType);
#else
        secnotice("octagon-account", "Received an IDMS account modification (altDSID: %@)", altDSID);
#endif // KEYCHAIN_SUPPORTS_PERSONA_MULTIUSER

        AKAccountManager *manager = [AKAccountManager sharedInstance];

        AKAppleIDSecurityLevel oldSecurityLevel = [manager securityLevelForAccount:oldAccount];
        AKAppleIDSecurityLevel newSecurityLevel = [manager securityLevelForAccount:account];

        if(oldSecurityLevel != newSecurityLevel) {
            secnotice("octagon-account", "IDMS security level has now moved to %ld for altDSID %@", (unsigned long)newSecurityLevel, altDSID);
            NSString* accountType = nil;
            switch (newSecurityLevel)
            {
                case AKAppleIDSecurityLevelUnknown:
                    accountType = @"Unknown";
                    break;
                case AKAppleIDSecurityLevelPasswordOnly:
                    accountType = @"PasswordOnly";
                    break;
                case AKAppleIDSecurityLevelStandard:
                    accountType = @"Standard";
                    break;
                case AKAppleIDSecurityLevelHSA1:
                    accountType = @"HSA1";
                    break;
                case AKAppleIDSecurityLevelHSA2:
                    accountType = @"HSA2";
                    break;
                case AKAppleIDSecurityLevelManaged:
                    accountType = @"Managed";
                    break;
                default:
                    accountType = @"oh no please file a radar to Security | iCloud Keychain security level";
                    break;
            }
            
            secnotice("octagon-account", "Security level for altDSID %@ is %lu.  Account type: %@", altDSID, (unsigned long)newSecurityLevel, accountType);
            
            __block NSError* error = nil;
            // Use an asynchronous otcontrol for Speed But Not Necessarily Correctness
            OTControl* otcontrol = [OTControl controlObject:false error:&error];
            if(!otcontrol || error) {
                secerror("octagon-account: Failed to get OTControl: %@", error);
            } else {
                OTControlArguments* arguments = [[OTControlArguments alloc] initWithAltDSID:altDSID];
               
                AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:@{kSecurityRTCFieldSecurityLevel : @(newSecurityLevel)} 
                                                                                                             altDSID:altDSID 
                                                                                                           eventName:kSecurityRTCEventNameIdMSSecurityLevel
                                                                                                            category:kSecurityRTCEventCategoryAccountDataAccessRecovery];

                [otcontrol notifyIDMSTrustLevelChangeForAltDSID:arguments reply:^(NSError * _Nullable idmsError) {
                    // take a retain on otcontrol so it won't invalidate the connection
                    (void)otcontrol;

                    if(idmsError) {
                        secerror("octagon-account: error with idms trust level change in: %s", [[idmsError description] UTF8String]);
                    } else {
                        secnotice("octagon-account", "informed octagon of IDMS trust level change");
                    }
                    
                    BOOL success = (idmsError == nil) ? YES : NO;
                    [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:success error:idmsError];
                }];
            }

        } else {
            secnotice("octagon-account", "No change to IDMS security level (%lu) for altDSID %@", (unsigned long)newSecurityLevel, altDSID);
        }
    }
#endif

    if ((changeType == kACAccountChangeTypeDeleted) && [oldAccount.accountType.identifier isEqualToString:ACAccountTypeIdentifierAppleAccount]) {
        NSString* altDSID = [oldAccount aa_altDSID];
#if KEYCHAIN_SUPPORTS_PERSONA_MULTIUSER
        UMUserPersona * persona = [[UMUserManager sharedManager] currentPersona];
        secnotice("octagon-account", "Received an Apple account deletion (altDSID: %@, persona %@(%d))", altDSID, persona.userPersonaUniqueString, (int)persona.userPersonaType);
#else
        secnotice("octagon-account", "Received an Apple account deletion (altDSID %@)", altDSID);
#endif // KEYCHAIN_SUPPORTS_PERSONA_MULTIUSER

        NSString *accountIdentifier = oldAccount.identifier;
        NSString *username = oldAccount.username;

        if(accountIdentifier != NULL && username !=NULL) {
            if (OctagonSupportsPersonaMultiuser() || [self accountIsPrimary:oldAccount]) {
                CFErrorRef removalError = NULL;

                secinfo("accounts", "Performing SOS circle credential removal for account %@: %@", accountIdentifier, username);

                if (SOSCompatibilityModeEnabled()) { // This is the feature flag check for SOS Deferral
                    if (SOSCCIsSOSTrustAndSyncingEnabled()) {
                        CFErrorRef sosOffError = NULL;
                        if (!SOSCCSetCompatibilityMode(false, &sosOffError)) { // This call sets SOS to OFF and performs all of the steps in SOSCCLoggedOutOfAccount()
                            secerror("Failed to turn SOS off for Account %@, error: %@", accountIdentifier, sosOffError);
                        }
                    } else {
                        secnotice("octagon-account", "SOS is already off, don't need to call SOSCCSetCompatibilityMode");
                    }
                } else {
                    if (!SOSCCLoggedOutOfAccount(&removalError)) {
                        secerror("Account %@ could not leave the SOS circle: %@", accountIdentifier, removalError);
                    }
                }

#if OCTAGON
                __block NSError* error = nil;

                // Use an asynchronous control for Speed
                OTControl* otcontrol = [OTControl controlObject:false error:&error];

                if (nil == otcontrol) {
                    secerror("octagon-account: Failed to get OTControl: %@", error.localizedDescription);
                } else {
                    OTControlArguments* arguments = [[OTControlArguments alloc] initWithAltDSID:altDSID];

                    [otcontrol appleAccountSignedOut:arguments reply:^(NSError * _Nullable signedInError) {
                        // take a retain on otcontrol so it won't invalidate the connection
                        (void)otcontrol;

                        if(signedInError) {
                            secerror("octagon-account: error signing out: %s", [[signedInError description] UTF8String]);
                        } else {
                            secnotice("octagon-account", "signed out of octagon trust");
                        }
                    }];
                }
#endif
            }
        }
    }
}

@end
