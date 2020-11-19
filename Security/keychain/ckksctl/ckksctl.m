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

// Mutual recursion to set up an object for jsonification
static NSDictionary* cleanDictionaryForJSON(NSDictionary* dict);

static id cleanObjectForJSON(id obj) {
    if(!obj) {
        return nil;
    }
    if([obj isKindOfClass:[NSError class]]) {
        NSError* obje = (NSError*) obj;
        NSMutableDictionary* newErrorDict = [@{@"code": @(obje.code), @"domain": obje.domain} mutableCopy];
        newErrorDict[@"userInfo"] = cleanDictionaryForJSON(obje.userInfo);
        return newErrorDict;
    } else if([NSJSONSerialization isValidJSONObject:obj]) {
        return obj;

    } else if ([obj isKindOfClass: [NSNumber class]]) {
        return obj;

    } else if([obj isKindOfClass: [NSData class]]) {
        NSData* dataObj = (NSData*)obj;
        return [dataObj base64EncodedStringWithOptions:0];

    } else if ([obj isKindOfClass: [NSDictionary class]]) {
        return cleanDictionaryForJSON((NSDictionary*) obj);

    } else if ([obj isKindOfClass: [NSArray class]]) {
        NSArray* arrayObj = (NSArray*)obj;
        NSMutableArray* cleanArray = [NSMutableArray arrayWithCapacity:arrayObj.count];

        for(id x in arrayObj) {
            [cleanArray addObject: cleanObjectForJSON(x)];
        }
        return cleanArray;

    } else {
        return [obj description];
    }
}

static NSDictionary* cleanDictionaryForJSON(NSDictionary* dict) {
    if(!dict) {
        return nil;
    }
    NSMutableDictionary* mutDict = [dict mutableCopy];
    for(id key in mutDict.allKeys) {
        id obj = mutDict[key];
        mutDict[key] = cleanObjectForJSON(obj);
    }
    return mutDict;
}

