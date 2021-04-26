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
#import "keychain/otctl/EscrowRequestCLI.h"
#import "keychain/escrowrequest/Framework/SecEscrowRequest.h"
#include "lib/SecArgParse.h"
#include "utilities/debugging.h"

#if TARGET_OS_WATCH
#import "keychain/otpaird/OTPairingClient.h"
#endif /* TARGET_OS_WATCH */

static int start = false;
static int signIn = false;
static int signOut = false;
static int resetoctagon = false;
static int resetProtectedData = false;
static int userControllableViewsSyncStatus = false;

static int fetchAllBottles = false;
static int recover = false;
static int depart = false;

static int status = false;

static int er_trigger = false;
static int er_status = false;
static int er_reset = false;
static int er_store = false;
static int ckks_policy_flag = false;

static int ttr_flag = false;

static int fetch_escrow_records = false;
static int fetch_all_escrow_records = false;

static int recoverRecord = false;
static int recoverSilentRecord = false;

static int resetAccountCDPContent = false;

static int health = false;

#if TARGET_OS_WATCH
static int pairme = false;
#endif /* TARGET_OS_WATCH */

static char* bottleIDArg = NULL;
static char* contextNameArg = NULL;
static char* secretArg = NULL;
static char* skipRateLimitingCheckArg = NULL;
static char* recordID = NULL;

static int argEnable = false;
static int argPause = false;

static int json = false;

static char* altDSIDArg = NULL;
static char* containerStr = NULL;
static char* radarNumber = NULL;
static char* appleIDArg = NULL;
static char* dsidArg = NULL;

static void internalOnly(void)
{
    if(!SecIsInternalRelease()) {
        secnotice("octagon", "Tool not available on non internal builds");
        errx(1, "Tool not available on non internal builds");
    }
}

