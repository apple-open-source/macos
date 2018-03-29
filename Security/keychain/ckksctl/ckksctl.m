//
//  Security
//

#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <Security/Security.h>
#import <Security/SecItemPriv.h>
#import <xpc/xpc.h>
#import <err.h>

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSControl.h"

#include "lib/SecArgParse.h"

static void nsprintf(NSString *fmt, ...) NS_FORMAT_FUNCTION(1, 2);
static void print_result(NSDictionary *dict, bool json_flag);
static void print_dict(NSDictionary *dict, int ind);
static void print_array(NSArray *array, int ind);
static void print_entry(id k, id v, int ind);

static void nsprintf(NSString *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    NSString *str = [[NSString alloc] initWithFormat:fmt arguments:ap];
    va_end(ap);

    puts([str UTF8String]);
#if !__has_feature(objc_arc)
    [str release];
#endif
}

static NSDictionary* flattenNSErrorsInDictionary(NSDictionary* dict) {
    if(!dict) {
        return nil;
    }
    NSMutableDictionary* mutDict = [dict mutableCopy];
    for(id key in mutDict.allKeys) {
        id obj = mutDict[key];
        if([obj isKindOfClass:[NSError class]]) {
            NSError* obje = (NSError*) obj;
            NSMutableDictionary* newErrorDict = [@{@"code": @(obje.code), @"domain": obje.domain} mutableCopy];
            newErrorDict[@"userInfo"] = flattenNSErrorsInDictionary(obje.userInfo);
            mutDict[key] = newErrorDict;
        } else if(![NSJSONSerialization isValidJSONObject:obj]) {
            mutDict[key] = [obj description];
        }
    }
    return mutDict;
}

static void print_result(NSDictionary *dict, bool json_flag)
{
    if (json_flag) {
        NSError *err;

        // NSErrors don't know how to JSON-ify themselves, for some reason
        // This will flatten a single layer of them
        if(![NSJSONSerialization isValidJSONObject:dict]) {
            dict = flattenNSErrorsInDictionary(dict);
        }

        if(![NSJSONSerialization isValidJSONObject:dict]) {
            printf("Still unsure how to JSONify the following object:\n");
            print_dict(dict, 0);
        }

        NSData *json = [NSJSONSerialization dataWithJSONObject:dict
                                                       options:(NSJSONWritingPrettyPrinted | NSJSONWritingSortedKeys)
                                                         error:&err];
        if (!json) {
            NSLog(@"error: %@", err.localizedDescription);
        } else {
            printf("%s", [[[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding] UTF8String]);
        }
    } else {
        print_dict(dict, 0);
    }
}

static void print_dict(NSDictionary *dict, int ind)
{
    NSArray *sortedKeys = [[dict allKeys] sortedArrayUsingSelector:@selector(localizedCaseInsensitiveCompare:)];
    for (id k in sortedKeys) {
        id v = dict[k];
        print_entry(k, v, ind);
    }
}

static void print_array(NSArray *array, int ind)
{
    [array enumerateObjectsUsingBlock:^(id v, NSUInteger i, BOOL *stop __unused) {
        print_entry(@(i), v, ind);
    }];
}

static void print_entry(id k, id v, int ind)
{
    if ([v isKindOfClass:[NSDictionary class]]) {
        if (ind == 0) {
            nsprintf(@"\n%*s%@ -", ind * 4, "", k);
            nsprintf(@"%*s========================", ind * 4, "");
        } else if (ind == 1) {
            nsprintf(@"\n%*s%@ -", ind * 4, "", k);
            nsprintf(@"%*s------------------------", ind * 4, "");
        } else {
            nsprintf(@"%*s%@ -", ind * 4, "", k);
        }

        print_dict(v, ind + 1);
    } else if ([v isKindOfClass:[NSArray class]]) {
        nsprintf(@"%*s%@ -", ind * 4, "", k);
        print_array(v, ind + 1);
    } else {
        nsprintf(@"%*s%@: %@", ind * 4, "", k, v);
    }
}

@interface CKKSControlCLI : NSObject
@property CKKSControl* control;
@end

@implementation CKKSControlCLI

- (instancetype) initWithCKKSControl:(CKKSControl*)control {
    if ((self = [super init])) {
        _control = control;
    }

    return self;
}

- (NSDictionary<NSString *, id> *)fetchPerformanceCounters
{
    NSMutableDictionary *perfDict = [[NSMutableDictionary alloc] init];
#if OCTAGON
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [self.control rpcPerformanceCounters:^(NSDictionary<NSString *,NSNumber *> * counters, NSError * error) {
        if(error) {
            perfDict[@"error"] = [error description];
        }

        [counters enumerateKeysAndObjectsUsingBlock:^(NSString * key, NSNumber * obj, BOOL *stop) {
            perfDict[key] = obj;
        }];

        dispatch_semaphore_signal(sema);
    }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60)) != 0) {
        perfDict[@"error"] = @"timed out waiting for response";
    }
