//
//  SOSTransportCircleKVS.c
//  sec
//
#include "keychain/SecureObjectSync/SOSTransport.h"
#include "keychain/SecureObjectSync/SOSTransportCircle.h"
#include "keychain/SecureObjectSync/SOSTransportCircleKVS.h"
#include "keychain/SecureObjectSync/SOSKVSKeys.h"
#include "keychain/SecureObjectSync/SOSInternal.h"
#include "keychain/SecureObjectSync/CKBridge/SOSCloudKeychainClient.h"
#include <utilities/SecCFWrappers.h>
#import "keychain/SecureObjectSync/SOSAccountTrustClassic.h"


@implementation SOSKVSCircleStorageTransport

@synthesize pending_changes = pending_changes;
@synthesize circleName = circleName;
extern CFStringRef kSOSAccountDebugScope;

-(id)init
{
    return [super init];
}

-(id)initWithAccount:(SOSAccount*)acct andCircleName:(NSString*)name
{
    if ((self = [super init])) {
        self.pending_changes = [NSMutableDictionary dictionary];
        self.circleName = [[NSString alloc] initWithString:name];
        self.account = acct;
        SOSRegisterTransportCircle(self);
    }
    return self;
}

-(NSString*) getCircleName
{
    return self.circleName;
}

-(CFIndex) getTransportType{
    return kKVS;
}

static bool SOSTransportCircleKVSUpdateKVS(NSDictionary *changes, CFErrorRef *error)
{
    CloudKeychainReplyBlock log_error = ^(CFDictionaryRef returnedValues __unused, CFErrorRef block_error) {
        if (block_error) {
            secerror("Error putting: %@", block_error);
        }
    };
    
    SOSCloudKeychainPutObjectsInCloud((__bridge CFDictionaryRef)(changes), dispatch_get_global_queue(SOS_TRANSPORT_PRIORITY, 0), log_error);
    return true;
}

-(bool)kvsSendPendingChanges:(CFErrorRef *)error
{
    CFErrorRef changeError = NULL;
    
    if (self.pending_changes == NULL || [self.pending_changes count] == 0) {
        return true;
    }
    
    CFTypeRef dsid = SOSAccountGetValue(account, kSOSDSIDKey, error);
    if(dsid == NULL)
        dsid = kCFNull;
    
    [self.pending_changes setObject:(__bridge id _Nonnull)((void*)(dsid)) forKey:(__bridge NSString*)kSOSKVSRequiredKey];
    
    bool success = SOSTransportCircleKVSUpdateKVS(self.pending_changes, &changeError);
    if (success) {
        [self.pending_changes removeAllObjects];
    } else {
        SOSCreateErrorWithFormat(kSOSErrorSendFailure, changeError, error, NULL,
                                 CFSTR("Send changes block failed [%@]"), self.pending_changes);
    }
    
    return success;
}

-(void)kvsAddToPendingChanges:(CFStringRef) message_key data:(CFDataRef)message_data
{
    if (self.pending_changes == NULL) {
        self.pending_changes = [NSMutableDictionary dictionary];
    }
    if (message_data == NULL) {
        [self.pending_changes setObject:[NSNull null] forKey:(__bridge NSString*)message_key];
    } else {
        [self.pending_changes setObject:(__bridge NSData*)message_data forKey:(__bridge NSString*)message_key];
    }
}

