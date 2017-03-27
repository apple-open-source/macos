
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSKVSKeys.h>
#include <Security/SecureObjectSync/SOSAccountPriv.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSTransportKeyParameterKVS.h>
#include <Security/SecureObjectSync/SOSTransportCircleKVS.h>
#include <Security/SecureObjectSync/SOSTransportMessageKVS.h>
#include <Security/SecureObjectSync/SOSTransportMessageIDS.h>
#include <Security/SecureObjectSync/SOSTransportMessage.h>
#include <Security/SecureObjectSync/SOSRing.h>

#include <SOSCloudKeychainClient.h>
#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>
#include <CoreFoundation/CFBase.h>

CFStringRef kKeyParameter = CFSTR("KeyParameter");
CFStringRef kCircle = CFSTR("Circle");
CFStringRef kMessage = CFSTR("Message");
CFStringRef kAlwaysKeys = CFSTR("AlwaysKeys");
CFStringRef kFirstUnlocked = CFSTR("FirstUnlockKeys");
CFStringRef kUnlocked = CFSTR("UnlockedKeys");
extern CFStringRef kSOSAccountDebugScope;

#define DATE_LENGTH 18

CFStringRef SOSInterestListCopyDescription(CFArrayRef interests)
{
    CFMutableStringRef description = CFStringCreateMutable(kCFAllocatorDefault, 0);
    CFStringAppendFormat(description, NULL, CFSTR("<Interest: "));
    
    if (interests) {
        CFArrayForEach(interests, ^(const void* string) {
            if (isString(string))
             
                CFStringAppendFormat(description, NULL, CFSTR(" '%@'"), string);
        });
    }
    CFStringAppend(description, CFSTR(">"));

    return description;
}


//
// MARK: Key Interest Processing
//

CFGiblisGetSingleton(CFMutableArrayRef, SOSGetTransportMessages, sTransportMessages,  ^{
    *sTransportMessages = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);
});

CFGiblisGetSingleton(CFMutableArrayRef, SOSGetTransportKeyParameters, sTransportKeyParameters,  ^{
    *sTransportKeyParameters = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);
});

CFGiblisGetSingleton(CFMutableArrayRef, SOSGetTransportCircles, sTransportCircles,  ^{
    *sTransportCircles = CFArrayCreateMutable(kCFAllocatorDefault, 0, NULL);
});


void SOSRegisterTransportMessage(SOSTransportMessageRef additional) {
    CFArrayAppendValue(SOSGetTransportMessages(), additional);
}

void SOSUnregisterTransportMessage(SOSTransportMessageRef removal) {
    CFArrayRemoveAllValue(SOSGetTransportMessages(), removal);
}

void SOSUnregisterAllTransportMessages() {
    CFArrayRemoveAllValues(SOSGetTransportMessages());
}

void SOSRegisterTransportCircle(SOSTransportCircleRef additional) {
    CFArrayAppendValue(SOSGetTransportCircles(), additional);
}

void SOSUnregisterTransportCircle(SOSTransportCircleRef removal) {
    CFArrayRemoveAllValue(SOSGetTransportCircles(), removal);
}

void SOSUnregisterAllTransportCircles() {
    CFArrayRemoveAllValues(SOSGetTransportCircles());
}

void SOSRegisterTransportKeyParameter(SOSTransportKeyParameterRef additional) {
    CFArrayAppendValue(SOSGetTransportKeyParameters(), additional);
}

void SOSUnregisterTransportKeyParameter(SOSTransportKeyParameterRef removal) {
    CFArrayRemoveAllValue(SOSGetTransportKeyParameters(), removal);
}

void SOSUnregisterAllTransportKeyParameters() {
    CFArrayRemoveAllValues(SOSGetTransportKeyParameters());
}

