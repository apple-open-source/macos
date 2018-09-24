/*
 * Copyright (c) 2018 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * SIMAccess.m
 * - C wrapper functions over ObjC interface the SIM module
 */

#include "SIMAccessPrivate.h"
#include "SIMAccess.h"
#include "symbol_scope.h"
#include "myCFUtil.h"
#include "EAPLog.h"

#import <Foundation/Foundation.h>

#if TARGET_OS_EMBEDDED

#include <CoreTelephony/CTServerConnectionPriv.h>
#include <CoreTelephony/CTServerConnectionSubscriber.h>
#import <CoreTelephony/CoreTelephonyClient.h>
#import <CoreTelephony/CoreTelephonyClient+Subscriber.h>
#import <CoreTelephony/CoreTelephonyClientSubscriberDelegate.h>


STATIC CTXPCServiceSubscriptionContext *
SubscriptionContextUserPreferredGet(CoreTelephonyClient *coreTelephonyclient)
{
    NSError 				*error = nil;
    CTXPCServiceSubscriptionContext 	*userPreferredSubscriptionCtx = nil;
    CTXPCServiceSubscriptionInfo	*subscriptionInfo = nil;

    subscriptionInfo = [coreTelephonyclient getSubscriptionInfoWithError:&error];
    if (error) {
	/* failed to get the subscription contexts */
	EAPLOG_FL(LOG_ERR,
		  "CoreTelephonyClient.getSubscriptionInfoWithError failed with "
		  "error: %@", error);
	return nil;
    }
    if (!subscriptionInfo && (!subscriptionInfo.subscriptions || subscriptionInfo.subscriptions.count == 0)) {
	/* received nil or zero subscription contexts */
	EAPLOG_FL(LOG_ERR, "failed to get the subscription contexts");
	return nil;
    }
    for (CTXPCServiceSubscriptionContext *subscriptionContext in subscriptionInfo.subscriptions) {
	if (subscriptionContext) {
	    if (subscriptionContext.userDataPreferred && [subscriptionContext.userDataPreferred boolValue]) {
		userPreferredSubscriptionCtx = subscriptionContext;
		break;
	    }
	}
    }
    return userPreferredSubscriptionCtx;
}

STATIC CTXPCServiceSubscriptionContext *
SubscriptionContextMatchingSlotGet(CoreTelephonyClient *coreTelephonyclient, NSUUID *uuid)
{
    NSError 				*error = nil;
    CTXPCServiceSubscriptionContext 	*matchingSubscriptionCtx = nil;
    CTXPCServiceSubscriptionInfo	*subscriptionInfo = nil;

    subscriptionInfo = [coreTelephonyclient getSubscriptionInfoWithError:&error];
    if (error) {
	/* failed to get the subscription contexts */
	EAPLOG_FL(LOG_ERR,
		  "CoreTelephonyClient.getSubscriptionInfoWithError failed with "
		  "error: %@", error);
	return nil;
    }
    if (!subscriptionInfo && (!subscriptionInfo.subscriptions || subscriptionInfo.subscriptions.count == 0)) {
	/* received nil or zero subscription contexts */
	EAPLOG_FL(LOG_ERR, "failed to get the subscription contexts");
	return nil;
    }
    for (CTXPCServiceSubscriptionContext *subscriptionContext in subscriptionInfo.subscriptions) {
	if (subscriptionContext) {
	    if (subscriptionContext.uuid && [subscriptionContext.uuid isEqual:uuid]) {
		matchingSubscriptionCtx = subscriptionContext;
		break;
	    }
	}
    }
    return matchingSubscriptionCtx;
}

