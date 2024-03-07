
#if OCTAGON

#import "utilities/debugging.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

#import "keychain/ot/OTConstants.h"
#import "keychain/ot/OTAccountsAdapter.h"

#import "OSX/sec/ipc/server_security_helpers.h"

@interface OTAccountsActualAdapter ()
@property (strong, nonatomic, nullable) ACAccountStore* store;
@end

@implementation OTAccountsActualAdapter

//test only
- (void)setAccountStore:(ACAccountStore*)store
{
    self.store = store;
}

static int NUM_RETRIES = 5;

- (BOOL)isErrorRetryable:(NSError*)error {
    BOOL isRetryable = NO;
    if (error &&
        [error.domain isEqualToString: NSCocoaErrorDomain] &&
        (error.code == NSXPCConnectionInterrupted || error.code == NSXPCConnectionInvalid)) {
        isRetryable = YES;
    }
    return isRetryable;
}

- (NSArray<ACAccount*>* _Nullable)fetchAccountsRetryingWithError:(NSError**)error
{
    NSArray<ACAccount*>* accounts = nil;
    
    int i = 0;
    bool retry;
     
    do {
        retry = false;
        NSError* reattemptError = nil;
        accounts = [self.store aa_appleAccountsWithError:&reattemptError];
        
        if (accounts) {
            secdebug("octagon-account", "found accounts after (%d) attempts", i);
            return accounts;
        }
        
        if (i < NUM_RETRIES && [self isErrorRetryable:reattemptError]) {
            secnotice("octagon-account", "retrying accountsd XPC, (%d, %@)", i, reattemptError);
            retry = true;
        } else {
            secerror("octagon-account: Can't talk with accountsd: %@", reattemptError);
            if (error) {
                *error = reattemptError;
            }
        }
        ++i;
    } while (retry);
    
    return accounts;
}

