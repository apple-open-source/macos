//
//  Security
//

#import <TargetConditionals.h>
#import <Foundation/Foundation.h>
#import <Security/SecInternalReleasePriv.h>
#import <Security/Security.h>
#import <err.h>
#import <OctagonTrust/OctagonTrust.h>

#import "keychain/otctl/OTControlCLI.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/otctl/EscrowRequestCLI.h"
#import "keychain/escrowrequest/Framework/SecEscrowRequest.h"
#include "lib/SecArgParse.h"
#include "utilities/debugging.h"

#if TARGET_OS_WATCH
#import "keychain/otpaird/OTPairingClient.h"
#endif /* TARGET_OS_WATCH */

#import <AppleFeatures/AppleFeatures.h>

int main(int argc, char** argv)
{
    int start = false;
    int signIn = false;
    int signOut = false;
    int resetoctagon = false;
    int resetProtectedData = false;
    int reset = false;
    int userControllableViewsSyncStatus = false;

    int fetchAllBottles = false;
    int recover = false;
    int depart = false;

    int status = false;

    int er_trigger = false;
    int er_status = false;
    int er_reset = false;
    int er_store = false;
    int ckks_policy_flag = false;

    int ttr_flag = false;

    int fetch_escrow_records = false;
    int fetch_all_escrow_records = false;

    int recoverRecord = false;
    int recoverSilentRecord = false;

    int resetAccountCDPContent = false;

    int createCustodianRecoveryKey = false;
    int joinWithCustodianRecoveryKey = false;
    int preflightJoinWithCustodianRecoveryKey = false;
    int removeCustodianRecoveryKey = false;
    int checkCustodianRecoveryKey = false;

    int setRecoveryKey = false;
    int removeRecoveryKey = false;
    int joinWithRecoveryKey = false;
    int checkRecoveryKey = false;
    int preflightJoinWithRecoveryKey = false;

    int createInheritanceKey = false;
    int generateInheritanceKey = false;
    int storeInheritanceKey = false;
    int joinWithInheritanceKey = false;
    int preflightJoinWithInheritanceKey = false;
    int removeInheritanceKey = false;
    int checkInheritanceKey = false;
    int recreateInheritanceKey = false;
    int createInheritanceKeyWithClaimTokenAndWrappingKey = false;

    int fetchAccountSettings = false;
    int fetchAccountWideSettings = false;
    int fetchAccountWideSettingsDefault = false;

    int enableWalrus = false;
    int disableWalrus = false;

    int enableWebAccess = false;
    int disableWebAccess = false;

    int health = false;
    int simulateReceivePush = false;
    int tlkRecoverability = false;
    int machineIDOverride = false;
    int reroll = false;

#if TARGET_OS_WATCH
    int pairme = false;
#endif /* TARGET_OS_WATCH */

    char* bottleIDArg = NULL;
    char* contextNameArg = NULL;
    char* secretArg = NULL;
    char* skipRateLimitingCheckArg = NULL;
    char* recordID = NULL;

    char* overrideForAccountScriptArg = NULL;
    char* overrideEscrowCacheArg = NULL;

    char* machineIDArg = NULL;

    int argEnable = false;
    int argPause = false;

    int json = false;

    int notifyIdMS = false;

    int printAccountMetadata = false;

    int forceFetch = false;
    int repair = false;

    char* altDSIDArg = NULL;
    char* containerStr = NULL;
    char* radarNumber = NULL;
    char* appleIDArg = NULL;
    char* dsidArg = NULL;
    char* wrappingKeyArg = NULL;
    char* wrappedKeyArg = NULL;
    char* claimTokenArg = NULL;
    char* custodianUUIDArg = NULL;
    char* inheritanceUUIDArg = NULL;
    char* timeoutInS = NULL;
    char* recoveryKeyArg = NULL;

    char* idmsTargetContext = NULL;
    char* idmsCuttlefishPassword = NULL;
    struct argument options[] = {
        {.shortname = 's', .longname = "secret", .argument = &secretArg, .description = "escrow secret"},
        {.shortname = 'e', .longname = "bottleID", .argument = &bottleIDArg, .description = "bottle record id"},
        {.shortname = 'r', .longname = "skipRateLimiting", .argument = &skipRateLimitingCheckArg, .description = " enter values YES or NO, option defaults to NO, This gives you the opportunity to skip the rate limiting check when performing the cuttlefish health check"},
        {.shortname = 'j', .longname = "json", .flag = &json, .flagval = true, .description = "Output in JSON"},
        {.shortname = 'i', .longname = "recordID", .argument = &recordID, .flagval = true, .description = "recordID"},

        {.shortname = 'o', .longname = "overrideForAccountScript", .argument = &overrideForAccountScriptArg, .description = " enter values YES or NO, option defaults to NO, This flag is only meant for the setup account for icloud cdp signin script"},
        {.shortname = 'c', .longname = "overrideEscrowCache", .argument = &overrideEscrowCacheArg, .description = " enter values YES or NO, option defaults to NO, include this if you want to force an escrow record fetch from cuttlefish for the freshest of escrow records"},

        {.shortname = 'E', .longname = "enable", .flag = &argEnable, .flagval = true, .description = "Enable something (pair with a modification command)"},
        {.shortname = 'P', .longname = "pause", .flag = &argPause, .flagval = true, .description = "Pause something (pair with a modification command)"},
	{.longname = "notifyIdMS", .flag = &notifyIdMS, .flagval = true, .description = "Notify IdMS on reset", .internal_only = true},
        {.longname = "forceFetch", .flag = &forceFetch, .flagval = true, .description = "Force fetch from cuttlefish"},
        {.longname = "repair", .flag = &repair, .flagval = true, .description = "Perform repair as part of health check"},

        {.shortname = 'a', .longname = "machineID", .argument = &machineIDArg, .description = "machineID override"},

        {.longname = "altDSID", .argument = &altDSIDArg, .description = "altDSID (for sign-in/out)"},
        {.longname = "entropy", .argument = &secretArg, .description = "escrowed entropy in JSON"},

        {.longname = "appleID", .argument = &appleIDArg, .description = "AppleID"},
        {.longname = "dsid", .argument = &dsidArg, .description = "DSID", .internal_only = true},

        {.longname = "container", .argument = &containerStr, .description = "CloudKit container name"},
        {.longname = "context", .argument = &contextNameArg, .description = "Context name"},
        {.longname = "radar", .argument = &radarNumber, .description = "Radar number"},

        {.longname = "wrapping-key", .argument = &wrappingKeyArg, .description = "Wrapping key (for joinWithCustodianRecoveryKey)", .internal_only = true},
        {.longname = "wrapped-key", .argument = &wrappedKeyArg, .description = "Wrapped key (for joinWithCustodianRecoveryKey)", .internal_only = true},
        {.longname = "claim-token", .argument = &claimTokenArg, .description = "Claim token for inheritance", .internal_only = true},
        {.longname = "custodianUUID", .argument = &custodianUUIDArg, .description = "UUID for joinWithCustodianRecoveryKey", .internal_only = true},
        {.longname = "inheritanceUUID", .argument = &inheritanceUUIDArg, .description = "UUID for joinWithInheritanceKey", .internal_only = true},
        {.longname = "timeout", .argument = &timeoutInS, .description = "timeout for command (in s)"},

        {.longname = "idms-target-context", .argument = &idmsTargetContext, .description = "idmsTargetContext", .internal_only = true},
        {.longname = "idms-cuttlefish-password", .argument = &idmsCuttlefishPassword, .description = "idmsCuttlefishPassword", .internal_only = true},

        {.command = "start", .flag = &start, .flagval = true, .description = "Start Octagon state machine", .internal_only = true},
        {.command = "sign-in", .flag = &signIn, .flagval = true, .description = "Inform Cuttlefish container of sign in", .internal_only = true},
        {.command = "sign-out", .flag = &signOut, .flagval = true, .description = "Inform Cuttlefish container of sign out", .internal_only = true},
        {.command = "status", .flag = &status, .flagval = true, .description = "Report Octagon status"},

        {.command = "resetoctagon", .flag = &resetoctagon, .flagval = true, .description = "Reset and establish new Octagon trust", .internal_only = true},
        {.command = "resetProtectedData", .flag = &resetProtectedData, .flagval = true, .description = "Reset ProtectedData", .internal_only = true},
        {.command = "reset", .flag = &reset, .flagval = true, .description = "Reset Octagon trust", .internal_only = true},

        {.command = "user-controllable-views", .flag = &userControllableViewsSyncStatus, .flagval = true, .description = "Modify or view user-controllable views status (If one of --enable or --pause is passed, will modify status)", .internal_only = true},

        {.command = "allBottles", .flag = &fetchAllBottles, .flagval = true, .description = "Fetch all viable bottles"},
        {.command = "recover", .flag = &recover, .flagval = true, .description = "Recover using this bottle"},
        {.command = "depart", .flag = &depart, .flagval = true, .description = "Depart from Octagon Trust", .internal_only = true},

        {.command = "er-trigger", .flag = &er_trigger, .flagval = true, .description = "Trigger an Escrow Request request", .internal_only = true},
        {.command = "er-status", .flag = &er_status, .flagval = true, .description = "Report status on any pending Escrow Request requests"},
        {.command = "er-reset", .flag = &er_reset, .flagval = true, .description = "Delete all Escrow Request requests"},
        {.command = "er-store", .flag = &er_store, .flagval = true, .description = "Store any pending Escrow Request prerecords"},

        {.command = "health", .flag = &health, .flagval = true, .description = "Check Octagon Health status"},
        {.command = "simulate-receive-push", .flag = &simulateReceivePush, .flagval = true, .description = "Simulate receiving a Octagon push notification", .internal_only = true},
        {.command = "ckks-policy", .flag = &ckks_policy_flag, .flagval = true, .description = "Trigger a refetch of the CKKS policy"},

        {.command = "taptoradar", .flag = &ttr_flag, .flagval = true, .description = "Trigger a TapToRadar", .internal_only = true},

        {.command = "fetchEscrowRecords", .flag = &fetch_escrow_records, .flagval = true, .description = "Fetch Escrow Records"},
        {.command = "fetchAllEscrowRecords", .flag = &fetch_all_escrow_records, .flagval = true, .description = "Fetch All Escrow Records"},

        {.command = "recover-record", .flag = &recoverRecord, .flagval = true, .description = "Recover record"},
        {.command = "recover-record-silent", .flag = &recoverSilentRecord, .flagval = true, .description = "Silent record recovery"},

        {.command = "reset-account-cdp-contents", .flag = &resetAccountCDPContent, .flagval = true, .description = "Reset an account's CDP contents (escrow records, kvs data, cuttlefish)", .internal_only = true},

        {.command = "create-custodian-recovery-key", .flag = &createCustodianRecoveryKey, .flagval = true, .description = "Create a custodian recovery key", .internal_only = true},
        {.command = "join-with-custodian-recovery-key", .flag = &joinWithCustodianRecoveryKey, .flagval = true, .description = "Join with a custodian recovery key", .internal_only = true},
        {.command = "preflight-join-with-custodian-recovery-key", .flag = &preflightJoinWithCustodianRecoveryKey, .flagval = true, .description = "Preflight join with a custodian recovery key", .internal_only = true},
        {.command = "remove-custodian-recovery-key", .flag = &removeCustodianRecoveryKey, .flagval = true, .description = "Remove a custodian recovery key", .internal_only = true},
        {.command = "check-custodian-recovery-key", .flag = &checkCustodianRecoveryKey, .flagval = true, .description = "Check a custodian recovery key for existence", .internal_only = true},
        {.command = "create-inheritance-key", .flag = &createInheritanceKey, .flagval = true, .description = "Create an inheritance key", .internal_only = true},
        {.command = "generate-inheritance-key", .flag = &generateInheritanceKey, .flagval = true, .description = "Generate an inheritance key", .internal_only = true},
        {.command = "store-inheritance-key", .flag = &storeInheritanceKey, .flagval = true, .description = "Store an inheritance key", .internal_only = true},
        {.command = "join-with-inheritance-key", .flag = &joinWithInheritanceKey, .flagval = true, .description = "Join with an inheritance key", .internal_only = true},
        {.command = "preflight-join-with-inheritance-key", .flag = &preflightJoinWithInheritanceKey, .flagval = true, .description = "Preflight join with an inheritance key", .internal_only = true},
        {.command = "remove-inheritance-key", .flag = &removeInheritanceKey, .flagval = true, .description = "Remove an inheritance key", .internal_only = true},
        {.command = "check-inheritance-key", .flag = &checkInheritanceKey, .flagval = true, .description = "Check an inheritance key for existence", .internal_only = true},
        {.command = "recreate-inheritance-key", .flag = &recreateInheritanceKey, .flagval = true, .description = "Recreate an inheritance key", .internal_only = true},
        {.command = "create-inheritance-key-with-claim-wrapping", .flag = &createInheritanceKeyWithClaimTokenAndWrappingKey, .flagval = true, .description = "Create an inheritance key given claim+wrapping key", .internal_only = true},

        {.command = "tlk-recoverability", .flag = &tlkRecoverability, .flagval = true, .description = "Evaluate tlk recoverability for an account", .internal_only = true},
        {.command = "set-machine-id-override", .flag = &machineIDOverride, .flagval = true, .description = "Set machineID override"},
        {.command = "remove-recovery-key", .flag = &removeRecoveryKey, .flagval = true, .description = "Remove a recovery key", .internal_only = true},
        {.command = "set-recovery-key", .flag = &setRecoveryKey, .flagval = true, .description = "Set a recovery key", .internal_only = true},
        {.command = "join-with-recovery-key", .flag = &joinWithRecoveryKey, .flagval = true, .description = "Join with a recovery key", .internal_only = true},
        {.command = "check-recovery-key", .flag = &checkRecoveryKey, .flagval = true, .description = "Check a recovery key", .internal_only = true},
        {.command = "preflight-join-with-recovery-key", .flag = &preflightJoinWithRecoveryKey, .flagval = true, .description = "Preflight join with a recovery key", .internal_only = true},
        {.longname = "recoveryKey", .argument = &recoveryKeyArg, .description = "recovery key"},

        {.command = "enable-walrus", .flag = &enableWalrus, .flagval = true, .description = "Enable Walrus Setting", .internal_only = true},
        {.command = "disable-walrus", .flag = &disableWalrus, .flagval = true, .description = "Disable Walrus Setting", .internal_only = true},
        {.command = "enable-webaccess", .flag = &enableWebAccess, .flagval = true, .description = "Enable Web Access Setting", .internal_only = true},
        {.command = "disable-webaccess", .flag = &disableWebAccess, .flagval = true, .description = "Disable Web Access Setting", .internal_only = true},
        {.command = "fetch-account-state", .flag = &fetchAccountSettings, .flagval = true, .description = "Fetch Account Settings", .internal_only = true},
        {.command = "fetch-account-wide-state", .flag = &fetchAccountWideSettings, .flagval = true, .description = "Fetch Account Wide Settings", .internal_only = true},
        {.command = "fetch-account-wide-state-default", .flag = &fetchAccountWideSettingsDefault, .flagval = true, .description = "Fetch Account Wide Settings with Default", .internal_only = true},

#if TARGET_OS_WATCH
        {.command = "pairme", .flag = &pairme, .flagval = true, .description = "Perform pairing (watchOS only)"},
#endif /* TARGET_OS_WATCH */

        {.command = "print-account-metadata", .flag = &printAccountMetadata, .flagval = true, .description = "Print Account Metadata", .internal_only = true},
        {.command = "reroll", .flag = &reroll, .flagval = true, .description = "Reroll PeerID", .internal_only = true},

        {}};

    struct arguments args = {
        .programname = "otctl",
        .description = "Control and report on Octagon Trust",
        .arguments = options,
    };

    if(!options_parse(argc, argv, &args)) {
        printf("\n");
        print_usage(&args);
        return 1;
    }

    @autoreleasepool {
        NSError* error = nil;

        // Use a synchronous control object
        OTControl* rpc = [OTControl controlObject:true error:&error];
        if(error || !rpc) {
            errx(1, "no OTControl failed: %s", [[error description] UTF8String]);
        }

        NSString* context = contextNameArg ? [NSString stringWithCString:contextNameArg encoding:NSUTF8StringEncoding] : OTDefaultContext;
        NSString* container = containerStr ? [NSString stringWithCString:containerStr encoding:NSUTF8StringEncoding] : OTCKContainerName;
        NSString* altDSID = altDSIDArg ? [NSString stringWithCString:altDSIDArg encoding:NSUTF8StringEncoding] : nil;
        NSString* dsid = dsidArg ? [NSString stringWithCString:dsidArg encoding:NSUTF8StringEncoding] : nil;
        NSString* appleID = appleIDArg ? [NSString stringWithCString:appleIDArg encoding:NSUTF8StringEncoding] : nil;

        NSString* skipRateLimitingCheck = skipRateLimitingCheckArg ? [NSString stringWithCString:skipRateLimitingCheckArg encoding:NSUTF8StringEncoding] : @"NO";

        NSString* wrappingKey = wrappingKeyArg ? [NSString stringWithCString:wrappingKeyArg encoding:NSUTF8StringEncoding] : nil;
        NSString* wrappedKey = wrappedKeyArg ? [NSString stringWithCString:wrappedKeyArg encoding:NSUTF8StringEncoding] : nil;
        NSString* claimToken = claimTokenArg ? [NSString stringWithCString:claimTokenArg encoding:NSUTF8StringEncoding] : nil;
        NSString* custodianUUIDString = custodianUUIDArg ? [NSString stringWithCString:custodianUUIDArg encoding:NSUTF8StringEncoding] : nil;
        NSString* inheritanceUUIDString = inheritanceUUIDArg ? [NSString stringWithCString:inheritanceUUIDArg encoding:NSUTF8StringEncoding] : nil;
        NSTimeInterval timeout = timeoutInS ? [[NSString stringWithCString:timeoutInS encoding:NSUTF8StringEncoding] integerValue] : 600;
        NSString* idmsTargetContextString = idmsTargetContext ? [NSString stringWithCString:idmsTargetContext encoding:NSUTF8StringEncoding] : nil;
        NSString* idmsCuttlefishPasswordString = idmsCuttlefishPassword ? [NSString stringWithCString:idmsCuttlefishPassword encoding:NSUTF8StringEncoding] : nil;

        NSString* overrideForAccountScriptString = overrideForAccountScriptArg ? [NSString stringWithCString:overrideForAccountScriptArg encoding:NSUTF8StringEncoding] : @"NO";
        NSString* overrideEscrowCacheString = overrideEscrowCacheArg ? [NSString stringWithCString:overrideEscrowCacheArg encoding:NSUTF8StringEncoding] : @"NO";
        BOOL overrideEscrowCache = [overrideEscrowCacheString isEqualToString:@"YES"];

        NSString* recoveryKey = recoveryKeyArg ? [NSString stringWithCString:recoveryKeyArg encoding:NSUTF8StringEncoding] : nil;

        OTControlCLI* ctl = [[OTControlCLI alloc] initWithOTControl:rpc];

        OTControlArguments* arguments = [[OTControlArguments alloc] initWithContainerName:container
                                                                                contextID:context
                                                                                  altDSID:altDSID
                                                                                   flowID:[NSString stringWithFormat:@"otctl-flowID-%@", [NSUUID UUID].UUIDString]
                                                                          deviceSessionID:[NSString stringWithFormat:@"otctl-deviceSessionID-%@", [NSUUID UUID].UUIDString]];

        NSError* escrowRequestError = nil;
        EscrowRequestCLI* escrowctl = [[EscrowRequestCLI alloc] initWithEscrowRequest:[SecEscrowRequest request:&escrowRequestError]];
        if(escrowRequestError) {
            errx(1, "SecEscrowRequest failed: %s", [[escrowRequestError description] UTF8String]);
        }
        if(resetoctagon) {
            return [ctl resetOctagon:arguments 
                   idmsTargetContext:idmsTargetContextString
              idmsCuttlefishPassword:idmsCuttlefishPasswordString
                          notifyIdMS:notifyIdMS
                             timeout:timeout];
        }
        if(resetProtectedData) {
            return [ctl resetProtectedData:arguments 
                                   appleID:appleID
                                      dsid:dsid
                         idmsTargetContext:idmsTargetContextString
                    idmsCuttlefishPassword:idmsCuttlefishPasswordString
                                notifyIdMS:notifyIdMS];
        }
        if(reset) {
            return [ctl reset:arguments 
                      appleID:appleID
                         dsid:dsid];

        }
        if(userControllableViewsSyncStatus) {
            if(argEnable && argPause) {
                print_usage(&args);
                return 1;
            }

            if(argEnable == false && argPause == false) {
                return [ctl fetchUserControllableViewsSyncStatus:arguments];
            }

            // At this point, we're sure that either argEnabled or argPause is set; so the value of argEnabled captures the user's intention
            return [ctl setUserControllableViewsSyncStatus:arguments enabled:argEnable];
        }

        if(fetchAllBottles) {
            return [ctl fetchAllBottles:arguments control:rpc overrideEscrowCache:overrideEscrowCache];
        }
        if(recover) {
            NSString* entropyJSON = secretArg ? [NSString stringWithCString:secretArg encoding:NSUTF8StringEncoding] : nil;
            NSString* bottleID = bottleIDArg ? [NSString stringWithCString:bottleIDArg encoding:NSUTF8StringEncoding] : nil;

            if(!entropyJSON || !bottleID) {
                print_usage(&args);
                return 1;
            }

            NSData* entropy = [[NSData alloc] initWithBase64EncodedString:entropyJSON options:0];
            if(!entropy) {
                errx(1, "bad base64 string passed to --entropy");
            }

            return [ctl recoverUsingBottleID:bottleID
                                     entropy:entropy
                                   arguments:arguments
                                     control:rpc];
        }
        if(depart) {
            return [ctl depart:arguments];
        }
        if(start) {
            return [ctl startOctagonStateMachine:arguments];
        }
        if(signIn) {
            return [ctl signIn:arguments];
        }
        if(signOut) {
            return [ctl signOut:arguments];
        }

        if(status) {
            return [ctl status:arguments json:json];
        }
        if(fetch_escrow_records) {
            return [ctl fetchEscrowRecords:arguments json:json overrideEscrowCache:overrideEscrowCache];
        }
        if(fetch_all_escrow_records) {
            return [ctl fetchAllEscrowRecords:arguments json:json overrideEscrowCache:overrideEscrowCache];
        }
        if (machineIDOverride) {
            NSString* machineID;
            if (machineIDArg != NULL) {
                machineID = [NSString stringWithCString:machineIDArg encoding:NSUTF8StringEncoding];
                printf("machineID: %s\n", machineID.description.UTF8String);
            } else {
                printf("unsetting machineID\n");
            }
            return [ctl setMachineIDOverride:arguments machineID:machineID json:json];
        }
        if(recoverRecord) {
            NSString* recordIDString = recordID ? [NSString stringWithCString:recordID encoding:NSUTF8StringEncoding] : nil;
            NSString* secret = secretArg ? [NSString stringWithCString:secretArg encoding:NSUTF8StringEncoding] : nil;

            if(!recordIDString || !secret || !appleID) {
                print_usage(&args);
                return 1;
            }

            BOOL overrideForAccountScript = [overrideForAccountScriptString isEqualToString:@"YES"];

            return [ctl performEscrowRecovery:arguments
                                     recordID:recordIDString
                                      appleID:appleID
                                       secret:secret
                     overrideForAccountScript:overrideForAccountScript
                          overrideEscrowCache:overrideEscrowCache];
        }
        if(recoverSilentRecord){
            NSString* secret = secretArg ? [NSString stringWithCString:secretArg encoding:NSUTF8StringEncoding] : nil;

            if(!secret || !appleID) {
                print_usage(&args);
                return 1;
            }

            return [ctl performSilentEscrowRecovery:arguments appleID:appleID secret:secret];
        }
        if(health) {
            BOOL skip = NO;
            if([skipRateLimitingCheck isEqualToString:@"YES"]) {
                skip = YES;
            } else {
                skip = NO;
            }
            return [ctl healthCheck:arguments skipRateLimitingCheck:skip repair:repair json:json];
        }

        if(simulateReceivePush) {
            return [ctl simulateReceivePush:arguments json:json];
        }
        if(tlkRecoverability) {
            return [ctl tlkRecoverability:arguments];
        }
        if(ckks_policy_flag) {
            return [ctl refetchCKKSPolicy:arguments];
        }
        if (ttr_flag) {
            if (radarNumber == NULL) {
                radarNumber = "1";
            }
            return [ctl tapToRadar:@"action" description:@"description" radar:[NSString stringWithUTF8String:radarNumber]];
        }
        if(resetAccountCDPContent){
            return [ctl resetAccountCDPContentsWithArguments:arguments idmsTargetContext:idmsTargetContextString idmsCuttlefishPassword:idmsCuttlefishPasswordString notifyIdMS:notifyIdMS ];
        }
        if(createCustodianRecoveryKey) {
            return [ctl createCustodianRecoveryKeyWithArguments:arguments uuidString:custodianUUIDString json:json timeout:timeout];
        }
        if(joinWithCustodianRecoveryKey) {
            if (!wrappingKey || !wrappedKey || !custodianUUIDString) {
                print_usage(&args);
                return 1;
            }
            return [ctl joinWithCustodianRecoveryKeyWithArguments:arguments
                                                      wrappingKey:wrappingKey
                                                       wrappedKey:wrappedKey
                                                       uuidString:custodianUUIDString
                                                          timeout:timeout];
        }
        if(preflightJoinWithCustodianRecoveryKey) {
            if (!wrappingKey || !wrappedKey || !custodianUUIDString) {
                print_usage(&args);
                return 1;
            }
            return [ctl preflightJoinWithCustodianRecoveryKeyWithArguments:arguments
                                                               wrappingKey:wrappingKey
                                                                wrappedKey:wrappedKey
                                                                uuidString:custodianUUIDString
                                                                   timeout:timeout];
        }
        if(removeCustodianRecoveryKey) {
            if (!custodianUUIDString) {
                print_usage(&args);
                return 1;
            }
            return [ctl removeCustodianRecoveryKeyWithArguments:arguments
                                                     uuidString:custodianUUIDString
                                                        timeout:timeout];
        }
        
        if (checkCustodianRecoveryKey) {
            if (!custodianUUIDString) {
                print_usage(&args);
                return 1;
            }
            return [ctl checkCustodianRecoveryKeyWithArguments:arguments
                                                    uuidString:custodianUUIDString
                                                       timeout:timeout];
        }
        
        if (removeRecoveryKey) {
            return [ctl removeRecoveryKeyWithArguments:arguments];
        }

        if (setRecoveryKey) {
            return [ctl setRecoveryKeyWithArguments:arguments];
        }

        if (joinWithRecoveryKey) {
            if (!recoveryKey) {
                print_usage(&args);
                return 1;
            }
            return [ctl joinWithRecoveryKeyWithArguments:arguments recoveryKey:recoveryKey];
        }

        if (checkRecoveryKey) {
            return [ctl checkRecoveryKeyWithArguments:arguments];
        }

        if (preflightJoinWithRecoveryKey) {
            if (!recoveryKey) {
                print_usage(&args);
                return 1;
            }
            return [ctl preflightJoinWithRecoveryKeyWithArguments:arguments recoveryKey:recoveryKey];
        }

        if(createInheritanceKey) {
            return [ctl createInheritanceKeyWithArguments:arguments uuidString:inheritanceUUIDString json:json timeout:timeout];
        }
        if(generateInheritanceKey) {
            return [ctl generateInheritanceKeyWithArguments:arguments json:json timeout:timeout];
        }
        if(storeInheritanceKey) {
            if (!wrappingKey || !wrappedKey || !inheritanceUUIDString) {
                print_usage(&args);
                return 1;
            }
            return [ctl storeInheritanceKeyWithArguments:arguments
                                             wrappingKey:wrappingKey
                                              wrappedKey:wrappedKey
                                              uuidString:inheritanceUUIDString
                                                 timeout:timeout];
        }
        if(joinWithInheritanceKey) {
            if (!wrappingKey || !wrappedKey || !inheritanceUUIDString) {
                print_usage(&args);
                return 1;
            }
            return [ctl joinWithInheritanceKeyWithArguments:arguments
                                                wrappingKey:wrappingKey
                                                 wrappedKey:wrappedKey
                                                 uuidString:inheritanceUUIDString
                                                    timeout:timeout];
        }
        if(preflightJoinWithInheritanceKey) {
            if (!wrappingKey || !wrappedKey || !inheritanceUUIDString) {
                print_usage(&args);
                return 1;
            }
            return [ctl preflightJoinWithInheritanceKeyWithArguments:arguments
                                                         wrappingKey:wrappingKey
                                                          wrappedKey:wrappedKey
                                                          uuidString:inheritanceUUIDString
                                                             timeout:timeout];
        }
        if(removeInheritanceKey) {
            if (!inheritanceUUIDString) {
                print_usage(&args);
                return 1;
            }
            return [ctl removeInheritanceKeyWithArguments:arguments
                                               uuidString:inheritanceUUIDString
                                                  timeout:timeout];
        }
        if (checkInheritanceKey) {
            if (!inheritanceUUIDString) {
                print_usage(&args);
                return 1;
            }
            return [ctl checkInheritanceKeyWithArguments:arguments
                                              uuidString:inheritanceUUIDString
                                                 timeout:timeout];
        }
        if(recreateInheritanceKey) {
            if (!wrappingKey || !wrappedKey || !claimToken) {
                print_usage(&args);
                return 1;
            }

            return [ctl recreateInheritanceKeyWithArguments:arguments
                                                 uuidString:inheritanceUUIDString
                                                wrappingKey:wrappingKey
                                                 wrappedKey:wrappedKey
                                                 claimToken:claimToken
                                                       json:json
                                                    timeout:timeout];
        }
        if(createInheritanceKeyWithClaimTokenAndWrappingKey) {
            if (!wrappingKey || !claimToken) {
                print_usage(&args);
                return 1;
            }

            return [ctl createInheritanceKeyWithClaimTokenAndWrappingKey:arguments
                                                              uuidString:inheritanceUUIDString
                                                              claimToken:claimToken
                                                             wrappingKey:wrappingKey
                                                                    json:json
                                                                 timeout:timeout];
        }

        if(enableWalrus) {
            return [ctl enableWalrusWithArguments:arguments timeout:timeout];
        }
        if(disableWalrus) {
            return [ctl disableWalrusWithArguments:arguments timeout:timeout];
        }
        
        if(enableWebAccess) {
            return [ctl enableWebAccessWithArguments:arguments timeout:timeout];
        }
        if(disableWebAccess) {
            return [ctl disableWebAccessWithArguments:arguments timeout:timeout];
        }
        
        if(fetchAccountSettings) {
            return [ctl fetchAccountSettingsWithArguments:arguments json:json];
        }
        if(fetchAccountWideSettings) {
            return [ctl fetchAccountWideSettingsWithArguments:arguments useDefault:false forceFetch:forceFetch json:json];
        }

        if(fetchAccountWideSettingsDefault) {
            return [ctl fetchAccountWideSettingsWithArguments:arguments useDefault:true forceFetch:forceFetch json:json];
        }

        if(er_trigger) {
            return (int)[escrowctl trigger];
        }
        if(er_status) {
            return (int)[escrowctl status];
        }
        if(er_reset) {
            return (int)[escrowctl reset];
        }
        if(er_store) {
            return (int)[escrowctl storePrerecordsInEscrow];
        }

#if TARGET_OS_WATCH
        if (pairme) {
            dispatch_semaphore_t sema = dispatch_semaphore_create(0);
            OTPairingInitiateWithCompletion(NULL, true, ^(bool success, NSError *pairingError) {
                if (success) {
                    printf("successfully paired with companion\n");
                } else {
                    printf("failed to pair with companion: %s\n", pairingError.description.UTF8String);
                }
                dispatch_semaphore_signal(sema);
            });
            dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
            return 0;
        }
#endif /* TARGET_OS_WATCH */
        if (printAccountMetadata) {
            return [ctl printAccountMetadataWithArguments:arguments json:json];
        }
        if (reroll) {
            return [ctl rerollWithArguments:arguments json:json];
        }

        print_usage(&args);
        return 1;
    }
    return 0;
}