#endif

    return perfDict;
}

- (long)resetLocal:(NSString*)view {
    __block long ret = 0;
#if OCTAGON
    printf("Beginning local reset for %s...\n", view ? [[view description] UTF8String] : "all zones");
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [self.control rpcResetLocal:view
                          reply:^(NSError *error) {
                              if(error == NULL) {
                                  printf("reset complete.\n");
                                  ret = 0;
                              } else {
                                  nsprintf(@"reset error: %@\n", error);
                                  ret = error.code;
                              }
                              dispatch_semaphore_signal(sema);
                          }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60 * 2)) != 0) {
        printf("\n\nError: timed out waiting for response\n");
        return -1;
    }
#endif // OCTAGON
    return ret;
}

- (long)resetCloudKit:(NSString*)view {
    __block long ret = 0;
#if OCTAGON
    printf("Beginning CloudKit reset for %s...\n", view ? [[view description] UTF8String] : "all zones");
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [self.control rpcResetCloudKit:view reply:^(NSError* error){
        if(error == NULL) {
            printf("CloudKit Reset complete.\n");
            ret = 0;
        } else {
            nsprintf(@"Reset error: %@\n", error);
            ret = error.code;
        }
        dispatch_semaphore_signal(sema);
    }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60 * 5)) != 0) {
        printf("\n\nError: timed out waiting for response\n");
        return -1;
    }
#endif // OCTAON
    return ret;
}

- (long)resync:(NSString*)view {
    __block long ret = 0;
#if OCTAGON
    printf("Beginning resync for %s...\n", view ? [[view description] UTF8String] : "all zones");
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [self.control rpcResync:view reply:^(NSError* error){
        if(error == NULL) {
            printf("resync success.\n");
            ret = 0;
        } else {
            nsprintf(@"resync errored: %@\n", error);
            ret = error.code;
        }
        dispatch_semaphore_signal(sema);
    }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60 * 2)) != 0) {
        printf("\n\nError: timed out waiting for response\n");
        return -1;
    }
#endif // OCTAGON
    return ret;
}

- (NSDictionary<NSString *, id> *)fetchStatus: (NSString*) view {
    NSMutableDictionary *status = [[NSMutableDictionary alloc] init];
#if OCTAGON
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [self.control rpcStatus: view reply: ^(NSArray<NSDictionary*>* result, NSError* error) {
        if(error) {
            status[@"error"] = [error description];
        }

        if(result.count <= 1u) {
            printf("No CKKS views are active.\n");
        }


        for(NSDictionary* viewStatus in result) {
            status[viewStatus[@"view"]] = viewStatus;
        }
        dispatch_semaphore_signal(sema);
    }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 30)) != 0) {
        status[@"error"] = @"timed out";
    }
#endif // OCTAGON
    return status;
}

