
#include <SecureObjectSync/SOSInternal.h>
#include <SecureObjectSync/SOSKVSKeys.h>
#include <SecureObjectSync/SOSAccountPriv.h>
#include <SecureObjectSync/SOSTransport.h>
#include <SecureObjectSync/SOSTransportKeyParameterKVS.h>
#include <SecureObjectSync/SOSTransportCircleKVS.h>
#include <SecureObjectSync/SOSTransportMessageKVS.h>
#include <SOSCloudKeychainClient.h>
#include <utilities/debugging.h>

CFStringRef kKeyParameter = CFSTR("KeyParameter");
CFStringRef kCircle = CFSTR("Circle");
CFStringRef kMessage = CFSTR("Message");
CFStringRef kAlwaysKeys = CFSTR("AlwaysKeys");
CFStringRef kFirstUnlocked = CFSTR("FirstUnlockKeys");
CFStringRef kUnlocked = CFSTR("UnlockedKeys");



CFStringRef SOSInterestListCopyDescription(CFArrayRef interests)
{
    CFMutableStringRef description = CFStringCreateMutable(kCFAllocatorDefault, 0);
    CFStringAppendFormat(description, NULL, CFSTR("<Interest: "));
    
    CFArrayForEach(interests, ^(const void* string) {
        if (isString(string))
            CFStringAppendFormat(description, NULL, CFSTR(" '%@'"), string);
    });
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
void SOSUpdateKeyInterest(void)
{
    CFMutableArrayRef alwaysKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFMutableArrayRef afterFirstUnlockKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFMutableArrayRef whenUnlockedKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFMutableDictionaryRef keyDict = CFDictionaryCreateMutableForCFTypes (kCFAllocatorDefault);
    
    CFArrayForEach(SOSGetTransportKeyParameters(), ^(const void *value) {
        SOSTransportKeyParameterKVSRef tkvs = (SOSTransportKeyParameterKVSRef) value;
        CFErrorRef localError = NULL;
        
        if (!SOSTransportKeyParameterKVSAppendKeyInterests(tkvs, alwaysKeys, afterFirstUnlockKeys, whenUnlockedKeys, &localError)) {
            secerror("Error getting key parameters interests %@", localError);
        }
        CFReleaseNull(localError);
    });
    CFMutableDictionaryRef keyParamsDict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionarySetValue(keyParamsDict, kAlwaysKeys, alwaysKeys);
    CFDictionarySetValue(keyParamsDict, kFirstUnlocked, afterFirstUnlockKeys);
    CFDictionarySetValue(keyParamsDict, kUnlocked, whenUnlockedKeys);
    CFDictionarySetValue(keyDict, kKeyParameter, keyParamsDict);
    CFErrorRef updateError = NULL;
    CFReleaseNull(alwaysKeys);
    CFReleaseNull(afterFirstUnlockKeys);
    CFReleaseNull(whenUnlockedKeys);
    alwaysKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    afterFirstUnlockKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    whenUnlockedKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    
    CFArrayForEach(SOSGetTransportCircles(), ^(const void *value) {
        SOSTransportCircleKVSRef tkvs = (SOSTransportCircleKVSRef) value;
        CFErrorRef localError = NULL;
        
        if(!SOSTransportCircleKVSAppendKeyInterest(tkvs, alwaysKeys, afterFirstUnlockKeys, whenUnlockedKeys, &localError)){
            secerror("Error getting circle interests %@", localError);
        }
        CFReleaseNull(localError);
        
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
        SOSTransportMessageKVSRef tkvs = (SOSTransportMessageKVSRef) value;
        CFErrorRef localError = NULL;
        
        if(!SOSTransportMessageKVSAppendKeyInterest(tkvs, alwaysKeys, afterFirstUnlockKeys, whenUnlockedKeys, &localError)){
            secerror("Error getting message interests %@", localError);
        }
        CFReleaseNull(localError);
        
    });
    
    CFMutableDictionaryRef messageDict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionarySetValue(messageDict, kAlwaysKeys, alwaysKeys);
    CFDictionarySetValue(messageDict, kFirstUnlocked, afterFirstUnlockKeys);
    CFDictionarySetValue(messageDict, kUnlocked, whenUnlockedKeys);
    CFDictionarySetValue(keyDict, kMessage, messageDict);
    
    CFReleaseNull(alwaysKeys);
    CFReleaseNull(afterFirstUnlockKeys);
    CFReleaseNull(whenUnlockedKeys);
    alwaysKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    afterFirstUnlockKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    whenUnlockedKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    
    
    if (!SOSCloudKeychainUpdateKeys(keyDict, &updateError))
    {
        secerror("Error updating keys: %@", updateError);
        // TODO: propagate error(s) to callers.
    } else {
        if (CFArrayGetCount(whenUnlockedKeys) == 0) {
            secnotice("sync", "Unlocked keys were empty!");
        }
        // This leaks 3 CFStringRefs in DEBUG builds.
        CFStringRef alwaysKeysDesc = SOSInterestListCopyDescription(alwaysKeys);
        CFStringRef afterFirstUnlockKeysDesc = SOSInterestListCopyDescription(afterFirstUnlockKeys);
        CFStringRef unlockedKeysDesc = SOSInterestListCopyDescription(whenUnlockedKeys);
        secdebug("sync", "Updating interest: always: %@,\nfirstUnlock: %@,\nunlockedKeys: %@",
                 alwaysKeysDesc,
                 afterFirstUnlockKeysDesc,
                 unlockedKeysDesc);
        CFReleaseNull(alwaysKeysDesc);
        CFReleaseNull(afterFirstUnlockKeysDesc);
        CFReleaseNull(unlockedKeysDesc);
    }
    
    CFReleaseNull(updateError);
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
    secnotice("updates", "Updates [%ld]: %@\n", CFDictionaryGetCount(updates), updateStr);
    secnotice("updates", "Handled [%ld]: %@\n", CFArrayGetCount(handledKeys), handledKeysStr);
    //    secnotice("updates", "Updates: %@\n", updates);
    //   secnotice("updates", "Handled: %@\n", handledKeys);
    
    CFReleaseSafe(updateStr);
    CFReleaseSafe(handledKeysStr);
}

CF_RETURNS_RETAINED
CFMutableArrayRef SOSTransportDispatchMessages(SOSAccountRef account, CFDictionaryRef updates, CFErrorRef *error){
    
    CFMutableArrayRef handledKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    
    if(CFDictionaryContainsKey(updates, kSOSKVSAccountChangedKey)){
        // While changing accounts we may modify the key params array. To avoid stepping on ourselves we
        // copy the list for iteration.
        CFArrayRef originalKeyParams = CFArrayCreateCopy(kCFAllocatorDefault, SOSGetTransportKeyParameters());
        CFArrayForEach(originalKeyParams, ^(const void *value) {
            SOSTransportKeyParameterRef tkvs = (SOSTransportKeyParameterRef) value;
            if( (SOSTransportKeyParameterGetAccount((SOSTransportKeyParameterRef)value), account)){
                SOSTransportKeyParameterHandleNewAccount(tkvs);
            }
        });
        CFReleaseNull(originalKeyParams);
        CFArrayAppendValue(handledKeys, kSOSKVSAccountChangedKey);
    }
    
    // Iterate through keys in updates.  Perform circle change update.
    // Then instantiate circles and engines and peers for all peers that
    // are receiving a message in updates.
    CFMutableDictionaryRef circle_peer_messages_table = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFMutableDictionaryRef circle_circle_messages_table = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFMutableDictionaryRef circle_retirement_messages_table = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    __block CFDataRef newParameters = NULL;
    __block bool initial_sync = false;
    __block bool new_account = false;
    
    CFDictionaryForEach(updates, ^(const void *key, const void *value) {
        CFErrorRef localError = NULL;
        CFStringRef circle_name = NULL;
        CFStringRef from_name = NULL;
        CFStringRef to_name = NULL;
        switch (SOSKVSKeyGetKeyTypeAndParse(key, &circle_name, &from_name, &to_name)) {
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
                
            case kUnknownKey:
                secnotice("updates", "Unknown key '%@', ignoring", key);
                break;
                
        }
        
        CFReleaseNull(circle_name);
        CFReleaseNull(from_name);
        CFReleaseNull(to_name);
        
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
                
                if (!SOSTransportCircleFlushChanges(tkvs, &localError))
                    secerror("error flushing changes: %@", localError);
                
                CFReleaseNull(localError);
                CFReleaseNull(handleCircleMessages);
            }
            
        });
    }
    // TODO: These should all produce circle -> messageTypeHandled messages
    //       That probably needs to wait for separation of transport types.
    if(CFDictionaryGetCount(circle_peer_messages_table)) {
        CFArrayForEach(SOSGetTransportMessages(), ^(const void *value) {
            SOSTransportMessageRef tkvs = (SOSTransportMessageRef) value;
            if(CFEqualSafe(SOSTransportMessageGetAccount((SOSTransportMessageRef)value), account)){
                CFErrorRef handleMessagesError = NULL;
                CFDictionaryRef handledPeers = SOSTransportMessageHandleMessages(tkvs, circle_peer_messages_table, &handleMessagesError);
                
                if (handledPeers) {
                    // We need to look for and send responses.
                    
                    CFErrorRef syncError = NULL;
                    if (!SOSTransportMessageSyncWithPeers((SOSTransportMessageRef)tkvs, handledPeers, &syncError)) {
                        secerror("Sync with peers failed: %@", syncError);
                    }
                    
                    CFDictionaryForEach(handledPeers, ^(const void *key, const void *value) {
                        if (isString(key) && isArray(value)) {
                            CFArrayForEach(value, ^(const void *value) {
                                if (isString(value)) {
                                    CFStringRef peerID = (CFStringRef) value;
                                    
                                    CFStringRef kvsHandledKey = SOSMessageKeyCreateFromPeerToTransport((SOSTransportMessageKVSRef)tkvs, peerID);
                                    CFArrayAppendValue(handledKeys, kvsHandledKey);
                                    CFReleaseSafe(kvsHandledKey);
                                }
                            });
                        }
                    });
                    
                    CFErrorRef flushError = NULL;
                    if (!SOSTransportMessageFlushChanges((SOSTransportMessageRef)tkvs, &flushError)) {
                        secerror("Flush failed: %@", flushError);
                    }
                }
                else {
                    secerror("Didn't handle? : %@", handleMessagesError);
                }
                CFReleaseNull(handledPeers);
                CFReleaseNull(handleMessagesError);
            }
        });
    }
    CFReleaseNull(circle_retirement_messages_table);
    CFReleaseNull(circle_circle_messages_table);
    CFReleaseNull(circle_peer_messages_table);
    
    
    
    showWhatWasHandled(updates, handledKeys);
    
    return handledKeys;
}


