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
#import "keychain/ckks/CKKSControlProtocol.h"
#import "ckksctl.h"

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

static void print_result(NSDictionary *dict, bool json_flag)
{
    if (json_flag) {
        NSError *err;
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

@interface CKKSControl ()
@property NSXPCConnection *connection;
@end

@implementation CKKSControl

- (instancetype) initWithEndpoint:(xpc_endpoint_t)endpoint
{
    if ((self = [super init]) == NULL)
        return NULL;

    NSXPCInterface *interface = CKKSSetupControlProtocol([NSXPCInterface interfaceWithProtocol:@protocol(CKKSControlProtocol)]);
    NSXPCListenerEndpoint *listenerEndpoint = [[NSXPCListenerEndpoint alloc] init];

    [listenerEndpoint _setEndpoint:endpoint];

    self.connection = [[NSXPCConnection alloc] initWithListenerEndpoint:listenerEndpoint];
    if (self.connection == NULL)
        return NULL;

    self.connection.remoteObjectInterface = interface;

    [self.connection resume];


    return self;
}

- (NSDictionary<NSString *, id> *)printPerformanceCounters
{
    NSMutableDictionary *perfDict = [[NSMutableDictionary alloc] init];
#if OCTAGON
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [[self.connection remoteObjectProxyWithErrorHandler: ^(NSError* error) {
        perfDict[@"error"] = [error description];
        dispatch_semaphore_signal(sema);

    }] performanceCounters:^(NSDictionary <NSString *, NSNumber *> *counters){
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

- (void)resetLocal: (NSString*)view {
#if OCTAGON
    printf("Beginning local reset for %s...\n", view ? [[view description] UTF8String] : "all zones");
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [[self.connection remoteObjectProxyWithErrorHandler: ^(NSError* error) {
        printf("\n\nError talking with daemon: %s\n", [[error description] UTF8String]);
        dispatch_semaphore_signal(sema);

    }]  rpcResetLocal:view reply:^(NSError* result){
        if(result == NULL) {
            printf("reset complete.\n");
        } else {
            printf("reset error: %s\n", [[result description] UTF8String]);
        }
        dispatch_semaphore_signal(sema);
    }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60)) != 0) {
        printf("\n\nError: timed out waiting for response\n");
    }
#endif // OCTAGON
}

- (void)resetCloudKit: (NSString*)view {
#if OCTAGON
    printf("Beginning CloudKit reset for %s...\n", view ? [[view description] UTF8String] : "all zones");
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [[self.connection remoteObjectProxyWithErrorHandler: ^(NSError* error) {
        printf("\n\nError talking with daemon: %s\n", [[error description] UTF8String]);
        dispatch_semaphore_signal(sema);

    }]  rpcResetCloudKit:view reply:^(NSError* result){
        if(result == NULL) {
            printf("CloudKit Reset complete.\n");
        } else {
            printf("Reset error: %s\n", [[result description] UTF8String]);
        }
        dispatch_semaphore_signal(sema);
    }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60)) != 0) {
        printf("\n\nError: timed out waiting for response\n");
    }
#endif // OCTAON
}

- (void)resync: (NSString*)view {
#if OCTAGON
    printf("Beginning resync for %s...\n", view ? [[view description] UTF8String] : "all zones");
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [[self.connection remoteObjectProxyWithErrorHandler: ^(NSError* error) {
        printf("\n\nError talking with daemon: %s\n", [[error description] UTF8String]);
        dispatch_semaphore_signal(sema);

    }]  rpcResync:view reply:^(NSError* result){
        if(result == NULL) {
            printf("resync success.\n");
        } else {
            printf("resync errored: %s\n", [[result description] UTF8String]);
        }
        dispatch_semaphore_signal(sema);
    }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60)) != 0) {
        printf("\n\nError: timed out waiting for response\n");
    }
#endif // OCTAGON
}

- (void)getAnalyticsSysdiagnose
{
    printf("Getting analytics sysdiagnose....\n");
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError* error) {
        printf("\n\nError talking with daemon: %s\n", [[error description] UTF8String]);
        dispatch_semaphore_signal(sema);
    }] rpcGetAnalyticsSysdiagnoseWithReply:^(NSString* sysdiagnose, NSError* error) {
        if (sysdiagnose && !error) {
            nsprintf(@"Analytics sysdiagnose:\n\n%@", sysdiagnose);
        }
        else {
            nsprintf(@"error retrieving sysdiagnose: %@", error);
        }

        dispatch_semaphore_signal(sema);
    }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60)) != 0) {
        printf("\n\nError: timed out waiting for response\n");
    }
}