- (void)printHumanReadableStatus: (NSString*) view {
#if OCTAGON
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [self.control rpcStatus: view reply: ^(NSArray<NSDictionary*>* result, NSError* error) {
        if(error) {
            printf("ERROR FETCHING STATUS: %s\n", [[error description] UTF8String]);
        }

#define pop(d, key) ({ id x = d[key]; d[key] = nil; x; })

        // First result is always global state
        // Ideally, this would come in another parameter, but we can't change the protocol until
        // <rdar://problem/33583242> CKKS: remove PCS's use of CKKSControlProtocol
        NSMutableDictionary* global = [result[0] mutableCopy];
        if(global) {
            NSString* selfPeers = pop(global, @"selfPeers");
            NSString* selfPeersError = pop(global, @"selfPeersError");
            NSArray* trustedPeers = pop(global, @"trustedPeers");
            NSString* trustedPeersError = pop(global, @"trustedPeersError");
            NSString* reachability = pop(global, @"reachability");
            NSString* ckdeviceID = pop(global, @"ckdeviceID");
            NSString* ckdeviceIDError = pop(global, @"ckdeviceIDError");

            printf("================================================================================\n\n");
            printf("Global state:\n\n");
            printf("Current self:         %s\n", [[selfPeers description] UTF8String]);
            if(![selfPeersError isEqual: [NSNull null]]) {
                printf("Self Peers Error:     %s\n", [[selfPeersError description] UTF8String]);
            }
            printf("Trusted peers:        %s\n", [[trustedPeers description] UTF8String]);
            if(![trustedPeersError isEqual: [NSNull null]]) {
                printf("Trusted Peers Error:  %s\n", [[trustedPeersError description] UTF8String]);
            }
            printf("Reachability:         %s\n", [[reachability description] UTF8String]);
            printf("CK DeviceID:          %s\n", [[ckdeviceID description] UTF8String]);
            printf("CK DeviceID Error:    %s\n", [[ckdeviceIDError description] UTF8String]);

            printf("\n");
        }

        NSArray* remainingViews = result.count > 1 ? [result subarrayWithRange:NSMakeRange(1, result.count-1)] : @[];

        if(remainingViews.count == 0u) {
            printf("No CKKS views are active.\n");
        }

        for(NSDictionary* viewStatus in remainingViews) {
            NSMutableDictionary* status = [viewStatus mutableCopy];

            NSString* viewName = pop(status,@"view");
            NSString* accountStatus = pop(status,@"ckaccountstatus");
            NSString* lockStateTracker = pop(status,@"lockstatetracker");
            NSString* accountTracker = pop(status,@"accounttracker");
            NSString* fetcher = pop(status,@"fetcher");
            NSString* zoneCreated = pop(status,@"zoneCreated");
            NSString* zoneCreatedError = pop(status,@"zoneCreatedError");
            NSString* zoneSubscribed = pop(status,@"zoneSubscribed");
            NSString* zoneSubscribedError = pop(status,@"zoneSubscribedError");
            NSString* zoneInitializeScheduler = pop(status,@"zoneInitializeScheduler");
            NSString* keystate = pop(status,@"keystate");
            NSString* keyStateError = pop(status,@"keyStateError");
            NSString* statusError = pop(status,@"statusError");
            NSString* currentTLK =    pop(status,@"currentTLK");
            NSString* currentClassA = pop(status,@"currentClassA");
            NSString* currentClassC = pop(status,@"currentClassC");
            NSString* currentTLKPtr =     pop(status,@"currentTLKPtr");
            NSString* currentClassAPtr = pop(status,@"currentClassAPtr");
            NSString* currentClassCPtr = pop(status,@"currentClassCPtr");
            NSString* currentManifestGeneration = pop(status,@"currentManifestGen");

            NSDictionary* oqe = pop(status,@"oqe");
            NSDictionary* iqe = pop(status,@"iqe");
            NSDictionary* keys = pop(status,@"keys");
            NSDictionary* ckmirror = pop(status,@"ckmirror");
            NSArray* devicestates = pop(status, @"devicestates");
            NSArray* tlkshares = pop(status, @"tlkshares");


            NSString* zoneSetupOperation                  = pop(status,@"zoneSetupOperation");
            NSString* keyStateOperation                   = pop(status,@"keyStateOperation");
            NSString* lastIncomingQueueOperation          = pop(status,@"lastIncomingQueueOperation");
            NSString* lastNewTLKOperation                 = pop(status,@"lastNewTLKOperation");
            NSString* lastOutgoingQueueOperation          = pop(status,@"lastOutgoingQueueOperation");
            NSString* lastRecordZoneChangesOperation      = pop(status,@"lastRecordZoneChangesOperation");
            NSString* lastProcessReceivedKeysOperation    = pop(status,@"lastProcessReceivedKeysOperation");
            NSString* lastReencryptOutgoingItemsOperation = pop(status,@"lastReencryptOutgoingItemsOperation");
            NSString* lastScanLocalItemsOperation         = pop(status,@"lastScanLocalItemsOperation");

            printf("================================================================================\n\n");

            printf("View: %s\n\n", [viewName UTF8String]);

            if(![statusError isEqual: [NSNull null]]) {
                printf("ERROR FETCHING STATUS: %s\n\n", [statusError UTF8String]);
            }

            printf("CloudKit account:     %s\n", [accountStatus UTF8String]);
            printf("Account tracker:      %s\n", [accountTracker UTF8String]);

            if(!([zoneCreated isEqualToString:@"yes"] && [zoneSubscribed isEqualToString:@"yes"])) {
                printf("CK Zone Created:            %s\n", [[zoneCreated description] UTF8String]);
                printf("CK Zone Created error:      %s\n", [[zoneCreatedError description] UTF8String]);

                printf("CK Zone Subscribed:         %s\n", [[zoneSubscribed description] UTF8String]);
                printf("CK Zone Subscription error: %s\n", [[zoneSubscribedError description] UTF8String]);
                printf("CK Zone initialize retry:   %s\n", [[zoneInitializeScheduler description] UTF8String]);
                printf("\n");
            }

            printf("Key state:            %s\n", [keystate UTF8String]);
            if(![keyStateError isEqual: [NSNull null]]) {
                printf("Key State Error: %s\n", [keyStateError UTF8String]);
            }
            printf("Lock state:           %s\n", [lockStateTracker UTF8String]);

            printf("Current TLK:          %s\n", ![currentTLK    isEqual: [NSNull null]]
                   ? [currentTLK    UTF8String]
                   : [[NSString stringWithFormat:@"missing; pointer is %@", currentTLKPtr] UTF8String]);
            printf("Current ClassA:       %s\n", ![currentClassA isEqual: [NSNull null]]
                   ? [currentClassA UTF8String]
                   : [[NSString stringWithFormat:@"missing; pointer is %@", currentClassAPtr] UTF8String]);
            printf("Current ClassC:       %s\n", ![currentClassC isEqual: [NSNull null]]
                   ? [currentClassC UTF8String]
                   : [[NSString stringWithFormat:@"missing; pointer is %@", currentClassCPtr] UTF8String]);

            printf("TLK shares:           %s\n", [[tlkshares description] UTF8String]);

            printf("Outgoing Queue counts: %s\n", [[oqe description] UTF8String]);
            printf("Incoming Queue counts: %s\n", [[iqe description] UTF8String]);
            printf("Key counts: %s\n", [[keys description] UTF8String]);
            printf("latest manifest generation: %s\n", [currentManifestGeneration isEqual:[NSNull null]] ? "null" : currentManifestGeneration.UTF8String);

            printf("Item counts (by key):  %s\n", [[ckmirror description] UTF8String]);
            printf("Peer states:           %s\n", [[devicestates description] UTF8String]);

            printf("zone change fetcher:                 %s\n", [[fetcher description] UTF8String]);
            printf("zoneSetupOperation:                  %s\n", [zoneSetupOperation                  isEqual: [NSNull null]] ? "never" : [zoneSetupOperation                  UTF8String]);
            printf("keyStateOperation:                   %s\n", [keyStateOperation                   isEqual: [NSNull null]] ? "never" : [keyStateOperation                   UTF8String]);
            printf("lastIncomingQueueOperation:          %s\n", [lastIncomingQueueOperation          isEqual: [NSNull null]] ? "never" : [lastIncomingQueueOperation          UTF8String]);
            printf("lastNewTLKOperation:                 %s\n", [lastNewTLKOperation                 isEqual: [NSNull null]] ? "never" : [lastNewTLKOperation                 UTF8String]);
            printf("lastOutgoingQueueOperation:          %s\n", [lastOutgoingQueueOperation          isEqual: [NSNull null]] ? "never" : [lastOutgoingQueueOperation          UTF8String]);
            printf("lastRecordZoneChangesOperation:      %s\n", [lastRecordZoneChangesOperation      isEqual: [NSNull null]] ? "never" : [lastRecordZoneChangesOperation      UTF8String]);
            printf("lastProcessReceivedKeysOperation:    %s\n", [lastProcessReceivedKeysOperation    isEqual: [NSNull null]] ? "never" : [lastProcessReceivedKeysOperation    UTF8String]);
            printf("lastReencryptOutgoingItemsOperation: %s\n", [lastReencryptOutgoingItemsOperation isEqual: [NSNull null]] ? "never" : [lastReencryptOutgoingItemsOperation UTF8String]);
            printf("lastScanLocalItemsOperation:         %s\n", [lastScanLocalItemsOperation         isEqual: [NSNull null]] ? "never" : [lastScanLocalItemsOperation         UTF8String]);

            if(status.allKeys.count > 0u) {
                printf("\nExtra information: %s\n", [[status description] UTF8String]);
            }
            printf("\n");
        }
        dispatch_semaphore_signal(sema);
    }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 30)) != 0) {
        printf("\n\nError: timed out waiting for response\n");
    }
