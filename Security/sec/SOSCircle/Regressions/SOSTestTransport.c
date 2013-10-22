//
//  SOSTestTransport.c
//  sec
//
//  Created by Michael Brouwer on 10/16/12.
//

#include "SOSTestTransport.h"

#include <utilities/SecCFWrappers.h>
#include <test/testmore.h>

struct SOSTestTransport {
    struct SOSTransport t;
    CFDataRef lastMessage;
};


/* Transport protocol. */
static bool SOSTestTransportQueue(SOSTransportRef transport, CFDataRef message) {
    struct SOSTestTransport *t = (struct SOSTestTransport *)transport;
    if (t->lastMessage) {
        fail("We already had an unproccessed message");
        CFReleaseNull(t->lastMessage);
    }
    CFRetain(message);
    t->lastMessage = message;
    return true;
}

CFDataRef SOSTestTransportDequeue(SOSTransportRef transport) {
    struct SOSTestTransport *t = (struct SOSTestTransport *)transport;
    CFDataRef message = t->lastMessage;
    t->lastMessage = NULL;
    return message;
}

void SOSTestTransportDispose(SOSTransportRef transport) {
    struct SOSTestTransport *t = (struct SOSTestTransport *)transport;
    free(t);
}

SOSTransportRef SOSTestTransportCreate(void) {
    struct SOSTestTransport *transport = calloc(1, sizeof(struct SOSTestTransport));
    transport->t.send = SOSTestTransportQueue;

    return (SOSTransportRef)transport;
}