static void print_result(NSDictionary *dict, bool json_flag)
{
    if (json_flag) {
        NSError *err;

        NSData *json = [NSJSONSerialization dataWithJSONObject:cleanDictionaryForJSON(dict)
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

    [self.control rpcResetCloudKit:view reason:@"ckksctl" reply:^(NSError* error){
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

- (void)printHumanReadableStatus:(NSString*)view shortenOutput:(BOOL)shortenOutput {
#if OCTAGON
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [self.control rpcStatus: view reply: ^(NSArray<NSDictionary*>* result, NSError* error) {
        if(error) {
            printf("ERROR FETCHING STATUS: %s\n", [[error description] UTF8String]);
        }

#define pop(d, key, cls) ({ id x = d[key]; d[key] = nil; [x isKindOfClass: [cls class]] ? x : nil; })

        // First result is always global state
        // Ideally, this would come in another parameter, but we can't change the protocol until
        // <rdar://problem/33583242> CKKS: remove PCS's use of CKKSControlProtocol
        NSMutableDictionary* global = [result[0] mutableCopy];
        if(global) {
            NSString* reachability = pop(global, @"reachability", NSString);
            NSString* ckdeviceID = pop(global, @"ckdeviceID", NSString);
            NSString* ckdeviceIDError = pop(global, @"ckdeviceIDError", NSString);
            NSString* lockStateTracker = pop(global,@"lockstatetracker", NSString);
            NSString* retry = pop(global,@"cloudkitRetryAfter", NSString);
            NSDate *lastCKKSPush = pop(global, @"lastCKKSPush", NSDate);
            NSString *syncingPolicy = pop(global, @"policy", NSString);
            NSString *viewsFromPolicy = pop(global, @"viewsFromPolicy", NSString);

            if(!shortenOutput) {
                printf("================================================================================\n\n");
                printf("Global state:\n\n");
            }

            printf("Syncing Policy:       %s\n", [[syncingPolicy description] UTF8String]);
            printf("Views from policy:    %s\n", [[viewsFromPolicy description] UTF8String]);

            if(!shortenOutput) {
                printf("Reachability:         %s\n", [[reachability description] UTF8String]);
                printf("Retry:                %s\n", [[retry description] UTF8String]);
                printf("CK DeviceID:          %s\n", [[ckdeviceID description] UTF8String]);
                printf("CK DeviceID Error:    %s\n", [[ckdeviceIDError description] UTF8String]);
                printf("Lock state:           %s\n", [[lockStateTracker description] UTF8String]);
                printf("Last CKKS push:       %s\n", [[lastCKKSPush description] UTF8String]);
            }

            printf("\n");
        }

        NSArray* remainingViews = result.count > 1 ? [result subarrayWithRange:NSMakeRange(1, result.count-1)] : @[];

        if(remainingViews.count == 0u) {
            printf("No CKKS views are active.\n");
        }

        for(NSDictionary* viewStatus in remainingViews) {
            if(shortenOutput) {
                NSMutableDictionary* status = [viewStatus mutableCopy];

                NSString* viewName = pop(status, @"view", NSString);
                NSString* keystate = pop(status, @"keystate", NSString);

                printf("%-25s: %s\n", [viewName UTF8String], [keystate UTF8String]);
                continue;
            }

            NSMutableDictionary* status = [viewStatus mutableCopy];

            NSString* viewName = pop(status,@"view", NSString);
            NSString* accountStatus = pop(status,@"ckaccountstatus", NSString);
            NSString* accountTracker = pop(status,@"accounttracker", NSString);
            NSString* fetcher = pop(status,@"fetcher", NSString);
            NSString* zoneCreated = pop(status,@"zoneCreated", NSString);
            NSString* zoneSubscribed = pop(status,@"zoneSubscribed", NSString);
            NSString* zoneInitializeScheduler = pop(status,@"zoneInitializeScheduler", NSString);
            NSString* keystate = pop(status,@"keystate", NSString);
            NSString* statusError = pop(status,@"statusError", NSString);
            NSString* itemSyncEnabled = pop(status,@"itemsyncing", NSString);
            NSString* currentTLK =    pop(status,@"currentTLK", NSString);
            NSString* currentClassA = pop(status,@"currentClassA", NSString);
            NSString* currentClassC = pop(status,@"currentClassC", NSString);
            NSString* currentTLKPtr =    pop(status,@"currentTLKPtr", NSString);
            NSString* currentClassAPtr = pop(status,@"currentClassAPtr", NSString);
            NSString* currentClassCPtr = pop(status,@"currentClassCPtr", NSString);
            NSArray* launchSequence = pop(status, @"launchSequence", NSArray);

            NSDictionary* oqe = pop(status,@"oqe", NSDictionary);
            NSDictionary* iqe = pop(status,@"iqe", NSDictionary);
            NSDictionary* keys = pop(status,@"keys", NSDictionary);
            NSDictionary* ckmirror = pop(status,@"ckmirror", NSDictionary);
            NSArray* devicestates = pop(status, @"devicestates", NSArray);
            NSArray* tlkshares = pop(status, @"tlkshares", NSArray);

            NSString* lastIncomingQueueOperation          = pop(status,@"lastIncomingQueueOperation", NSString);
            NSString* lastNewTLKOperation                 = pop(status,@"lastNewTLKOperation", NSString);
            NSString* lastOutgoingQueueOperation          = pop(status,@"lastOutgoingQueueOperation", NSString);
            NSString* lastProcessReceivedKeysOperation    = pop(status,@"lastProcessReceivedKeysOperation", NSString);
            NSString* lastReencryptOutgoingItemsOperation = pop(status,@"lastReencryptOutgoingItemsOperation", NSString);

            printf("================================================================================\n\n");

            printf("View: %s\n\n", [viewName UTF8String]);

            if(statusError != nil) {
                printf("ERROR FETCHING STATUS: %s\n\n", [statusError UTF8String]);
            }

            printf("CloudKit account:     %s\n", [accountStatus UTF8String]);
            printf("Account tracker:      %s\n", [accountTracker UTF8String]);

            if(!([zoneCreated isEqualToString:@"yes"] && [zoneSubscribed isEqualToString:@"yes"])) {
                printf("CK Zone Created:            %s\n", [[zoneCreated description] UTF8String]);
                printf("CK Zone Subscribed:         %s\n", [[zoneSubscribed description] UTF8String]);
                printf("CK Zone initialize retry:   %s\n", [[zoneInitializeScheduler description] UTF8String]);
                printf("\n");
            }

            printf("Key state:            %s\n", [keystate UTF8String]);
            printf("Current TLK:          %s\n", currentTLK != nil
                   ? [currentTLK    UTF8String]
                   : [[NSString stringWithFormat:@"missing; pointer is %@", currentTLKPtr] UTF8String]);
            printf("Current ClassA:       %s\n", currentClassA != nil
                   ? [currentClassA UTF8String]
                   : [[NSString stringWithFormat:@"missing; pointer is %@", currentClassAPtr] UTF8String]);
            printf("Current ClassC:       %s\n", currentClassC != nil
                   ? [currentClassC UTF8String]
                   : [[NSString stringWithFormat:@"missing; pointer is %@", currentClassCPtr] UTF8String]);

            printf("TLK shares:           %s\n", [[tlkshares description] UTF8String]);

            printf("Item syncing:          %s\n", [[itemSyncEnabled description] UTF8String]);
            printf("Outgoing Queue counts: %s\n", [[oqe description] UTF8String]);
            printf("Incoming Queue counts: %s\n", [[iqe description] UTF8String]);
            printf("Key counts: %s\n", [[keys description] UTF8String]);

            printf("Item counts (by key):  %s\n", [[ckmirror description] UTF8String]);
            printf("Peer states:           %s\n", [[devicestates description] UTF8String]);

            printf("zone change fetcher:                 %s\n", [[fetcher description] UTF8String]);
            printf("lastIncomingQueueOperation:          %s\n", lastIncomingQueueOperation          == nil ? "never" : [lastIncomingQueueOperation          UTF8String]);
            printf("lastNewTLKOperation:                 %s\n", lastNewTLKOperation                 == nil ? "never" : [lastNewTLKOperation                 UTF8String]);
            printf("lastOutgoingQueueOperation:          %s\n", lastOutgoingQueueOperation          == nil ? "never" : [lastOutgoingQueueOperation          UTF8String]);
            printf("lastProcessReceivedKeysOperation:    %s\n", lastProcessReceivedKeysOperation    == nil ? "never" : [lastProcessReceivedKeysOperation    UTF8String]);
            printf("lastReencryptOutgoingItemsOperation: %s\n", lastReencryptOutgoingItemsOperation == nil ? "never" : [lastReencryptOutgoingItemsOperation UTF8String]);

            printf("Launch sequence:\n");
            for (NSString *event in launchSequence) {
                printf("\t%s\n", [[event description] UTF8String]);
            }

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

    // The maximum device-side delay to start a fetch is 120s, so we must wait longer than that for a response.
    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 135)) != 0) {
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

- (long)ckmetric {
    __block long ret = 0;
#if OCTAGON
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [self.control rpcCKMetric:@"testMetric" attributes:@{ @"testAttribute" : @"value" } reply:^(NSError* error) {
        if(error) {
            printf("Error sending metric: %s\n", [[error description] UTF8String]);
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
static int shortOutput = false;
static int ckmetric = false;

static char* viewArg = NULL;

int main(int argc, char **argv)
{
    static struct argument options[] = {
        { .shortname='p', .longname="perfcounters", .flag=&perfCounters, .flagval=true, .description="Print CKKS performance counters"},
        { .shortname='j', .longname="json", .flag=&json, .flagval=true, .description="Output in JSON format"},
        { .shortname='s', .longname="short", .flag=&shortOutput, .flagval=true, .description="Output a short format"},
        { .shortname='v', .longname="view", .argument=&viewArg, .description="Operate on a single view"},

        { .command="status", .flag=&status, .flagval=true, .description="Report status on CKKS views"},
        { .command="fetch", .flag=&fetch, .flagval=true, .description="Fetch all new changes in CloudKit and attempt to process them"},
        { .command="push", .flag=&push, .flagval=true, .description="Push all pending local changes to CloudKit"},
        { .command="resync", .flag=&resync, .flagval=true, .description="Resync all data with what's in CloudKit"},
        { .command="reset", .flag=&reset, .flagval=true, .description="All local data will be wiped, and data refetched from CloudKit"},
        { .command="reset-cloudkit", .flag=&resetCloudKit, .flagval=true, .description="All data in CloudKit will be removed and replaced with what's local"},
        { .command="ckmetric", .flag=&ckmetric, .flagval=true, .description="Push CloudKit metric"},
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

        CKKSControl* rpc = [CKKSControl CKKSControlObject:false error:&error];
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
                [ctl printHumanReadableStatus:view shortenOutput:shortOutput];
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
        } else if(ckmetric) {
            return (int)[ctl ckmetric];
        } else {
            print_usage(&args);
            return -1;
        }
    }
    return 0;
}