#endif // OCTAGON
}

- (long)fetch:(NSString*)view {
    __block long ret = 0;
#if OCTAGON
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [self.control rpcFetchAndProcessChanges:view reply:^(NSError* error) {
        if(error) {
            printf("Error fetching: %s\n", [[error description] UTF8String]);
            ret = (error.code == 0 ? -1 : error.code);
        } else {
            printf("Complete.\n");
            ret = 0;
        }

        dispatch_semaphore_signal(sema);
    }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 65)) != 0) {
        printf("\n\nError: timed out waiting for response\n");
        return -1;
    }
#endif // OCTAGON
    return ret;
}

- (long)push:(NSString*)view {
    __block long ret = 0;
#if OCTAGON
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [self.control rpcPushOutgoingChanges:view reply:^(NSError* error) {
        if(error) {
            printf("Error pushing: %s\n", [[error description] UTF8String]);
            ret = (error.code == 0 ? -1 : error.code);
        } else {
            printf("Complete.\n");
            ret = 0;
        }

        dispatch_semaphore_signal(sema);
    }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 65)) != 0) {
        printf("\n\nError: timed out waiting for response\n");
        return -1;
    }
#endif // OCTAGON
    return ret;
}

@end


static int perfCounters = false;
static int status = false;
static int resync = false;
static int reset = false;
static int resetCloudKit = false;
static int fetch = false;
static int push = false;
static int json = false;

