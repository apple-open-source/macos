
#ifndef SOSTransportMessage_h
#define SOSTransportMessage_h

#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSEngine.h> // TODO: Remove when SOSEnginePeerMessageSentBlock moves to SOSPeer.h
#include <CoreFoundation/CFRuntime.h>

typedef struct __OpaqueSOSTransportMessage *SOSTransportMessageRef;


struct __OpaqueSOSTransportMessage {
    CFRuntimeBase   _base;
    SOSEngineRef    engine;
    SOSAccountRef   account;
    CFStringRef     circleName;
    /* Connections from CF land to vtable land */
    CFStringRef             (*copyDescription)(SOSTransportMessageRef object);
    void                    (*destroy)(SOSTransportMessageRef object);
    CFStringRef             (*getName)(SOSTransportMessageRef object);
    
    /* send message operations */
    bool                    (*syncWithPeers)(SOSTransportMessageRef transport, CFDictionaryRef circleToPeerIDs, CFErrorRef *error);
    bool                    (*cleanupAfterPeerMessages)(SOSTransportMessageRef transport, CFDictionaryRef circleToPeerIDs, CFErrorRef* error);
    bool                    (*sendMessages)(SOSTransportMessageRef transport, CFDictionaryRef circle_messages, CFErrorRef *error);
    bool                    (*flushChanges)(SOSTransportMessageRef transport, CFErrorRef *error);
    CFIndex                 (*getTransportType)(SOSTransportMessageRef transport, CFErrorRef *error);
    CFDictionaryRef         (*handleMessages)(SOSTransportMessageRef transport, CFMutableDictionaryRef circle_peer_messages_table, CFErrorRef *error);
    
};

CFStringRef SOSTransportMessageGetCircleName(SOSTransportMessageRef transport);

SOSTransportMessageRef SOSTransportMessageCreateForSubclass(size_t size,SOSAccountRef account, CFStringRef circleName, CFErrorRef *error);
bool SOSTransportMessageHandlePeerMessage(SOSTransportMessageRef transport, CFStringRef peer_id, CFDataRef codedMessage, CFErrorRef *error);

typedef bool (^SOSTransportSendToPeerBlock)(SOSTransportMessageRef transport, CFStringRef peerID, CFDataRef message, SOSEnginePeerMessageSentBlock sentBlock);

SOSEngineRef SOSTransportMessageGetEngine(SOSTransportMessageRef transport);

SOSAccountRef SOSTransportMessageGetAccount(SOSTransportMessageRef transport);
    
bool SOSTransportMessageCleanupAfterPeerMessages(SOSTransportMessageRef transport, CFDictionaryRef circleToPeerIDs, CFErrorRef* error);

bool SOSTransportMessageSendMessages(SOSTransportMessageRef transport, CFDictionaryRef circle_messages, CFErrorRef *error);
bool SOSTransportMessageFlushChanges(SOSTransportMessageRef transport, CFErrorRef *error);

bool SOSTransportMessageSyncWithPeers(SOSTransportMessageRef transport, CFDictionaryRef circleToPeerIDs, CFErrorRef *error);

SOSTransportMessageRef SOSTransportMessageCreateForSubclass(size_t size,
                                                            SOSAccountRef account, CFStringRef circleName,
                                                            CFErrorRef *error);
CF_RETURNS_RETAINED
CFDictionaryRef SOSTransportMessageHandleMessages(SOSTransportMessageRef transport, CFMutableDictionaryRef circle_peer_messages_table, CFErrorRef *error);

CFTypeID SOSTransportMessageGetTypeID(void);

CFIndex SOSTransportMessageGetTransportType(SOSTransportMessageRef transport, CFErrorRef *error);

#endif