static bool SOSTransportCircleKVSUpdateRetirementRecords(CFDictionaryRef updates, CFErrorRef* error){
    CFErrorRef updateError = NULL;
    bool success = false;
    if (SOSTransportCircleKVSUpdateKVS((__bridge NSDictionary*)updates, &updateError)){
        success = true;
    } else {
        SOSCreateErrorWithFormat(kSOSErrorSendFailure, updateError, error, NULL,
                                 CFSTR("update parameters key failed [%@]"), updates);
    }
    return success;
}
-(bool) expireRetirementRecords:(CFDictionaryRef) retirements err:(CFErrorRef *)error
{
    bool success = true;
    CFMutableDictionaryRef keysToWrite = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    
    CFDictionaryForEach(retirements, ^(const void *key, const void *value) {
        if (isString(key) && isArray(value)) {
            CFStringRef circle_name = (CFStringRef) key;
            CFArrayRef retirees = (CFArrayRef) value;
            
            CFArrayForEach(retirees, ^(const void *value) {
                if (isString(value)) {
                    CFStringRef retiree_id = (CFStringRef) value;
                    
                    CFStringRef kvsKey = SOSRetirementKeyCreateWithCircleNameAndPeer(circle_name, retiree_id);
                    
                    CFDictionaryAddValue(keysToWrite, kvsKey, kCFNull);
                    
                    CFReleaseSafe(kvsKey);
                }
            });
        }
    });
    
    if(CFDictionaryGetCount(keysToWrite)) {
        success = SOSTransportCircleKVSUpdateRetirementRecords(keysToWrite, error);
    }
    CFReleaseNull(keysToWrite);
    
    return success;
}

-(bool) flushChanges:(CFErrorRef *)error
{
    return [self kvsSendPendingChanges:error];

}

-(bool) postCircle:(CFStringRef)name circleData:(CFDataRef)circle_data err:(CFErrorRef *)error
{
    CFStringRef circle_key = SOSCircleKeyCreateWithName(name, error);
    if (circle_key)
        [self kvsAddToPendingChanges:circle_key data:circle_data];
   
    CFReleaseNull(circle_key);
    
    return true;
}

-(CFDictionaryRef)CF_RETURNS_RETAINED handleRetirementMessages:(CFMutableDictionaryRef) circle_retirement_messages_table err:(CFErrorRef *)error
{
    return SOSAccountHandleRetirementMessages(self.account, circle_retirement_messages_table, error);
}

-(CFArrayRef)CF_RETURNS_RETAINED handleCircleMessagesAndReturnHandledCopy:(CFMutableDictionaryRef) circle_circle_messages_table err:(CFErrorRef *)error
{
    CFMutableArrayRef handledKeys = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryForEach(circle_circle_messages_table, ^(const void *key, const void *value) {
        CFErrorRef circleMessageError = NULL;
        if(!isString(key) || !isData(value)) {
            secerror("Error, Key-Value for CircleMessage was not CFString/CFData");
        } if (!SOSAccountHandleCircleMessage(self.account, key, value, &circleMessageError)) {
            secerror("Error handling circle message %@ (%@): %@", key, value, circleMessageError);
        } else{
            CFStringRef circle_id = (CFStringRef) key;
            CFArrayAppendValue(handledKeys, circle_id);
        }
        CFReleaseNull(circleMessageError);
    });
    
    return handledKeys;
}

-(bool)kvsAppendKeyInterest:(CFMutableArrayRef) alwaysKeys firstUnlock:(CFMutableArrayRef) afterFirstUnlockKeys unlocked:(CFMutableArrayRef)unlockedKeys err:(CFErrorRef *)error
{
    CFStringRef circle_name = NULL;
    CFStringRef circle_key = NULL;
    
    if(SOSAccountHasPublicKey(self.account, NULL)){
        require_quiet(circle_name = (__bridge CFStringRef)self.circleName, fail);
        require_quiet(circle_key = SOSCircleKeyCreateWithName(circle_name, error), fail);

        require_quiet(account, fail);
        SOSAccountTrustClassic *trust = account.trust;
        SOSCircleRef circle = trust.trustedCircle;
        require_quiet(circle && CFEqualSafe(circle_name, SOSCircleGetName(circle)), fail);

        SOSCircleForEachActivePeer(circle, ^(SOSPeerInfoRef peer) {
            CFStringRef retirement_key = SOSRetirementKeyCreateWithCircleNameAndPeer(circle_name, SOSPeerInfoGetPeerID(peer));
            CFArrayAppendValue(alwaysKeys, retirement_key);
            CFReleaseNull(retirement_key);
        });
        
        CFArrayAppendValue(unlockedKeys, circle_key); // This is where circle interest is handled
        
        CFReleaseNull(circle_key);
    }
    return true;

fail:
    CFReleaseNull(circle_key);
    return false;
}