//
// Should we be dispatching back to our queue to handle later
//
void SOSUpdateKeyInterest(SOSAccountRef account)
{
    CFMutableArrayRef alwaysKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFMutableArrayRef afterFirstUnlockKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFMutableArrayRef whenUnlockedKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFMutableDictionaryRef keyDict = CFDictionaryCreateMutableForCFTypes (kCFAllocatorDefault);
    
    CFArrayForEach(SOSGetTransportKeyParameters(), ^(const void *value) {
        SOSTransportKeyParameterRef tKP = (SOSTransportKeyParameterRef) value;
        if (SOSTransportKeyParameterGetAccount(tKP) == account && SOSTransportKeyParameterGetTransportType(tKP, NULL) == kKVS) {
            SOSTransportKeyParameterKVSRef tkvs = (SOSTransportKeyParameterKVSRef) value;
            CFErrorRef localError = NULL;
        
            if (!SOSTransportKeyParameterKVSAppendKeyInterests(tkvs, alwaysKeys, afterFirstUnlockKeys, whenUnlockedKeys, &localError)) {
                secerror("Error getting key parameters interests %@", localError);
            }
            CFReleaseNull(localError);
        }
    });
    CFMutableDictionaryRef keyParamsDict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionarySetValue(keyParamsDict, kAlwaysKeys, alwaysKeys);
    CFDictionarySetValue(keyParamsDict, kFirstUnlocked, afterFirstUnlockKeys);
    CFDictionarySetValue(keyParamsDict, kUnlocked, whenUnlockedKeys);
    CFDictionarySetValue(keyDict, kKeyParameter, keyParamsDict);

    CFReleaseNull(alwaysKeys);
    CFReleaseNull(afterFirstUnlockKeys);
    CFReleaseNull(whenUnlockedKeys);
    alwaysKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    afterFirstUnlockKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    whenUnlockedKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    
    CFArrayForEach(SOSGetTransportCircles(), ^(const void *value) {
        if (SOSTransportCircleGetAccount((SOSTransportCircleRef)value) == account && SOSTransportCircleGetTransportType((SOSTransportCircleRef)value, NULL) == kKVS) {
            SOSTransportCircleKVSRef tkvs = (SOSTransportCircleKVSRef) value;
            CFErrorRef localError = NULL;

            if(!SOSTransportCircleKVSAppendKeyInterest(tkvs, alwaysKeys, afterFirstUnlockKeys, whenUnlockedKeys, &localError)){
                secerror("Error getting circle interests %@", localError);
            }
            if(!SOSTransportCircleKVSAppendPeerInfoKeyInterest(tkvs, alwaysKeys, afterFirstUnlockKeys, whenUnlockedKeys, &localError)){
                secerror("Error getting peer info interests %@", localError);
            }
            if(!SOSTransportCircleKVSAppendRingKeyInterest(tkvs, alwaysKeys, afterFirstUnlockKeys, whenUnlockedKeys, &localError)){
                secerror("Error getting ring interests %@", localError);
            }
            if(!SOSTransportCircleKVSAppendDebugKeyInterest(tkvs, alwaysKeys, afterFirstUnlockKeys, whenUnlockedKeys, &localError)){
                secerror("Error getting debug key interests %@", localError);
            }
            CFReleaseNull(localError);
        }
        
    });
    CFMutableDictionaryRef circleDict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionarySetValue(circleDict, kAlwaysKeys, alwaysKeys);
    CFDictionarySetValue(circleDict, kFirstUnlocked, afterFirstUnlockKeys);
    CFDictionarySetValue(circleDict, kUnlocked, whenUnlockedKeys);
    CFDictionarySetValue(keyDict, kCircle, circleDict);
    
    CFReleaseNull(alwaysKeys);
    CFReleaseNull(afterFirstUnlockKeys);
    CFReleaseNull(whenUnlockedKeys);
    alwaysKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    afterFirstUnlockKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    whenUnlockedKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    
    CFArrayForEach(SOSGetTransportMessages(), ^(const void *value) {
        if (SOSTransportMessageGetAccount((SOSTransportMessageRef) value) == account && SOSTransportMessageGetTransportType((SOSTransportMessageRef) value, NULL) == kKVS) {
            SOSTransportMessageKVSRef tkvs = (SOSTransportMessageKVSRef) value;
            CFErrorRef localError = NULL;
        
            if(!SOSTransportMessageKVSAppendKeyInterest(tkvs, alwaysKeys, afterFirstUnlockKeys, whenUnlockedKeys, &localError)){
                secerror("Error getting message interests %@", localError);
            }
            CFReleaseNull(localError);
        }
    });
    
    CFMutableDictionaryRef messageDict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionarySetValue(messageDict, kAlwaysKeys, alwaysKeys);
    CFDictionarySetValue(messageDict, kFirstUnlocked, afterFirstUnlockKeys);
    CFDictionarySetValue(messageDict, kUnlocked, whenUnlockedKeys);
    CFDictionarySetValue(keyDict, kMessage, messageDict);
    
    //
    // Log what we are about to do.
    //
    secnotice("key-interests", "Updating interests: %@", keyDict);

    CFStringRef uuid = SOSAccountCopyUUID(account);
    SOSCloudKeychainUpdateKeys(keyDict, uuid, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(CFDictionaryRef returnedValues, CFErrorRef error) {
        if (error) {
            secerror("Error updating keys: %@", error);
        }
    });
    CFReleaseNull(uuid);

    CFReleaseNull(alwaysKeys);
    CFReleaseNull(afterFirstUnlockKeys);
    CFReleaseNull(whenUnlockedKeys);
    CFReleaseNull(keyParamsDict);
    CFReleaseNull(circleDict);
    CFReleaseNull(messageDict);
    CFReleaseNull(keyDict);
}