PRIVATE_EXTERN CFStringRef
_SIMCopyIMSI(CFDictionaryRef properties)
{
    @autoreleasepool {
    	NSError 				*error = nil;
    	NSString 				*imsi = nil;
	CTXPCServiceSubscriptionContext 	*preferredSubscriptionCtx = nil;
	CFStringRef 				slotUUID = NULL;

    	CoreTelephonyClient *coreTelephonyclient = [[CoreTelephonyClient alloc] init];
    	if (!coreTelephonyclient) {
	    /* failed to get the instance of Core Telephony Client */
	    EAPLOG_FL(LOG_ERR, "failed to get the CoreTelephonyClient instance");
	    return NULL;
    	}
	if (properties != NULL) {
	    slotUUID = isA_CFString(CFDictionaryGetValue(properties,
							 kCTSimSupportUICCAuthenticationSlotUUIDKey));
	}
	if (slotUUID) {
	    /* carrier Wi-Fi calling case */
	    NSUUID *uuid = [[NSUUID alloc] initWithUUIDString:(__bridge NSString *)slotUUID];
	    preferredSubscriptionCtx = SubscriptionContextMatchingSlotGet(coreTelephonyclient, uuid);
	} else {
	    /* carrier hotspot case */
	    preferredSubscriptionCtx = SubscriptionContextUserPreferredGet(coreTelephonyclient);
	}
	if (!preferredSubscriptionCtx) {
	    EAPLOG_FL(LOG_ERR, "failed to get the preferred subscription context");
	    return NULL;
    	}
	if (preferredSubscriptionCtx) {
	    imsi = [coreTelephonyclient copyMobileSubscriberIdentity:preferredSubscriptionCtx error:&error];
	    if (error) {
	    	/* failed to read IMSI */
	    	EAPLOG_FL(LOG_ERR,
		      	"CoreTelephonyClient.copyMobileSubscriberIdentity failed with "
		      	"error: %@", error);
	    	return NULL;
	    }
	    return (__bridge_retained CFStringRef)imsi;
    	}
    	return NULL;
    }
}

PRIVATE_EXTERN CFStringRef
_SIMCopyRealm(CFDictionaryRef properties)
{
    @autoreleasepool {
    	NSError 				*error = nil;
    	NSString 				*mcc = nil;
    	NSString 				*mnc = nil;
    	NSString 				*realm = nil;
	CTXPCServiceSubscriptionContext 	*preferredSubscriptionCtx = nil;
	CFStringRef 				slotUUID = NULL;

    	CoreTelephonyClient *coreTelephonyclient = [[CoreTelephonyClient alloc] init];
    	if (!coreTelephonyclient) {
	    EAPLOG_FL(LOG_ERR, "failed to get the CoreTelephonyClient instance");
	    return NULL;
    	}
	if (properties != NULL) {
	    slotUUID = isA_CFString(CFDictionaryGetValue(properties,
							 kCTSimSupportUICCAuthenticationSlotUUIDKey));
	}
	if (slotUUID) {
	    /* carrier Wi-Fi calling case */
	    NSUUID *uuid = [[NSUUID alloc] initWithUUIDString:(__bridge NSString *)slotUUID];
	    preferredSubscriptionCtx = SubscriptionContextMatchingSlotGet(coreTelephonyclient, uuid);
	} else {
	    /* carrier hotspot case */
	    preferredSubscriptionCtx = SubscriptionContextUserPreferredGet(coreTelephonyclient);
	}
	if (!preferredSubscriptionCtx) {
	    EAPLOG_FL(LOG_ERR, "failed to get the preferred subscription context");
	    return NULL;
    	}
	if (preferredSubscriptionCtx) {
	    mcc = [coreTelephonyclient copyMobileSubscriberCountryCode:preferredSubscriptionCtx error:&error];
	    if (error) {
	    	EAPLOG_FL(LOG_ERR,
		      	"CoreTelephonyClient.copyMobileSubscriberCountryCode failed with "
		      	"error: %@", error);
	    	return NULL;
	    }
	    mnc = [coreTelephonyclient copyMobileSubscriberNetworkCode:preferredSubscriptionCtx error:&error];
	    if (error) {
	    	EAPLOG_FL(LOG_ERR,
		      	"CoreTelephonyClient.copyMobileSubscriberNetworkCode failed with "
		      	"error: %@", error);
	    	return NULL;
	    }
	    if (mcc && mnc) {
	    	if (mnc.length == 2) {
		    mnc = [NSString stringWithFormat:@"0%@", mnc];
	    	}
	    	realm = [NSString stringWithFormat:@"wlan.mnc%@.mcc%@.3gppnetwork.org" , mnc, mcc];
	    	return (__bridge_retained CFStringRef)realm;
	    }
    	}
    	return NULL;
    }
}

