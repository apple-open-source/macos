
#ifndef SOSTransportMessage_h
#define SOSTransportMessage_h

#include <SecureObjectSync/SOSAccount.h>
#include <CoreFoundation/CFRuntime.h>

typedef struct __OpaqueSOSTransportMessage *SOSTransportMessageRef;

struct __OpaqueSOSTransportMessage {
    CFRuntimeBase   _base;
    SOSEngineRef    engine;
    SOSAccountRef   account;
    
    /* Connections from CF land to vtable land */
    CFStringRef             (*copyDescription)(SOSTransportMessageRef object);
    void                    (*destroy)(SOSTransportMessageRef object);
    
    /* send message operations */
    bool                    (*syncWithPeers)(SOSTransportMessageRef transport, CFDictionaryRef circleToPeerIDs, CFErrorRef *error);
    bool                    (*cleanupAfterPeerMessages)(SOSTransportMessageRef transport, CFDictionaryRef circleToPeerIDs, CFErrorRef* error);
    bool                    (*sendMessages)(SOSTransportMessageRef transport, CFDictionaryRef circle_messages, CFErrorRef *error);
    bool                    (*flushChanges)(SOSTransportMessageRef transport, CFErrorRef *error);
    CFDictionaryRef         (*handleMessages)(SOSTransportMessageRef transport, CFMutableDictionaryRef circle_peer_messages_table, CFErrorRef *error);
};

SOSTransportMessageRef SOSTransportMessageCreateForSubclass(size_t size,SOSAccountRef account, CFStringRef circleName, CFErrorRef *error);
bool SOSTransportMessageHandlePeerMessage(SOSTransportMessageRef transport, CFStringRef peer_id, CFDataRef codedMessage, CFErrorRef *error);

typedef bool (^SOSTransportSendToPeerBlock)(SOSTransportMessageRef transport, CFStringRef peerID, CFDataRef message, SOSEnginePeerMessageSentBlock sentBlock);

SOSEngineRef SOSTransportMessageGetEngine(SOSTransportMessageRef transport);

SOSAccountRef SOSTransportMessageGetAccount(SOSTransportMessageRef transport);
    
bool SOSTransportMessageCleanupAfterPeerMessages(SOSTransportMessageRef transport, CFDictionaryRef circleToPeerIDs, CFErrorRef* error);

bool SOSTransportMessageSendMessagesIfNeeded(SOSTransportMessageRef transport, CFStringRef circle_id, CFArrayRef handledPeers, CFErrorRef *error);

bool SOSTransportMessageSendMessages(SOSTransportMessageRef transport, CFDictionaryRef circle_messages, CFErrorRef *error);
bool SOSTransportMessageFlushChanges(SOSTransportMessageRef transport, CFErrorRef *error);


SOSTransportMessageRef SOSTransportMessageCreateForSubclass(size_t size,
                                                            SOSAccountRef account, CFStringRef circleName,
                                                            CFErrorRef *error);
CF_RETURNS_RETAINED
CFDictionaryRef SOSTransportMessageHandleMessages(SOSTransportMessageRef transport, CFMutableDictionaryRef circle_peer_messages_table, CFErrorRef *error);


#endif