static char* viewArg = NULL;

int main(int argc, char **argv)
{
    static struct argument options[] = {
        { .shortname='p', .longname="perfcounters", .flag=&perfCounters, .flagval=true, .description="Print CKKS performance counters"},
        { .shortname='j', .longname="json", .flag=&json, .flagval=true, .description="Output in JSON format"},
        { .shortname='v', .longname="view", .argument=&viewArg, .description="Operate on a single view"},

        { .command="status", .flag=&status, .flagval=true, .description="Report status on CKKS views"},
        { .command="fetch", .flag=&fetch, .flagval=true, .description="Fetch all new changes in CloudKit and attempt to process them"},
        { .command="push", .flag=&push, .flagval=true, .description="Push all pending local changes to CloudKit"},
        { .command="resync", .flag=&resync, .flagval=true, .description="Resync all data with what's in CloudKit"},
        { .command="reset", .flag=&reset, .flagval=true, .description="All local data will be wiped, and data refetched from CloudKit"},
        { .command="reset-cloudkit", .flag=&resetCloudKit, .flagval=true, .description="All data in CloudKit will be removed and replaced with what's local"},
        {}
    };

    static struct arguments args = {
        .programname="ckksctl",
        .description="Control and report on CKKS",
        .arguments = options,
    };

    if(!options_parse(argc, argv, &args)) {
        printf("\n");
        print_usage(&args);
        return -1;
    }

    @autoreleasepool {
        NSError* error = nil;

        CKKSControl* rpc = [CKKSControl controlObject:&error];
        if(error || !rpc) {
            errx(1, "no CKKSControl failed: %s", [[error description] UTF8String]);
        }

        CKKSControlCLI* ctl = [[CKKSControlCLI alloc] initWithCKKSControl:rpc];

        NSString* view = viewArg ? [NSString stringWithCString: viewArg encoding: NSUTF8StringEncoding] : nil;

        if(status) {
            // Complicated logic, but you can choose any combination of (json, perfcounters) that you like.
            NSMutableDictionary *statusDict = [[NSMutableDictionary alloc] init];
            if(perfCounters) {
                statusDict[@"performance"] = [ctl fetchPerformanceCounters];
            }
            if (json) {
                statusDict[@"status"] = [ctl fetchStatus:view];
            }
            if(json || perfCounters) {
               print_result(statusDict, true);
                printf("\n");
            }

            if(!json) {
                [ctl printHumanReadableStatus:view];
            }
            return 0;
        } else if(perfCounters) {
            NSMutableDictionary *statusDict = [[NSMutableDictionary alloc] init];
            statusDict[@"performance"] = [ctl fetchPerformanceCounters];
            print_result(statusDict, false);

        } else if(fetch) {
            return (int)[ctl fetch:view];
        } else if(push) {
            return (int)[ctl push:view];
        } else if(reset) {
            return (int)[ctl resetLocal:view];
        } else if(resetCloudKit) {
            return (int)[ctl resetCloudKit:view];
        } else if(resync) {
            return (int)[ctl resync:view];
        } else {
            print_usage(&args);
            return -1;
        }
    }
    return 0;
}
