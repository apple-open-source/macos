
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#import <Security/SecXPCHelper.h>

#import "keychain/escrowrequest/EscrowRequestXPCProtocol.h"
#import "utilities/debugging.h"

NSXPCInterface* SecEscrowRequestSetupControlProtocol(NSXPCInterface* interface) {
    NSSet<Class>* errClasses = [SecXPCHelper safeErrorClasses];

    @try {
        [interface setClasses:errClasses forSelector:@selector(triggerEscrowUpdate:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(cachePrerecord:serializedPrerecord:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchPrerecord:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchRequestWaitingOnPasscode:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchRequestStatuses:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(resetAllRequests:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(storePrerecordsInEscrow:) argumentIndex:1 ofReply:YES];
        
    }
    @catch(NSException* e) {
        secerror("SecEscrowRequestSetupControlProtocol failed, continuing, but you might crash later: %@", e);
        @throw e;
    }

    return interface;
}