- (void)getAnalyticsJSON
{
    printf("Getting analytics json....\n");
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    
    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError* error) {
        printf("\n\nError talking with daemon: %s\n", [[error description] UTF8String]);
        dispatch_semaphore_signal(sema);
    }] rpcGetAnalyticsJSONWithReply:^(NSData* json, NSError* error) {
        if (json && !error) {
            nsprintf(@"Analytics JSON:\n\n%@", [[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding]);
        }
        else {
            nsprintf(@"error retrieving JSON: %@", error);
        }
        
        dispatch_semaphore_signal(sema);
    }];
    
    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60)) != 0) {
        printf("\n\nError: timed out waiting for response\n");
    }
}

- (void)forceAnalyticsUpload
{
    printf("Uploading....\n");
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    
    [[self.connection remoteObjectProxyWithErrorHandler:^(NSError* error) {
        printf("\n\nError talking with daemon: %s\n", [[error description] UTF8String]);
        dispatch_semaphore_signal(sema);
    }] rpcForceUploadAnalyticsWithReply:^(BOOL success, NSError* error) {
        if (success) {
            nsprintf(@"successfully uploaded analytics data");
        }
        else {
            nsprintf(@"error uploading analytics: %@", error);
        }
        
        dispatch_semaphore_signal(sema);
    }];
    
    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 60)) != 0) {
        printf("\n\nError: timed out waiting for response\n");
    }
}

- (NSDictionary<NSString *, id> *)status: (NSString*) view {
    NSMutableDictionary *status = [[NSMutableDictionary alloc] init];
#if OCTAGON
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [[self.connection remoteObjectProxyWithErrorHandler: ^(NSError* error) {
        status[@"error"] = [error description];
        dispatch_semaphore_signal(sema);

    }]  rpcStatus: view reply: ^(NSArray<NSDictionary*>* result, NSError* error) {
        if(error) {
            status[@"error"] = [error description];
        }

        if(result.count == 0u) {
            printf("No CKKS views are active.\n");
        }


        for(NSDictionary* viewStatus in result) {
            status[viewStatus[@"view"]] = viewStatus;
        }
        dispatch_semaphore_signal(sema);
    }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 5)) != 0) {
        status[@"error"] = @"timed out";
    }
#endif // OCTAGON
    return status;
}

- (void)status_custom: (NSString*) view {
#if OCTAGON
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [[self.connection remoteObjectProxyWithErrorHandler: ^(NSError* error) {
        printf("\n\nError talking with daemon: %s\n", [[error description] UTF8String]);
        dispatch_semaphore_signal(sema);

    }]  rpcStatus: view reply: ^(NSArray<NSDictionary*>* result, NSError* error) {
        if(error) {
            printf("ERROR FETCHING STATUS: %s\n", [[error description] UTF8String]);
        }

        if(result.count == 0u) {
            printf("No CKKS views are active.\n");
        }

        for(NSDictionary* viewStatus in result) {
            NSMutableDictionary* status = [viewStatus mutableCopy];

    #define pop(d, key) ({ id x = d[key]; d[key] = nil; x; })

            NSString* viewName = pop(status,@"view");
            NSString* accountStatus = pop(status,@"ckaccountstatus");
            NSString* lockStateTracker = pop(status,@"lockstatetracker");
            NSString* accountTracker = pop(status,@"accounttracker");
            NSString* fetcher = pop(status,@"fetcher");
            NSString* setup = pop(status,@"setup");
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
            NSString* currentManifestGeneration = pop(status,@"currentManifestGen");

            NSDictionary* oqe = pop(status,@"oqe");
            NSDictionary* iqe = pop(status,@"iqe");
            NSDictionary* keys = pop(status,@"keys");
            NSDictionary* ckmirror = pop(status,@"ckmirror");
            NSArray* devicestates = pop(status, @"devicestates");

            NSString* zoneSetupOperation                  = pop(status,@"zoneSetupOperation");
            NSString* viewSetupOperation                  = pop(status,@"viewSetupOperation");
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
            printf("Ran setup operation:  %s\n", [setup UTF8String]);

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

            printf("Current TLK:          %s\n", [currentTLK    isEqual: [NSNull null]] ? "null" : [currentTLK    UTF8String]);
            printf("Current ClassA:       %s\n", [currentClassA isEqual: [NSNull null]] ? "null" : [currentClassA UTF8String]);
            printf("Current ClassC:       %s\n", [currentClassC isEqual: [NSNull null]] ? "null" : [currentClassC UTF8String]);

            printf("Outgoing Queue counts: %s\n", [[oqe description] UTF8String]);
            printf("Incoming Queue counts: %s\n", [[iqe description] UTF8String]);
            printf("Key counts: %s\n", [[keys description] UTF8String]);
            printf("latest manifest generation: %s\n", [currentManifestGeneration isEqual:[NSNull null]] ? "null" : currentManifestGeneration.UTF8String);

            printf("Item counts (by key):  %s\n", [[ckmirror description] UTF8String]);
            printf("Peer states:           %s\n", [[devicestates description] UTF8String]);

            printf("zone change fetcher:                 %s\n", [[fetcher description] UTF8String]);
            printf("zoneSetupOperation:                  %s\n", [zoneSetupOperation                  isEqual: [NSNull null]] ? "never" : [zoneSetupOperation                  UTF8String]);
            printf("viewSetupOperation:                  %s\n", [viewSetupOperation                  isEqual: [NSNull null]] ? "never" : [viewSetupOperation                  UTF8String]);
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

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 5)) != 0) {
        printf("\n\nError: timed out waiting for response\n");
    }
#endif // OCTAGON
}

