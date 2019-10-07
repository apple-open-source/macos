
#import <Foundation/Foundation.h>

#import "utilities/debugging.h"
#import "keychain/escrowrequest/EscrowRequestServer.h"
#import "keychain/escrowrequest/EscrowRequestServerHelpers.h"

// False by default, to avoid turning this on in tests that don't want it
static bool EscrowRequestServerEnabled = false;
bool EscrowRequestServerIsEnabled(void) {
    return EscrowRequestServerEnabled;
}
void EscrowRequestServerSetEnabled(bool enabled) {
    EscrowRequestServerEnabled = enabled;
}

void EscrowRequestServerInitialize(void) {
    secnotice("escrowrequest", "performing EscrowRequestServerInitialize");
    EscrowRequestServer* server = [EscrowRequestServer server];
    [server.controller.stateMachine startOperation];
}
