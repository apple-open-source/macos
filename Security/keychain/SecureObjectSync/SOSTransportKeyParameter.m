
#include "keychain/SecureObjectSync/SOSTransport.h"
#include "keychain/SecureObjectSync/SOSTransportKeyParameter.h"
#include "keychain/SecureObjectSync/SOSKVSKeys.h"
#include <securityd/SOSCloudCircleServer.h>
#include <utilities/SecCFWrappers.h>
#include "keychain/SecureObjectSync/SOSAccountPriv.h"
#include "keychain/SecureObjectSync/CKBridge/SOSCloudKeychainClient.h"

@implementation CKKeyParameter

@synthesize account = account;

-(bool) SOSTransportKeyParameterHandleKeyParameterChanges:(CKKeyParameter*) transport  data:(CFDataRef) data err:(CFErrorRef) error
{
    return SOSAccountHandleParametersChange(account, data, &error);
}

-(SOSAccount*) SOSTransportKeyParameterGetAccount:(CKKeyParameter*) transport
{
    return account;
}


-(CFIndex) SOSTransportKeyParameterGetTransportType:(CKKeyParameter*) transport err:(CFErrorRef *)error
{
    return kKVS;
}


-(void) SOSTransportKeyParameterHandleNewAccount:(CKKeyParameter*) transport acct:(SOSAccount*) acct
{
    SOSAccountSetToNew(acct);
}

-(id) initWithAccount:(SOSAccount*) acct
{
    self  = [super init];
    if(self){
        self.account = acct;
        SOSRegisterTransportKeyParameter(self);
    }
    return self;
}

-(bool) SOSTransportKeyParameterKVSAppendKeyInterests:(CKKeyParameter*)transport ak:(CFMutableArrayRef)alwaysKeys firstUnLock:(CFMutableArrayRef)afterFirstUnlockKeys unlocked:(CFMutableArrayRef) unlockedKeys err:(CFErrorRef *)error
{
    CFArrayAppendValue(alwaysKeys, kSOSKVSKeyParametersKey);

    return true;
}

static bool SOSTransportKeyParameterKVSUpdateKVS(CFDictionaryRef changes, CFErrorRef *error){
    CloudKeychainReplyBlock log_error = ^(CFDictionaryRef returnedValues __unused, CFErrorRef block_error) {
        if (block_error) {
            secerror("Error putting: %@", block_error);
        }
    };

    SOSCloudKeychainPutObjectsInCloud(changes, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), log_error);
    return true;
}

-(bool) SOSTransportKeyParameterPublishCloudParameters:(CKKeyParameter*) transport data:(CFDataRef)newParameters err:(CFErrorRef*) error
{
    if(newParameters) {
        secnotice("circleOps", "Publishing Cloud Parameters");
    } else {
        secnotice("circleOps", "Tried to publish nil Cloud Parameters");
        (void) SecRequirementError(newParameters != NULL, error, CFSTR("Tried to publish nil Cloud Parameters"));
        return false;
    }

    bool waitForeverForSynchronization = true;
    CFDictionaryRef changes = NULL;
    CFDataRef timeData = NULL;
    CFMutableStringRef timeDescription = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("["));
    CFAbsoluteTime currentTimeAndDate = CFAbsoluteTimeGetCurrent();

    withStringOfAbsoluteTime(currentTimeAndDate, ^(CFStringRef decription) {
        CFStringAppend(timeDescription, decription);
    });
    CFStringAppend(timeDescription, CFSTR("]"));

    timeData = CFStringCreateExternalRepresentation(NULL,timeDescription,
                                                    kCFStringEncodingUTF8, '?');

    CFMutableDataRef timeAndKeyParametersMutable = CFDataCreateMutable(kCFAllocatorDefault, CFDataGetLength(timeData) + CFDataGetLength(newParameters));
    CFDataAppend(timeAndKeyParametersMutable, timeData);
    CFDataAppend(timeAndKeyParametersMutable, newParameters);
    CFDataRef timeAndKeyParameters = CFDataCreateCopy(kCFAllocatorDefault, timeAndKeyParametersMutable);

    CFStringRef ourPeerID = (__bridge CFStringRef)account.peerID;

    if(ourPeerID != NULL){
        CFStringRef keyParamKey = SOSLastKeyParametersPushedKeyCreateWithPeerID(ourPeerID);

        changes = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                               kSOSKVSKeyParametersKey, newParameters,
                                               keyParamKey, timeAndKeyParameters,
                                               NULL);
        CFReleaseNull(keyParamKey);
    }
    else
    {
        CFStringRef keyParamKeyWithAccount = SOSLastKeyParametersPushedKeyCreateWithAccountGestalt(account);
        changes = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                               kSOSKVSKeyParametersKey, newParameters,
                                               keyParamKeyWithAccount, timeAndKeyParameters,
                                               NULL);
        CFReleaseNull(keyParamKeyWithAccount);
    }
    bool success = SOSTransportKeyParameterKVSUpdateKVS(changes, error);

    sync_the_last_data_to_kvs((__bridge CFTypeRef)(account), waitForeverForSynchronization);
    CFReleaseNull(changes);
    CFReleaseNull(timeData);
    CFReleaseNull(timeAndKeyParameters);
    CFReleaseNull(timeAndKeyParametersMutable);
    CFReleaseNull(timeDescription);

    return success;
}

@end