int main(int argc, char** argv)
{
    static struct argument options[] = {
        {.shortname = 's', .longname = "secret", .argument = &secretArg, .description = "escrow secret"},
        {.shortname = 'e', .longname = "bottleID", .argument = &bottleIDArg, .description = "bottle record id"},
        {.shortname = 'r', .longname = "skipRateLimiting", .argument = &skipRateLimitingCheckArg, .description = " enter values YES or NO, option defaults to NO, This gives you the opportunity to skip the rate limiting check when performing the cuttlefish health check"},
        {.shortname = 'j', .longname = "json", .flag = &json, .flagval = true, .description = "Output in JSON"},
        {.shortname = 'i', .longname = "recordID", .argument = &recordID, .flagval = true, .description = "recordID"},

        {.shortname = 'E', .longname = "enable", .flag = &argEnable, .flagval = true, .description = "Enable something (pair with a modification command)"},
        {.shortname = 'P', .longname = "pause", .flag = &argPause, .flagval = true, .description = "Pause something (pair with a modification command)"},

        {.longname = "altDSID", .argument = &altDSIDArg, .description = "altDSID (for sign-in/out)"},
        {.longname = "entropy", .argument = &secretArg, .description = "escrowed entropy in JSON"},

        {.longname = "appleID", .argument = &appleIDArg, .description = "AppleID"},
        {.longname = "dsid", .argument = &dsidArg, .description = "DSID"},

        {.longname = "container", .argument = &containerStr, .description = "CloudKit container name"},
        {.longname = "radar", .argument = &radarNumber, .description = "Radar number"},

        {.command = "start", .flag = &start, .flagval = true, .description = "Start Octagon state machine"},
        {.command = "sign-in", .flag = &signIn, .flagval = true, .description = "Inform Cuttlefish container of sign in"},
        {.command = "sign-out", .flag = &signOut, .flagval = true, .description = "Inform Cuttlefish container of sign out"},
        {.command = "status", .flag = &status, .flagval = true, .description = "Report Octagon status"},

        {.command = "resetoctagon", .flag = &resetoctagon, .flagval = true, .description = "Reset and establish new Octagon trust"},
        {.command = "resetProtectedData", .flag = &resetProtectedData, .flagval = true, .description = "Reset ProtectedData"},

        {.command = "user-controllable-views", .flag = &userControllableViewsSyncStatus, .flagval = true, .description = "Modify or view user-controllable views status (If one of --enable or --pause is passed, will modify status)"},

        {.command = "allBottles", .flag = &fetchAllBottles, .flagval = true, .description = "Fetch all viable bottles"},
        {.command = "recover", .flag = &recover, .flagval = true, .description = "Recover using this bottle"},
        {.command = "depart", .flag = &depart, .flagval = true, .description = "Depart from Octagon Trust"},

        {.command = "er-trigger", .flag = &er_trigger, .flagval = true, .description = "Trigger an Escrow Request request"},
        {.command = "er-status", .flag = &er_status, .flagval = true, .description = "Report status on any pending Escrow Request requests"},
        {.command = "er-reset", .flag = &er_reset, .flagval = true, .description = "Delete all Escrow Request requests"},
        {.command = "er-store", .flag = &er_store, .flagval = true, .description = "Store any pending Escrow Request prerecords"},

        {.command = "health", .flag = &health, .flagval = true, .description = "Check Octagon Health status"},
        {.command = "ckks-policy", .flag = &ckks_policy_flag, .flagval = true, .description = "Trigger a refetch of the CKKS policy"},

        {.command = "taptoradar", .flag = &ttr_flag, .flagval = true, .description = "Trigger a TapToRadar"},

        {.command = "fetchEscrowRecords", .flag = &fetch_escrow_records, .flagval = true, .description = "Fetch Escrow Records"},
        {.command = "fetchAllEscrowRecords", .flag = &fetch_all_escrow_records, .flagval = true, .description = "Fetch All Escrow Records"},

        {.command = "recover-record", .flag = &recoverRecord, .flagval = true, .description = "Recover record"},
        {.command = "recover-record-silent", .flag = &recoverSilentRecord, .flagval = true, .description = "Silent record recovery"},

        {.command = "reset-account-cdp-contents", .flag = &resetAccountCDPContent, .flagval = true, .description = "Reset an account's CDP contents (escrow records, kvs data, cuttlefish)"},


#if TARGET_OS_WATCH
        {.command = "pairme", .flag = &pairme, .flagval = true, .description = "Perform pairing (watchOS only)"},
#endif /* TARGET_OS_WATCH */
        {}};

    static struct arguments args = {
        .programname = "otctl",
        .description = "Control and report on Octagon Trust",
        .arguments = options,
    };

    if(!options_parse(argc, argv, &args)) {
        printf("\n");
        print_usage(&args);
        return -1;
    }

    @autoreleasepool {
        NSError* error = nil;

        // Use a synchronous control object
        OTControl* rpc = [OTControl controlObject:true error:&error];
        if(error || !rpc) {
            errx(1, "no OTControl failed: %s", [[error description] UTF8String]);
        }

        NSString* context = contextNameArg ? [NSString stringWithCString:contextNameArg encoding:NSUTF8StringEncoding] : OTDefaultContext;
        NSString* container = containerStr ? [NSString stringWithCString:containerStr encoding:NSUTF8StringEncoding] : nil;
        NSString* altDSID = altDSIDArg ? [NSString stringWithCString:altDSIDArg encoding:NSUTF8StringEncoding] : nil;
        NSString* dsid = dsidArg ? [NSString stringWithCString:dsidArg encoding:NSUTF8StringEncoding] : nil;
        NSString* appleID = appleIDArg ? [NSString stringWithCString:appleIDArg encoding:NSUTF8StringEncoding] : nil;

        NSString* skipRateLimitingCheck = skipRateLimitingCheckArg ? [NSString stringWithCString:skipRateLimitingCheckArg encoding:NSUTF8StringEncoding] : @"NO";

        OTControlCLI* ctl = [[OTControlCLI alloc] initWithOTControl:rpc];

        NSError* escrowRequestError = nil;
        EscrowRequestCLI* escrowctl = [[EscrowRequestCLI alloc] initWithEscrowRequest:[SecEscrowRequest request:&escrowRequestError]];
        if(escrowRequestError) {
            errx(1, "SecEscrowRequest failed: %s", [[escrowRequestError description] UTF8String]);
        }
        if(resetoctagon) {
            long ret = [ctl resetOctagon:container context:context altDSID:altDSID];
            return (int)ret;
        }
        if(resetProtectedData) {
            internalOnly();
            long ret = [ctl resetProtectedData:container context:context altDSID:altDSID appleID:appleID dsid:dsid];
            return (int)ret;
        }
        if(userControllableViewsSyncStatus) {
            internalOnly();

            if(argEnable && argPause) {
                print_usage(&args);
                return -1;
            }

            if(argEnable == false && argPause == false) {
                return (int)[ctl fetchUserControllableViewsSyncStatus:container contextID:context];
            }

            // At this point, we're sure that either argEnabled or argPause is set; so the value of argEnabled captures the user's intention
            return (int)[ctl setUserControllableViewsSyncStatus:container contextID:context enabled:argEnable];
        }

        if(fetchAllBottles) {
            return (int)[ctl fetchAllBottles:altDSID containerName:container context:context control:rpc];
        }
        if(recover) {
            NSString* entropyJSON = secretArg ? [NSString stringWithCString:secretArg encoding:NSUTF8StringEncoding] : nil;
            NSString* bottleID = bottleIDArg ? [NSString stringWithCString:bottleIDArg encoding:NSUTF8StringEncoding] : nil;

            if(!entropyJSON || !bottleID) {
                print_usage(&args);
                return -1;
            }

            NSData* entropy = [[NSData alloc] initWithBase64EncodedString:entropyJSON options:0];
            if(!entropy) {
                print_usage(&args);
                return -1;
            }

            return (int)[ctl recoverUsingBottleID:bottleID
                                          entropy:entropy
                                          altDSID:altDSID
                                    containerName:container
                                          context:context
                                          control:rpc];
        }
        if(depart) {
            return (int)[ctl depart:container context:context];
        }
        if(start) {
            internalOnly();
            return (int)[ctl startOctagonStateMachine:container context:context];
        }
        if(signIn) {
            internalOnly();
            return (int)[ctl signIn:altDSID container:container context:context];
        }
        if(signOut) {
            internalOnly();
            return (int)[ctl signOut:container context:context];
        }

        if(status) {
            return (int)[ctl status:container context:context json:json];
        }
        if(fetch_escrow_records) {
            return (int)[ctl fetchEscrowRecords:container context:context];
        }
        if(fetch_all_escrow_records) {
            return (int)[ctl fetchAllEscrowRecords:container context:context];
        }
        if(recoverRecord) {
            NSString* recordIDString = recordID ? [NSString stringWithCString:recordID encoding:NSUTF8StringEncoding] : nil;
            NSString* secret = secretArg ? [NSString stringWithCString:secretArg encoding:NSUTF8StringEncoding] : nil;

            if(!recordIDString || !secret || !appleID) {
                print_usage(&args);
                return -1;
            }

            return (int)[ctl performEscrowRecovery:container context:context recordID:recordIDString appleID:appleID secret:secret];
        }
        if(recoverSilentRecord){
            NSString* secret = secretArg ? [NSString stringWithCString:secretArg encoding:NSUTF8StringEncoding] : nil;

            if(!secret || !appleID) {
                print_usage(&args);
                return -1;
            }

            return (int)[ctl performSilentEscrowRecovery:container context:context appleID:appleID secret:secret];
        }
        if(health) {
            BOOL skip = NO;
            if([skipRateLimitingCheck isEqualToString:@"YES"]) {
                skip = YES;
            } else {
                skip = NO;
            }
            return (int)[ctl healthCheck:container context:context skipRateLimitingCheck:skip];
        }
        if(ckks_policy_flag) {
            return (int)[ctl refetchCKKSPolicy:container context:context];
        }
        if (ttr_flag) {
            if (radarNumber == NULL) {
                radarNumber = "1";
            }
            return (int)[ctl tapToRadar:@"action" description:@"description" radar:[NSString stringWithUTF8String:radarNumber]];
        }
        if(resetAccountCDPContent){
            return (int)[ctl resetAccountCDPContentsWithContainerName:container contextID:context];
        }

        if(er_trigger) {
            internalOnly();
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
            OTPairingInitiateWithCompletion(NULL, ^(bool success, NSError *pairingError) {
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

        print_usage(&args);
        return -1;
    }
    return 0;
}