PRIVATE_EXTERN CFDictionaryRef
_SIMCopyEncryptedIMSIInfo(EAPType type)
{
    @autoreleasepool {
    	NSError 				*error = nil;
    	NSString 				*imsi = nil;
    	NSString 				*imsi_with_prefix = nil;
    	NSString				*imsi_to_encrypt = nil;
    	NSString				*realm = nil;
    	CTXPCServiceSubscriptionContext 	*userPreferredSubscriptionCtx = nil;
    	NSDictionary 			*ret_encrypted = nil;

    	CoreTelephonyClient *coreTelephonyclient = [[CoreTelephonyClient alloc] init];
    	if (!coreTelephonyclient) {
	    EAPLOG_FL(LOG_ERR, "failed to get the CoreTelephonyClient instance");
	    return NULL;
    	}
	userPreferredSubscriptionCtx = SubscriptionContextUserPreferredGet(coreTelephonyclient);
    	if (!userPreferredSubscriptionCtx) {
	    EAPLOG_FL(LOG_ERR, "failed to get the user preferred subscription context");
	    return NULL;
    	}
	imsi = (__bridge_transfer NSString *)_SIMCopyIMSI(NULL);
    	if (!imsi) {
	    return NULL;
    	}
	realm = (__bridge_transfer NSString *)_SIMCopyRealm(NULL);
    	switch(type) {
	    case kEAPTypeEAPSIM:
	    	imsi_with_prefix = [NSString stringWithFormat:@"1%@", imsi];
	    	break;
	    default:
	    case kEAPTypeEAPAKA:
	    	imsi_with_prefix = [NSString stringWithFormat:@"0%@", imsi];
    	}
    	if (realm) {
	    imsi_to_encrypt = [NSString stringWithFormat:@"%@@%@", imsi_with_prefix, realm];
    	} else {
	    imsi_to_encrypt = imsi_with_prefix;
    	}

    	ret_encrypted = [coreTelephonyclient context:userPreferredSubscriptionCtx getEncryptedIdentity:imsi_to_encrypt error:&error];
    	if (error) {
	    EAPLOG_FL(LOG_ERR,
		      "CoreTelephonyClient.getEncryptedIdentity failed with "
		      "error: %@", error);
	    ret_encrypted = nil;
    	}
    	return (__bridge_retained CFDictionaryRef)ret_encrypted;
    }
}

PRIVATE_EXTERN void
_SIMReportDecryptionError(CFDataRef encryptedIdentity)
{
    @autoreleasepool {
    	NSError 				*error = nil;
    	CTXPCServiceSubscriptionContext 	*userPreferredSubscriptionCtx = nil;

    	CoreTelephonyClient *coreTelephonyclient = [[CoreTelephonyClient alloc] init];
    	if (!coreTelephonyclient) {
	    EAPLOG_FL(LOG_ERR, "failed to get the CoreTelephonyClient instance");
	    return;
    	}
	userPreferredSubscriptionCtx = SubscriptionContextUserPreferredGet(coreTelephonyclient);
    	if (!userPreferredSubscriptionCtx) {
	    EAPLOG_FL(LOG_ERR, "failed to get the user preferred subscription context");
	    return;
    	}
    	if (userPreferredSubscriptionCtx) {
	    error = [coreTelephonyclient context:userPreferredSubscriptionCtx evaluateMobileSubscriberIdentity:(__bridge NSData*)encryptedIdentity];
	    if (error) {
	    	EAPLOG_FL(LOG_ERR,
		      	"CoreTelephonyClient.evaluateMobileSubscriberIdentity failed with "
		      	"error: %@", error);
	    }
    	}
    	return;
    }
}