- (void)fetch: (NSString*) view {
#if OCTAGON
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [[self.connection remoteObjectProxyWithErrorHandler: ^(NSError* error) {
        printf("\n\nError talking with daemon: %s\n", [[error description] UTF8String]);
        dispatch_semaphore_signal(sema);

    }] rpcFetchAndProcessChanges:view reply:^(NSError* error) {
        if(error) {
            printf("Error fetching: %s\n", [[error description] UTF8String]);
        } else {
            printf("Complete.\n");
        }

        dispatch_semaphore_signal(sema);
    }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 65)) != 0) {
        printf("\n\nError: timed out waiting for response\n");
    }
#endif // OCTAGON
}

- (void)push: (NSString*) view {
#if OCTAGON
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);

    [[self.connection remoteObjectProxyWithErrorHandler: ^(NSError* error) {
        printf("\n\nError talking with daemon: %s\n", [[error description] UTF8String]);
        dispatch_semaphore_signal(sema);

    }] rpcPushOutgoingChanges:view reply:^(NSError* error) {
        if(error) {
            printf("Error pushing: %s\n", [[error description] UTF8String]);
        } else {
            printf("Complete.\n");
        }

        dispatch_semaphore_signal(sema);
    }];

    if(dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 65)) != 0) {
        printf("\n\nError: timed out waiting for response\n");
    }
#endif // OCTAGON
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
static int getAnalyticsSysdiagnose = false;
static int getAnalyticsJSON = false;
static int uploadAnalytics = false;

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
        { .command="get-analytics-sysdiagnose", .flag=&getAnalyticsSysdiagnose, .flagval=true, .description="Retrieve the current sysdiagnose dump for CKKS analytics"},
        { .command="get-analytics", .flag=&getAnalyticsJSON, .flagval=true, .description="Retrieve the current JSON blob that would be uploaded to the logging server if an upload occurred now"},
        { .command="upload-analytics", .flag=&uploadAnalytics, .flagval=true, .description="Force an upload of analytics data to cloud server"},
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
        xpc_endpoint_t endpoint = NULL;

        CFErrorRef cferror = NULL;
        endpoint = _SecSecuritydCopyCKKSEndpoint(&cferror);
        if (endpoint == NULL) {
            CFStringRef errorstr = NULL;

            if(cferror) {
                errorstr = CFErrorCopyDescription(cferror);
            }

            errx(1, "no CKKSControl endpoint available: %s", errorstr ? CFStringGetCStringPtr(errorstr, kCFStringEncodingUTF8) : "unknown error");
        }

        CKKSControl *ctl = [[CKKSControl alloc] initWithEndpoint:endpoint];
        if (ctl == NULL) {
            errx(1, "failed to create CKKSControl object");
        }

        NSString* view = viewArg ? [NSString stringWithCString: viewArg encoding: NSUTF8StringEncoding] : nil;

        if(status || perfCounters) {
            NSMutableDictionary *statusDict = [[NSMutableDictionary alloc] init];
            statusDict[@"performance"] = [ctl printPerformanceCounters];
            if (!json) {
                print_result(statusDict, false);
                if (status) {
                    [ctl status_custom:view];
                }
            } else {
                if (status) {
                    statusDict[@"status"] = [ctl status:view];
                }
                print_result(statusDict, true);
            }
        } else if(fetch) {
            [ctl fetch:view];
        } else if(push) {
            [ctl push:view];
        } else if(reset) {
            [ctl resetLocal:view];
        } else if(resetCloudKit) {
            [ctl resetCloudKit:view];
        } else if(resync) {
            [ctl resync:view];
        } else if(getAnalyticsSysdiagnose) {
            [ctl getAnalyticsSysdiagnose];
        } else if(getAnalyticsJSON) {
            [ctl getAnalyticsJSON];
        } else if(uploadAnalytics) {
            [ctl forceAnalyticsUpload];
        } else {
            print_usage(&args);
            return -1;
        }
    }
    return 0;
}
