
#import <Foundation/Foundation.h>
#import <objc/runtime.h>

#import "keychain/escrowrequest/EscrowRequestXPCProtocol.h"
#import "utilities/debugging.h"

NSXPCInterface* SecEscrowRequestSetupControlProtocol(NSXPCInterface* interface) {
    static NSMutableSet *errClasses;

    static dispatch_once_t onceToken;

    dispatch_once(&onceToken, ^{
        errClasses = [NSMutableSet set];
        char *classes[] = {
            "NSArray",
            "NSData",
            "NSDate",
            "NSDictionary",
            "NSError",
            "NSNull",
            "NSNumber",
            "NSOrderedSet",
            "NSSet",
            "NSString",
            "NSURL",
        };

        for (unsigned n = 0; n < sizeof(classes)/sizeof(classes[0]); n++) {
            Class cls = objc_getClass(classes[n]);
            if (cls) {
                [errClasses addObject:cls];
            }
        }
    });

    @try {
        [interface setClasses:errClasses forSelector:@selector(triggerEscrowUpdate:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(cachePrerecord:serializedPrerecord:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchPrerecord:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchRequestWaitingOnPasscode:) argumentIndex:1 ofReply:YES];
        
    }
    @catch(NSException* e) {
        secerror("SecEscrowRequestSetupControlProtocol failed, continuing, but you might crash later: %@", e);
#if DEBUG
        @throw e;
#endif
    }

    return interface;
}