PRIVATE_EXTERN CFDictionaryRef
_SIMCreateAuthResponse(CFStringRef slotUUID, CFDictionaryRef auth_params)
{
    @autoreleasepool {
    	CTXPCServiceSubscriptionContext 	*preferredSubscriptionCtx = nil;
    	__block CFDictionaryRef 		retAuthInfo = NULL;

    	CoreTelephonyClient *coreTelephonyclient = [[CoreTelephonyClient alloc] init];
    	if (!coreTelephonyclient) {
	    EAPLOG_FL(LOG_ERR, "failed to get the CoreTelephonyClient instance");
	    return NULL;
    	}
    	if (slotUUID) {
	    /* carrier Wi-Fi calling case */
	    NSUUID *uuid = [[NSUUID alloc] initWithUUIDString:(__bridge NSString *)slotUUID];
	    preferredSubscriptionCtx = SubscriptionContextMatchingSlotGet(coreTelephonyclient, uuid);
    	} else {
	    /* carrier hotspot case */
	    preferredSubscriptionCtx = SubscriptionContextUserPreferredGet(coreTelephonyclient);
    	}
    	if (preferredSubscriptionCtx) {
	    __block dispatch_semaphore_t	semaphore;

	    semaphore = dispatch_semaphore_create(0);
	    if (semaphore == NULL) {
	    	EAPLOG_FL(LOG_ERR, "dispatch_semaphore_create() failed");
	    	return NULL;
	    }
	    [coreTelephonyclient generateUICCAuthenticationInfo:preferredSubscriptionCtx authParams:(__bridge NSDictionary *)auth_params completion:^(NSDictionary *authInfo, NSError *error) {
	    	if (error) {
		    EAPLOG_FL(LOG_ERR,
			      "CoreTelephonyClient.generateUICCAuthenticationInfo failed with "
			      "error: %@", error);
	    	} else {
		    retAuthInfo = (__bridge_retained CFDictionaryRef)authInfo;
	    	}
	    	dispatch_semaphore_signal(semaphore);
	    }];
	    {
	    	dispatch_time_t	t;
#define WAIT_TIME_SECONDS	20
	    	t = dispatch_time(DISPATCH_TIME_NOW, WAIT_TIME_SECONDS * NSEC_PER_SEC);
	    	if (dispatch_semaphore_wait(semaphore, t) != 0) {
		    EAPLOG_FL(LOG_NOTICE,
			      "timed out while waiting for response");
	    	}
	    }
    	}
    	return retAuthInfo;
    }
}

/**
 ** SIMAuthenticate{SIM,AKA}
 **/

STATIC CFDictionaryRef
make_gsm_request(const uint8_t * rand_p)
{
    CFDictionaryRef	dict;
    const void *	keys[2];
    CFDataRef		rand;
    const void *	values[2];

    rand = CFDataCreate(NULL, rand_p, SIM_RAND_SIZE);
    keys[0] = kCTSimSupportUICCAuthenticationTypeKey;
    values[0] = kCTSimSupportUICCAuthenticationTypeEAPSIM;
    keys[1] = kCTSimSupportUICCAuthenticationRandKey;
    values[1] = rand;

    dict = CFDictionaryCreate(NULL,
			      (const void * * )keys,
			      (const void * *)values,
			      sizeof(keys) / sizeof(keys[0]),
			      &kCFTypeDictionaryKeyCallBacks,
			      &kCFTypeDictionaryValueCallBacks);
    CFRelease(rand);
    return (dict);
}

STATIC CFDictionaryRef
make_aka_request(CFDataRef rand, CFDataRef autn)
{
    const void *	keys[3];
    const void *	values[3];

    keys[0] = kCTSimSupportUICCAuthenticationTypeKey;
    values[0] = kCTSimSupportUICCAuthenticationTypeEAPAKA;
    keys[1] = kCTSimSupportUICCAuthenticationRandKey;
    values[1] = rand;
    keys[2] = kCTSimSupportUICCAuthenticationAutnKey;
    values[2] = autn;

    return (CFDictionaryCreate(NULL,
			       (const void * * )keys,
			       (const void * *)values,
			       sizeof(keys) / sizeof(keys[0]),
			       &kCFTypeDictionaryKeyCallBacks,
			       &kCFTypeDictionaryValueCallBacks));
}