//register ring key
-(bool)kvsAppendRingKeyInterest:(CFMutableArrayRef) alwaysKeys firstUnlock:(CFMutableArrayRef)afterFirstUnlockKeys unlocked:(CFMutableArrayRef) unlockedKeys err:(CFErrorRef *)error
{
    if(SOSAccountHasPublicKey(self.account, NULL)){
        require_quiet(account, fail);
        SOSAccountTrustClassic *trust = account.trust;
        SOSCircleRef circle = trust.trustedCircle;
        require_quiet(circle, fail);

        // Always interested in backup rings:
        SOSAccountForEachRingName(account, ^(CFStringRef ringName) {
            CFStringRef ring_key = SOSRingKeyCreateWithRingName(ringName);
            CFArrayAppendValue(unlockedKeys, ring_key);
            CFReleaseNull(ring_key);
        });
    }
    return true;
fail:
    return false;
}

//register debug scope key
-(bool)kvsAppendDebugKeyInterest:(CFMutableArrayRef) alwaysKeys firstUnlock:(CFMutableArrayRef)afterFirstUnlockKeys unlocked:(CFMutableArrayRef) unlockedKeys err:(CFErrorRef *)error
{
    CFStringRef key = SOSDebugInfoKeyCreateWithTypeName(kSOSAccountDebugScope);
    CFArrayAppendValue(alwaysKeys, key);
    CFReleaseNull(key);
    return true;
}

//send debug info over KVS
-(bool) kvssendDebugInfo:(CFStringRef) type debug:(CFTypeRef) debugInfo  err:(CFErrorRef *)error
{
    CFStringRef key = SOSDebugInfoKeyCreateWithTypeName(type);
    NSDictionary *changes = @{(__bridge NSString*)key:(__bridge id _Nonnull)debugInfo};
    
    CFReleaseNull(key);
    bool success = SOSTransportCircleKVSUpdateKVS(changes, error);
    
    return success;
}

-(bool) kvsSendAccountChangedWithDSID:(CFStringRef) dsid err:(CFErrorRef *)error
{
    NSDictionary *changes = @{(__bridge NSString*)kSOSKVSAccountChangedKey:(__bridge NSString*)dsid};
    
    bool success = SOSTransportCircleKVSUpdateKVS(changes, error);
    
    
    return success;
}

//send the Ring over KVS
-(bool) kvsRingPostRing:(CFStringRef) ringName ring:(CFDataRef) ring err:(CFErrorRef *)error
{
    CFStringRef ringKey = SOSRingKeyCreateWithName(ringName, error);
    
    if(ringKey)
        [self kvsAddToPendingChanges:ringKey data:ring];
    
    CFReleaseNull(ringKey);
    
    return true;
}

-(bool) kvsRingFlushChanges:(CFErrorRef*) error
{
    return [self kvsSendPendingChanges:error];
}

-(bool) kvsSendOfficialDSID:(CFStringRef) dsid err:(CFErrorRef *)error
{
    NSDictionary *changes = @{(__bridge NSString*)kSOSKVSOfficialDSIDKey : (__bridge NSString*)dsid};
    bool success = SOSTransportCircleKVSUpdateKVS(changes, error);
    return success;
}

-(bool) postRetirement:(CFStringRef)circleName peer:(SOSPeerInfoRef)peer err:(CFErrorRef *)error
{
    CFDataRef retirement_data = SOSPeerInfoCopyEncodedData(peer, kCFAllocatorDefault, error);

    require_quiet(retirement_data, fail);

    CFStringRef retirement_key = SOSRetirementKeyCreateWithCircleNameAndPeer((__bridge CFStringRef)(self.circleName), SOSPeerInfoGetPeerID(peer));
    if (retirement_key)
        [self kvsAddToPendingChanges:retirement_key data:retirement_data];
    
    CFReleaseNull(retirement_key);
    CFReleaseNull(retirement_data);
    return true;
fail:
    CFReleaseNull(retirement_data);
    return true;
}
@end

