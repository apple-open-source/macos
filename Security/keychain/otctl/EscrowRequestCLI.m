
#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <Security/SecItemPriv.h>
#import <Security/Security.h>
#import <xpc/xpc.h>

#include "utilities/SecCFWrappers.h"
#include "utilities/SecInternalReleasePriv.h"
#import "utilities/debugging.h"

#import "keychain/otctl/EscrowRequestCLI.h"

@implementation EscrowRequestCLI

- (instancetype)initWithEscrowRequest:(SecEscrowRequest*)escrowRequest
{
    if((self = [super init])) {
        _escrowRequest = escrowRequest;
    }

    return self;
}

- (long)trigger
{
    NSError* error = nil;
    [self.escrowRequest triggerEscrowUpdate:@"cli" error:&error];

    if(error) {
        printf("Errored: %s", [[error description] UTF8String]);
        return 1;

    } else {
        printf("Complete.\n");
    }
    return 0;
}

- (long)status
{
    NSError* error = nil;
    NSDictionary* statuses = [self.escrowRequest fetchStatuses:&error];

    if(error) {
        printf("Errored: %s\n", [[error description] UTF8String]);
        return 1;
    }

    if(statuses.count == 0) {
        printf("No requests are waiting for a passcode.\n");
        return 0;
    }

    for(NSString* uuid in statuses.allKeys) {
        printf("Request %s: %s\n", [uuid UTF8String], [[statuses[uuid] description] UTF8String]);
    }

    return 0;
}

- (long)reset
{
    NSError* error = nil;
    [self.escrowRequest resetAllRequests:&error];

    if(error) {
        printf("Errored: %s\n", [[error description] UTF8String]);
        return 1;
    }

    printf("Complete.\n");
    return 0;
}

- (long)storePrerecordsInEscrow
{
    NSError* error = nil;
    uint64_t recordsWritten = [self.escrowRequest storePrerecordsInEscrow:&error];

    if(error) {
        printf("Errored: %s\n", [[error description] UTF8String]);
        return 1;
    }

    printf("Complete: %d records written.\n", (int)recordsWritten);
    return 0;
}

@end