- (TPSpecificUser* _Nullable)findAccountForCurrentThread:(id<OTPersonaAdapter>)personaAdapter
                                         optionalAltDSID:(NSString* _Nullable)altDSID
                                   cloudkitContainerName:(NSString*)cloudkitContainerName
                                        octagonContextID:(NSString*)octagonContextID
                                                   error:(NSError**)error
{
#if KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER && HAVE_MOBILE_KEYBAG_SUPPORT
    if (device_is_multiuser()) {
        secerror("findAccountForCurrentThread does not support EDU Mode");
        if (error) {
            *error = [NSError errorWithDomain:OctagonErrorDomain
                                         code:OctagonErrorUnsupportedInEDUMode
                                  description:@"function is unsupported in EDU mode"];
        }
        return nil;
    }
#endif
    BOOL personaIsPrimary = [personaAdapter currentThreadIsForPrimaryiCloudAccount];
    NSString* currentThreadPersonaUniqueString = [personaAdapter currentThreadPersonaUniqueString];

    secinfo("octagon-account", "persona identifier: %@", currentThreadPersonaUniqueString);
    if (!self.store) {
        self.store = [ACAccountStore defaultStore];
    }
    
    NSArray<ACAccount*>* accounts = [self fetchAccountsRetryingWithError:error];
    if (!accounts) {
        secerror("octagon-account: failed to find accounts");
        return nil;
    }

    ACAccount* foundPersonaAccount = nil;
    ACAccount* foundAltDSIDAccount = nil;
    ACAccount* foundPrimaryAccount = nil;
    ACAccount* chosenAccount = nil;

    // Query for the two possible matches and the nil personaIdentifier
    for(ACAccount* x in accounts) {
        if(altDSID != nil && [altDSID isEqualToString:x.aa_altDSID]) {
            foundAltDSIDAccount = x;
        }
        if([currentThreadPersonaUniqueString isEqualToString:x.personaIdentifier]) {
            foundPersonaAccount = x;
        }
        if(x.personaIdentifier == nil && [x aa_isAccountClass:AAAccountClassPrimary]) {
            foundPrimaryAccount = x;
        }
    }
    
    secinfo("octagon-account", "Search Criteria  - persona: %@ altDSID: %@", currentThreadPersonaUniqueString, altDSID);
    secinfo("octagon-account", "Primary account - persona primary: %{BOOL}d altDSID: %@", personaIsPrimary, foundPrimaryAccount.aa_altDSID);
    secinfo("octagon-account", "Match by persona - persona: %@ altDSID: %@", foundPersonaAccount.personaIdentifier, foundPersonaAccount.aa_altDSID);
    secinfo("octagon-account", "Match by altDSID - persona: %@ altDSID: %@", foundAltDSIDAccount.personaIdentifier, foundAltDSIDAccount.aa_altDSID);

    // Choose the entry that best matches here
    if(personaIsPrimary) {
        if(altDSID) {
            if(foundAltDSIDAccount) {
                chosenAccount = foundAltDSIDAccount;
            }
        } else {
            if(foundPrimaryAccount) {
                // Return the primary account if no specific altDSID was requested
                chosenAccount = foundPrimaryAccount;
            } else if(foundPersonaAccount) {
                chosenAccount = foundPersonaAccount;
            }
        }
    }
#if KEYCHAIN_SUPPORTS_PERSONA_MULTIUSER
    else {
        if(altDSID) {
            if(foundPersonaAccount && foundAltDSIDAccount && [foundAltDSIDAccount.personaIdentifier isEqualToString: foundPersonaAccount.personaIdentifier]) {
                chosenAccount = foundPersonaAccount;
            } else {
                secnotice("octagon-account", "Search Criteria  - persona: %@ altDSID: %@", currentThreadPersonaUniqueString, altDSID);
                secnotice("octagon-account", "Primary account - persona primary: %{BOOL}d altDSID: %@", personaIsPrimary, foundPrimaryAccount.aa_altDSID);
                secnotice("octagon-account", "Match by persona - persona: %@ altDSID: %@", foundPersonaAccount.personaIdentifier, foundPersonaAccount.aa_altDSID);
                secnotice("octagon-account", "Match by altDSID - persona: %@ altDSID: %@", foundAltDSIDAccount.personaIdentifier, foundAltDSIDAccount.aa_altDSID);

                if(error) {
                    *error = [NSError errorWithDomain:OctagonErrorDomain
                                                 code:OctagonErrorAltDSIDPersonaMismatch
                                          description:[NSString stringWithFormat:@"AppleAccount mismatch for persona '%@' and altDSID '%@'", currentThreadPersonaUniqueString, altDSID]];
                }
                secnotice("octagon-account", "Persona/altDSID mis-match specified for query");
                return nil;
            }
        } else {
            if(foundPersonaAccount) {
                chosenAccount = foundPersonaAccount;
            }
        }
    }
#endif
    
    if(chosenAccount == nil) {
        if(currentThreadPersonaUniqueString != nil) {
            secnotice("octagon-account", "Unable to find Apple account matching persona %@", currentThreadPersonaUniqueString);
        } else {
            secinfo("octagon-account", "Unable to find Apple account matching primary persona (nil)");
        }
        if(error && !*error) {
            *error = [NSError errorWithDomain:OctagonErrorDomain
                                         code:OctagonErrorNoAppleAccount
                                  description:[NSString stringWithFormat:@"No AppleAccount exists matching persona '%@' and altDSID '%@'", currentThreadPersonaUniqueString, altDSID]];
        }
        return nil;
    }

    NSString* discoveredAltDSID = chosenAccount.aa_altDSID;
    NSString* accountIdentifier = chosenAccount.identifier;

    // We could use the identifier from currentPersona, but then we'd have to coalesce the default/system/personal peronas.
    // Match whatever Accounts does here.
    NSString* discoveredPersonaUniqueString = chosenAccount.personaIdentifier;

    return [[TPSpecificUser alloc] initWithCloudkitContainerName:cloudkitContainerName
                                                octagonContextID:octagonContextID
                                                  appleAccountID:accountIdentifier
                                                         altDSID:discoveredAltDSID
                                                isPrimaryPersona:[chosenAccount aa_isAccountClass:AAAccountClassPrimary]
                                             personaUniqueString:discoveredPersonaUniqueString];
}

- (NSArray<TPSpecificUser*>* _Nullable)inflateAllTPSpecificUsers:(NSString*)cloudkitContainerName
                                                octagonContextID:(NSString*)octagonContextID
{
#if KEYCHAIN_SUPPORTS_EDU_MODE_MULTIUSER && HAVE_MOBILE_KEYBAG_SUPPORT
    if (device_is_multiuser()) {
        secerror("inflateAllTPSpecificUsers does not support EDU Mode");
        return nil;
    }
#endif
    ACAccountStore *store = [ACAccountStore defaultStore];
    NSMutableArray<TPSpecificUser *>* activeAccounts = [NSMutableArray array];

    for(ACAccount* x in store.aa_appleAccounts) {
        TPSpecificUser* activeAccount = [[TPSpecificUser alloc] initWithCloudkitContainerName:cloudkitContainerName
                                                                             octagonContextID:octagonContextID
                                                                               appleAccountID:x.identifier
                                                                                      altDSID:x.aa_altDSID
                                                                             isPrimaryPersona:[x aa_isAccountClass:AAAccountClassPrimary]
                                                                          personaUniqueString:x.personaIdentifier];
        [activeAccounts addObject:activeAccount];
    }
    return activeAccounts;
}

@end

#endif // OCTAGON