STATIC bool
getKcSRESFromResponse(CFDictionaryRef response,
		      uint8_t * kc_p, uint8_t * sres_p)
{
    CFDataRef		data;
    CFRange		range;
    bool		ret = FALSE;

    /* get the Kc bytes */
    data = CFDictionaryGetValue(response,
				kCTSimSupportUICCAuthenticationKcKey);
    if (data == NULL) {
	data = CFDictionaryGetValue(response,
				    kCTSimSupportSIMAuthenticationKc);
    }
    if (data == NULL || CFDataGetLength(data) != SIM_KC_SIZE) {
	EAPLOG_FL(LOG_NOTICE, "bogus Kc value");
	goto done;
    }
    range = CFRangeMake(0, SIM_KC_SIZE);
    CFDataGetBytes(data, range, kc_p);

    /* get the SRES bytes */
    data = CFDictionaryGetValue(response,
				kCTSimSupportUICCAuthenticationSresKey);
    if (data == NULL) {
	data = CFDictionaryGetValue(response,
				    kCTSimSupportSIMAuthenticationSres);
    }
    if (data == NULL || CFDataGetLength(data) != SIM_SRES_SIZE) {
	EAPLOG_FL(LOG_NOTICE, "bogus SRES value");
	goto done;
    }
    range = CFRangeMake(0, SIM_SRES_SIZE);
    CFDataGetBytes(data, range, sres_p);
    ret = TRUE;

done:
    return (ret);
}

#define N_ATTEMPTS	3

STATIC CFDictionaryRef
SIMCreateAuthResponse(CFStringRef slotUUID, CFDictionaryRef request)
{
    int			i;
    CFDictionaryRef 	response = NULL;

    for (i = 0; i < N_ATTEMPTS; i++) {
	response = _SIMCreateAuthResponse(slotUUID, request);
	if (response) {
	    break;
	}
    }
    return (response);
}

PRIVATE_EXTERN bool
SIMAuthenticateGSM(CFDictionaryRef properties, const uint8_t * rand_p, int count,
		   uint8_t * kc_p, uint8_t * sres_p)
{
    int			i;
    bool		ret = false;
    CFStringRef 	slotUUID = NULL;

    if (properties != NULL) {
	slotUUID = isA_CFString(CFDictionaryGetValue(properties,
						     kCTSimSupportUICCAuthenticationSlotUUIDKey));
    }
    for (i = 0; i < count; i++) {
	CFDictionaryRef	request;
	CFDictionaryRef response = NULL;

	request = make_gsm_request(rand_p + SIM_RAND_SIZE * i);
	response = SIMCreateAuthResponse(slotUUID, request);
	CFRelease(request);
	if (response != NULL) {
	    ret = getKcSRESFromResponse(response,
					kc_p + SIM_KC_SIZE * i,
					sres_p + SIM_SRES_SIZE * i);
	    CFRelease(response);
	}
	else {
	    EAPLOG_FL(LOG_NOTICE, "Could not access SIM");
	    ret = false;
	}
	if (ret == false) {
	    break;
	}
    }
    return (ret);
}

/*
 1) Success, UIM returns Res, Ck, and Ik
 2) Sync failure, UIM returns Auts
 3) Auth Reject, no returned parameters.
 */
STATIC void
AKAAuthResultsSetValuesWithResponse(AKAAuthResultsRef results,
				    CFDictionaryRef response)
{
    CFDataRef		ck;
    CFDataRef		ik;
    CFDataRef		res;

    AKAAuthResultsInit(results);
    ck = CFDictionaryGetValue(response,
			      kCTSimSupportUICCAuthenticationCkKey);
    ik = CFDictionaryGetValue(response,
			      kCTSimSupportUICCAuthenticationIkKey);
    res = CFDictionaryGetValue(response,
			       kCTSimSupportUICCAuthenticationResKey);
    if (ck != NULL && ik != NULL && res != NULL) {
	AKAAuthResultsSetCK(results, ck);
	AKAAuthResultsSetIK(results, ik);
	AKAAuthResultsSetRES(results, res);
    }
    else {
	CFDataRef	auts;

	auts = CFDictionaryGetValue(response,
				    kCTSimSupportUICCAuthenticationAutsKey);
	if (auts != NULL) {
	    AKAAuthResultsSetAUTS(results, auts);
	}
    }
    return;
}

PRIVATE_EXTERN bool
SIMAuthenticateAKA(CFDictionaryRef properties, CFDataRef rand, CFDataRef autn, AKAAuthResultsRef results)
{
    CFDictionaryRef	request;
    CFDictionaryRef 	response = NULL;
    bool		success = false;
    CFStringRef 	slotUUID = NULL;

    AKAAuthResultsInit(results);
    if (properties != NULL) {
	slotUUID = isA_CFString(CFDictionaryGetValue(properties,
						     kCTSimSupportUICCAuthenticationSlotUUIDKey));
    }
    request = make_aka_request(rand, autn);
    response = SIMCreateAuthResponse(slotUUID, request);
    CFRelease(request);
    if (response != NULL) {
	AKAAuthResultsSetValuesWithResponse(results, response);
	CFRelease(response);
	success = true;
    }
    else {
	EAPLOG_FL(LOG_NOTICE, "Could not access SIM");
    }
    return (success);
}