static void showWhatWasHandled(CFDictionaryRef updates, CFMutableArrayRef handledKeys) {
    
    CFMutableStringRef updateStr = CFStringCreateMutable(kCFAllocatorDefault, 0);
    CFMutableStringRef handledKeysStr = CFStringCreateMutable(kCFAllocatorDefault, 0);
    
    CFDictionaryForEach(updates, ^(const void *key, const void *value) {
        if (isString(key)) {
            CFStringAppendFormat(updateStr, NULL, CFSTR("%@ "), (CFStringRef)key);
        }
    });
    CFArrayForEach(handledKeys, ^(const void *value) {
        if (isString(value)) {
            CFStringAppendFormat(handledKeysStr, NULL, CFSTR("%@ "), (CFStringRef)value);
        }
    });
    secinfo("updates", "Updates [%ld]: %@", CFDictionaryGetCount(updates), updateStr);
    secinfo("updates", "Handled [%ld]: %@", CFArrayGetCount(handledKeys), handledKeysStr);
    
    CFReleaseSafe(updateStr);
    CFReleaseSafe(handledKeysStr);
}

#define KVS_STATE_INTERVAL 50

CF_RETURNS_RETAINED
CFMutableArrayRef SOSTransportDispatchMessages(SOSAccountTransactionRef txn, CFDictionaryRef updates, CFErrorRef *error){
    SOSAccountRef account = txn->account;
    
    CFMutableArrayRef handledKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFStringRef dsid = NULL;
    
    if(CFDictionaryGetValueIfPresent(updates, kSOSKVSAccountChangedKey, (const void**)&dsid)){
        secnotice("accountChange", "SOSTransportDispatchMessages received kSOSKVSAccountChangedKey");

        // While changing accounts we may modify the key params array. To avoid stepping on ourselves we
        // copy the list for iteration.  Now modifying the transport outside of the list iteration.
        CFMutableArrayRef transportsToUse = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        
        CFArrayForEach(SOSGetTransportKeyParameters(), ^(const void *value) {
            SOSTransportKeyParameterRef transport = (SOSTransportKeyParameterRef) value;
            if(CFEqualSafe(SOSTransportKeyParameterGetAccount(transport), account)){
                CFArrayAppendValue(transportsToUse, transport);
            }

        });
        
        CFArrayForEach(transportsToUse, ^(const void *value) {
            SOSTransportKeyParameterRef tempTransport = (SOSTransportKeyParameterRef) value;
            
            CFStringRef accountDSID = (CFStringRef)SOSAccountGetValue(account, kSOSDSIDKey, error);
            
            if(accountDSID == NULL){
                SOSTransportKeyParameterHandleNewAccount(tempTransport, account);
                SOSAccountSetValue(account, kSOSDSIDKey, dsid, error);
                secdebug("dsid", "Assigning new DSID: %@", dsid);
            } else if(accountDSID != NULL && CFStringCompare(accountDSID, dsid, 0) != 0 ) {
                SOSTransportKeyParameterHandleNewAccount(tempTransport, account);
                SOSAccountSetValue(account, kSOSDSIDKey, dsid, error);
                secdebug("dsid", "Assigning new DSID: %@", dsid);
            } else {
                secdebug("dsid", "DSIDs are the same!");
            }
        });
        
        CFReleaseNull(transportsToUse);
    
        CFArrayAppendValue(handledKeys, kSOSKVSAccountChangedKey);
    }

    
    // Iterate through keys in updates.  Perform circle change update.
    // Then instantiate circles and engines and peers for all peers that
    // are receiving a message in updates.
    CFMutableDictionaryRef circle_peer_messages_table = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFMutableDictionaryRef circle_circle_messages_table = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFMutableDictionaryRef circle_retirement_messages_table = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFMutableDictionaryRef ring_update_message_table = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFMutableDictionaryRef peer_info_message_table = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFMutableDictionaryRef debug_info_message_table = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    
    __block CFDataRef newParameters = NULL;
    __block bool initial_sync = false;
    __block bool new_account = false;
    
    CFDictionaryForEach(updates, ^(const void *key, const void *value) {
        CFErrorRef localError = NULL;
        CFStringRef circle_name = NULL;
        CFStringRef ring_name = NULL;
        CFStringRef peer_info_name = NULL;
        CFStringRef from_name = NULL;
        CFStringRef to_name = NULL;
        CFStringRef backup_name = NULL;
        
        require_quiet(isString(key), errOut);
        switch (SOSKVSKeyGetKeyTypeAndParse(key, &circle_name, &peer_info_name, &ring_name, &backup_name, &from_name, &to_name)) {
            case kCircleKey:
                CFDictionarySetValue(circle_circle_messages_table, circle_name, value);
                break;
            case kInitialSyncKey:
                initial_sync = true;
                break;
            case kParametersKey:
                if (isData(value)) {
                    newParameters = (CFDataRef) CFRetainSafe(value);
                }
                break;
            case kMessageKey: {
                CFMutableDictionaryRef circle_messages = CFDictionaryEnsureCFDictionaryAndGetCurrentValue(circle_peer_messages_table, circle_name);
                CFDictionarySetValue(circle_messages, from_name, value);
                break;
            }
            case kRetirementKey: {
                CFMutableDictionaryRef circle_retirements = CFDictionaryEnsureCFDictionaryAndGetCurrentValue(circle_retirement_messages_table, circle_name);
                CFDictionarySetValue(circle_retirements, from_name, value);
                break;
            }
            case kAccountChangedKey:
                new_account = true;
                break;
            case kPeerInfoKey:
                CFDictionarySetValue(peer_info_message_table, peer_info_name, value);
                break;
            case kRingKey:
                if(isString(ring_name))
                    CFDictionarySetValue(ring_update_message_table, ring_name, value);
                break;
            case kDebugInfoKey:
                CFDictionarySetValue(debug_info_message_table, peer_info_name, value);
                break;
            case kLastCircleKey:
            case kLastKeyParameterKey:
            case kUnknownKey:
                secnotice("updates", "Unknown key '%@', ignoring", key);
                break;
                
        }

    errOut:
        CFReleaseNull(circle_name);
        CFReleaseNull(from_name);
        CFReleaseNull(to_name);
        CFReleaseNull(ring_name);
        CFReleaseNull(peer_info_name);
        CFReleaseNull(backup_name);
        
        if (error && *error)
            secerror("Peer message processing error for: %@ -> %@ (%@)", key, value, *error);
        if (localError)
            secerror("Peer message local processing error for: %@ -> %@ (%@)", key, value, localError);
        
        CFReleaseNull(localError);
    });
    
    
    if (newParameters) {
        CFArrayForEach(SOSGetTransportKeyParameters(), ^(const void *value) {
            SOSTransportKeyParameterRef tkvs = (SOSTransportKeyParameterRef) value;
            CFErrorRef localError = NULL;
            if(CFEqualSafe(SOSTransportKeyParameterGetAccount(tkvs), account)){
                if(!SOSTransportKeyParameterHandleKeyParameterChanges(tkvs, newParameters, localError))
                    secerror("Transport failed to handle new key parameters: %@", localError);
            }
        });
        CFArrayAppendValue(handledKeys, kSOSKVSKeyParametersKey);
    }
    CFReleaseNull(newParameters);
    
    if(initial_sync){
        CFArrayAppendValue(handledKeys, kSOSKVSInitialSyncKey);
    }
    
    if(CFDictionaryGetCount(debug_info_message_table)) {
        /* check for a newly set circle debug scope */
        CFTypeRef debugScope = CFDictionaryGetValue(debug_info_message_table, kSOSAccountDebugScope);
        if (debugScope) {
            if(isString(debugScope)){
                ApplyScopeListForID(debugScope, kScopeIDCircle);
            }else if(isDictionary(debugScope)){
                ApplyScopeDictionaryForID(debugScope, kScopeIDCircle);
            }
        }
        CFStringRef debugInfoKey = SOSDebugInfoKeyCreateWithTypeName(kSOSAccountDebugScope);
        CFArrayAppendValue(handledKeys, debugInfoKey);
        CFReleaseNull(debugInfoKey);
    }
    
    if(CFDictionaryGetCount(circle_retirement_messages_table)) {
        CFArrayForEach(SOSGetTransportCircles(), ^(const void *value) {
            SOSTransportCircleRef tkvs = (SOSTransportCircleRef) value;
            if(CFEqualSafe(SOSTransportCircleGetAccount((SOSTransportCircleRef)value), account)){
                CFErrorRef localError = NULL;
                CFDictionaryRef handledRetirementKeys = SOSTransportCircleHandleRetirementMessages(tkvs, circle_retirement_messages_table, error);
                if(handledRetirementKeys == NULL){
                    secerror("Transport failed to handle retirement messages: %@", localError);
                } else {
                    CFDictionaryForEach(handledRetirementKeys, ^(const void *key, const void *value) {
                        CFStringRef circle_name = (CFStringRef)key;
                        CFArrayRef handledPeerIDs = (CFArrayRef)value;
                        CFArrayForEach(handledPeerIDs, ^(const void *value) {
                            CFStringRef peer_id = (CFStringRef)value;
                            CFStringRef keyHandled = SOSRetirementKeyCreateWithCircleNameAndPeer(circle_name, peer_id);
                            CFArrayAppendValue(handledKeys, keyHandled);
                            CFReleaseNull(keyHandled);
                        });
                    });
                }
                CFReleaseNull(handledRetirementKeys);
                CFReleaseNull(localError);
            }
        });
    }
    if(CFDictionaryGetCount(peer_info_message_table)){
        CFArrayForEach(SOSGetTransportCircles(), ^(const void *value) {
            SOSTransportCircleRef tkvs = (SOSTransportCircleRef) value;
            if(CFEqualSafe(SOSTransportCircleGetAccount((SOSTransportCircleRef)value), account)){
                CFErrorRef localError = NULL;
                CFArrayRef handledPeerInfoMessages = SOSTransportCircleKVSHandlePeerInfoV2Messages(tkvs, peer_info_message_table, error);
                if(handledPeerInfoMessages == NULL){
                    secerror("Transport failed to handle peer info messages: %@", localError);
                } else {
                    CFArrayForEach(handledPeerInfoMessages, ^(const void *value) {
                        CFStringRef peer_id = (CFStringRef)value;
                        CFStringRef keyHandled = SOSPeerInfoV2KeyCreateWithPeerName(peer_id);
                        CFArrayAppendValue(handledKeys, keyHandled);
                        CFReleaseNull(keyHandled);
                    });
                }
                CFReleaseNull(handledPeerInfoMessages);
                CFReleaseNull(localError);
            }
        });
    }
    if(CFDictionaryGetCount(circle_peer_messages_table)) {
        CFArrayForEach(SOSGetTransportMessages(), ^(const void *value) {
            SOSTransportMessageRef tmsg = (SOSTransportMessageRef) value;
            CFDictionaryRef circleToPeersHandled = NULL;
            CFErrorRef handleMessagesError = NULL;
            CFErrorRef flushError = NULL;

            require_quiet(CFEqualSafe(SOSTransportMessageGetAccount(tmsg), account), done);

            circleToPeersHandled = SOSTransportMessageHandleMessages(tmsg, circle_peer_messages_table, &handleMessagesError);
            require_action_quiet(circleToPeersHandled, done, secnotice("msg", "No messages handled: %@", handleMessagesError));

            CFArrayRef handledPeers = asArray(CFDictionaryGetValue(circleToPeersHandled, SOSTransportMessageGetCircleName(tmsg)), NULL);

            if (handledPeers) {
                CFArrayForEach(handledPeers, ^(const void *value) {
                    CFStringRef peerID = asString(value, NULL);
                    if (peerID) {
                        
                        CFStringRef kvsHandledKey = SOSMessageKeyCreateFromPeerToTransport(tmsg, peerID);
                        if (kvsHandledKey) {
                            CFArrayAppendValue(handledKeys, kvsHandledKey);
                        }
                        CFReleaseNull(kvsHandledKey);
                    }
                });
            }

            require_action_quiet(SOSTransportMessageFlushChanges(tmsg, &flushError), done, secnotice("msg", "Flush failed: %@", flushError););

        done:
            CFReleaseNull(flushError);
            CFReleaseNull(circleToPeersHandled);
            CFReleaseNull(handleMessagesError);
        });
    }
    if(CFDictionaryGetCount(circle_circle_messages_table)) {
        CFArrayForEach(SOSGetTransportCircles(), ^(const void *value) {
            SOSTransportCircleRef tkvs = (SOSTransportCircleRef) value;
            if(CFEqualSafe(SOSTransportCircleGetAccount((SOSTransportCircleRef)value), account)){
                CFArrayRef handleCircleMessages = SOSTransportCircleHandleCircleMessages(tkvs, circle_circle_messages_table, error);
                CFErrorRef localError = NULL;
                if(handleCircleMessages == NULL){
                    secerror("Transport failed to handle circle messages: %@", localError);
                } else if(CFArrayGetCount(handleCircleMessages) == 0) {
                    if(CFDictionaryGetCount(circle_circle_messages_table) != 0) {
                        secerror("Transport failed to process all circle messages: (%ld/%ld) %@",
                                 CFArrayGetCount(handleCircleMessages),
                                 CFDictionaryGetCount(circle_circle_messages_table), localError);
                    } else {
                        secnotice("circle", "Transport handled no circle messages");
                    }
                } else {
                    CFArrayForEach(handleCircleMessages, ^(const void *value) {
                        CFStringRef keyHandled = SOSCircleKeyCreateWithName((CFStringRef)value, error);
                        CFArrayAppendValue(handledKeys, keyHandled);
                        CFReleaseNull(keyHandled);
                    });
                }
    
                CFReleaseNull(localError);
                CFReleaseNull(handleCircleMessages);
            }
            
        });
    }
    if(CFDictionaryGetCount(ring_update_message_table)){
        CFArrayForEach(SOSGetTransportCircles(), ^(const void *value) {
            if(CFEqualSafe(SOSTransportCircleGetAccount((SOSTransportCircleRef)value), account)){
                CFErrorRef localError = NULL;
                CFMutableArrayRef handledRingMessages = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

                CFDictionaryForEach(ring_update_message_table, ^(const void *key, const void *value) {
                    CFDataRef ringData = asData(value, NULL);
                    SOSRingRef ring = SOSRingCreateFromData(error, ringData);

                    if(SOSAccountUpdateRingFromRemote(account, ring, error)){
                        CFArrayAppendValue(handledRingMessages, key);
                    }
                    CFReleaseNull(ring);
                });
                if(CFArrayGetCount(handledRingMessages) == 0){
                    secerror("Transport failed to handle ring messages: %@", localError);
                } else {
                    CFArrayForEach(handledRingMessages, ^(const void *value) {
                        CFStringRef ring_name = (CFStringRef)value;
                        CFStringRef keyHandled = SOSRingKeyCreateWithRingName(ring_name);
                        CFArrayAppendValue(handledKeys, keyHandled);
                        CFReleaseNull(keyHandled);
                    });
                }
                CFReleaseNull(handledRingMessages);
                CFReleaseNull(localError);
            }
        });
    }

    CFReleaseNull(circle_retirement_messages_table);
    CFReleaseNull(circle_circle_messages_table);
    CFReleaseNull(circle_peer_messages_table);
    CFReleaseNull(debug_info_message_table);
    CFReleaseNull(ring_update_message_table);
    CFReleaseNull(peer_info_message_table);
    CFReleaseNull(debug_info_message_table);

    showWhatWasHandled(updates, handledKeys);
    
    return handledKeys;
}