#pragma mark - SIMStatusIndicator Interface

@interface SIMStatusIndicator : NSObject <CoreTelephonyClientSubscriberDelegate> {

}

@property (nonatomic, strong, nullable) CoreTelephonyClient *coreTelephonyClient;
@property (nonatomic, strong) dispatch_queue_t queue;
@property SIMAccessConnectionCallback callback;
@property CFRunLoopRef runloop;
@property CFStringRef runloopMode;
@property CFTypeRef applicationContext;

- (void) createConnection;

@end

#pragma mark -

@implementation SIMStatusIndicator

- (id)init {
    if ((self = [super init])) {
	EAPLOG_FL(LOG_NOTICE, "SIMStatusIndicator initialized.");
    }
    return self;
}

- (void) createConnection {
    self.queue = dispatch_queue_create("SIM status indicator queue", NULL);
    self.coreTelephonyClient = [[CoreTelephonyClient alloc] initWithQueue:_queue];
    self.coreTelephonyClient.delegate = self;
}

#pragma mark CoreTelephonyClientSubscriberDelegate

- (void)simStatusDidChange:(CTXPCServiceSubscriptionContext *)context status:(NSString *)status {
    static NSString *lastStatus = nil;

    if (context && context.userDataPreferred && [context.userDataPreferred boolValue]) {
	if ([status isEqualToString:lastStatus]) {
	    return;
	}
	lastStatus = status;
	CFRunLoopPerformBlock(self.runloop, self.runloopMode,
			  ^{
			      self.callback((__bridge CFTypeRef)self, (__bridge CFStringRef)status, (void *)self.applicationContext);
			  });
	CFRunLoopWakeUp(self.runloop);
    }
}

@end

CFTypeRef
_SIMAccessConnectionCreate(void)
{
    @autoreleasepool {
    	SIMStatusIndicator *status_indicator = [[SIMStatusIndicator alloc] init];
    	[status_indicator createConnection];
    	return CFBridgingRetain(status_indicator);
    }
}

void
_SIMAccessConnectionRegisterForNotification(CFTypeRef connection, SIMAccessConnectionCallback callback, void *info, CFRunLoopRef runLoop,
					    CFStringRef runloopMode) {
    @autoreleasepool {
    	SIMStatusIndicator *indicator = (__bridge SIMStatusIndicator *)connection;
    	indicator.callback = callback;
    	indicator.runloop = runLoop;
    	indicator.runloopMode = runloopMode;
    	indicator.applicationContext = (CFTypeRef)info;
    }
}

#endif /* TARGET_OS_EMBEDDED */

PRIVATE_EXTERN void
AKAAuthResultsSetCK(AKAAuthResultsRef results, CFDataRef ck)
{
    my_FieldSetRetainedCFType(&results->ck, ck);
    return;
}

PRIVATE_EXTERN void
AKAAuthResultsSetIK(AKAAuthResultsRef results, CFDataRef ik)
{
    my_FieldSetRetainedCFType(&results->ik, ik);
    return;
}

PRIVATE_EXTERN void
AKAAuthResultsSetRES(AKAAuthResultsRef results, CFDataRef res)
{
    my_FieldSetRetainedCFType(&results->res, res);
    return;
}

PRIVATE_EXTERN void
AKAAuthResultsSetAUTS(AKAAuthResultsRef results, CFDataRef auts)
{
    my_FieldSetRetainedCFType(&results->auts, auts);
    return;
}

PRIVATE_EXTERN void
AKAAuthResultsInit(AKAAuthResultsRef results)
{
    bzero(results, sizeof(*results));
    return;
}

PRIVATE_EXTERN void
AKAAuthResultsRelease(AKAAuthResultsRef results)
{
    AKAAuthResultsSetCK(results, NULL);
    AKAAuthResultsSetIK(results, NULL);
    AKAAuthResultsSetRES(results, NULL);
    AKAAuthResultsSetAUTS(results, NULL);
    return;
}
