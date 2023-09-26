/*
 * Copyright (c) 2016-2022 Apple Inc. All rights reserved.
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

#import "SCTest.h"
#import "SCTestUtils.h"
#import "scdtest.h"

/*
 * SCDTEST_READ_UNRESTRICTED_KEY
 * - key will be present and readable
 *
 * SCDTEST_READ_UNRESTRICTED_NOT_PRESENT_KEY
 * - key will not be present
 */
#define SCDTEST_READ_UNRESTRICTED_KEY				\
	CFSTR(SCDTEST_PREFIX "read-unrestricted-present.key")
#define SCDTEST_READ_UNRESTRICTED_NOT_PRESENT_KEY		\
	CFSTR(SCDTEST_PREFIX "read-unrestricted-not-present.key")
#if !TARGET_OS_BRIDGE
static void
add_to_dict_and_array(CFMutableDictionaryRef dict,
		      CFMutableArrayRef array,
		      CFStringRef key)
{
	CFDictionarySetValue(dict, key, kCFBooleanTrue);
	CFArrayAppendValue(array, key);
}
#endif
@interface SCTestDynamicStore : SCTest
@property SCDynamicStoreRef store;
@property dispatch_semaphore_t sem;
@property int counter;
@end

@implementation SCTestDynamicStore

+ (NSString *)command
{
	return @"dynamic_store";
}

+ (NSString *)commandDescription
{
	return @"Tests the SCDynamicStore code path";
}

- (instancetype)initWithOptions:(NSDictionary *)options
{
	self = [super initWithOptions:options];
	if (self) {
		_store = SCDynamicStoreCreate(kCFAllocatorDefault,
						  CFSTR("SCTest"),
						  NULL,
						  NULL);
#if !TARGET_OS_BRIDGE
		// We want to send requests to a non-existent dynamic store in bridgeOS.
		if (_store == NULL) {
			SCTestLog("Could not create session");
			ERR_EXIT;
		}
#endif
	}
	return self;
}

- (void)dealloc
{
	if (self.store != NULL) {
		CFRelease(self.store);
		self.store = NULL;
	}
}

- (void)start
{
	CFStringRef key = NULL;
	CFPropertyListRef value = NULL;

	if (self.options[kSCTestDynamicStoreOptionDNS]) {
		key = SCDynamicStoreKeyCreateNetworkGlobalEntity(kCFAllocatorDefault, kSCDynamicStoreDomainState, kSCEntNetDNS);
		value = SCDynamicStoreCopyValue(self.store, key);
		SCTestLog("%@ : %@", key, value);
		CFRelease(key);
		if (value != NULL) {
			CFRelease(value);
		}
	}
	if (self.options[kSCTestDynamicStoreOptionIPv4]) {
		key = SCDynamicStoreKeyCreateNetworkGlobalEntity(kCFAllocatorDefault, kSCDynamicStoreDomainState, kSCEntNetIPv4);
		value = SCDynamicStoreCopyValue(self.store, key);
		SCTestLog("%@ : %@", key, value);
		CFRelease(key);
		if (value != NULL) {
			CFRelease(value);
		}
	}
	if (self.options[kSCTestDynamicStoreOptionIPv6]) {
		key = SCDynamicStoreKeyCreateNetworkGlobalEntity(kCFAllocatorDefault, kSCDynamicStoreDomainState, kSCEntNetIPv6);
		value = SCDynamicStoreCopyValue(self.store, key);
		SCTestLog("%@ : %@", key, value);
		CFRelease(key);
		if (value != NULL) {
			CFRelease(value);
		}
	}
	if (self.options[kSCTestDynamicStoreOptionProxies]) {
		key = SCDynamicStoreKeyCreateNetworkGlobalEntity(kCFAllocatorDefault, kSCDynamicStoreDomainState, kSCEntNetProxies);
		value = SCDynamicStoreCopyValue(self.store, key);
		SCTestLog("%@ : %@", key, value);
		CFRelease(key);
		if (value != NULL) {
			CFRelease(value);
		}
	}

	[self cleanupAndExitWithErrorCode:0];
}

- (void)cleanupAndExitWithErrorCode:(int)error
{
	[super cleanupAndExitWithErrorCode:error];
}

- (BOOL)setup
{
	return YES;
}

- (BOOL)unitTest
{
	BOOL allUnitTestsPassed = YES;

	if(![self setup]) {
		return NO;
	}

#if !TARGET_OS_BRIDGE
	allUnitTestsPassed &= [self unitTestSetAndRemoveValue];
	allUnitTestsPassed &= [self unitTestCopyMultiple];
	allUnitTestsPassed &= [self unitTestSCDynamicStoreCallbackStress];
	allUnitTestsPassed &= [self unitTestSCDynamicStoreSetMultipleStress];
#else // !TARGET_OS_BRIDGE
	allUnitTestsPassed &= [self unitTestSCDynamicStoreCopyValueReturnsNullOnBridgeOS];
#endif // !TARGET_OS_BRIDGE

	if(![self tearDown]) {
		return NO;
	}

	return allUnitTestsPassed;
}

- (BOOL)tearDown
{
	return YES;
}

#if !TARGET_OS_BRIDGE
#pragma mark -
#pragma mark Non-BridgeOS SCDynamicStore Unit Tests

- (BOOL)unitTestSetAndRemoveValue
{
	int iterations = 1000;
	NSDictionary *bogusValue;
	SCTestDynamicStore *test;

	test = [[SCTestDynamicStore alloc] initWithOptions:self.options];
	bogusValue = @{@"Pretty":@"Useless"};

	for (int i = 0; i < iterations; i++) {
		NSUUID *uuid = [NSUUID UUID];
		CFStringRef key;
		BOOL ok;

		key = SCDynamicStoreKeyCreateNetworkServiceEntity(kCFAllocatorDefault,
								  CFSTR("SCTest"),
								  (__bridge CFStringRef)uuid.UUIDString,
								  kSCEntNetDNS);
		ok = SCDynamicStoreSetValue(test.store, key, (__bridge CFDictionaryRef)bogusValue);
		if (!ok) {
			SCTestLog("Failed to set value in SCDynamicStore. Error: %s", SCErrorString(SCError()));
			CFRelease(key);
			return NO;
		}

		ok = SCDynamicStoreRemoveValue(test.store, key);
		if (!ok) {
			SCTestLog("Failed to remove value from SCDynamicStore. Error: %s", SCErrorString(SCError()));
			CFRelease(key);
			return NO;
		}

		CFRelease(key);
	}

	SCTestLog("Successfully completed setAndRemove unit test");
	return YES;
}

- (BOOL)unitTestCopyMultiple
{
	NSString *pattern;
	SCTestDynamicStore *test;
	CFArrayRef keyList;
	int iterations = 1000;

	test = [[SCTestDynamicStore alloc] initWithOptions:self.options];
	pattern = (__bridge_transfer NSString *)SCDynamicStoreKeyCreate(kCFAllocatorDefault,
									CFSTR("%@/%@/%@/%@/%@"),
									kSCDynamicStoreDomainState,
									kSCCompNetwork,
									kSCCompInterface,
									kSCCompAnyRegex,
									kSCCompAnyRegex);

	keyList = SCDynamicStoreCopyKeyList(test.store, (__bridge CFStringRef)pattern);
	for (int i = 0; i < iterations; i++) {
		CFDictionaryRef value = SCDynamicStoreCopyMultiple(test.store, keyList, NULL);
		if (value == NULL) {
			SCTestLog("Failed to copy multiple values from SCDynamicStore. Error: %s", SCErrorString(SCError()));
			CFRelease(keyList);
			return NO;
		}
		CFRelease(value);
	}
	CFRelease(keyList);

	pattern = (__bridge_transfer NSString *)SCDynamicStoreKeyCreate(kCFAllocatorDefault,
									CFSTR("%@/%@/%@/%@/%@"),
									kSCDynamicStoreDomainSetup,
									kSCCompNetwork,
									kSCCompService,
									kSCCompAnyRegex,
									kSCCompAnyRegex);

	keyList = SCDynamicStoreCopyKeyList(test.store, (__bridge CFStringRef)pattern);
	for (int i = 0; i < iterations; i++) {
		CFDictionaryRef value = SCDynamicStoreCopyMultiple(test.store, keyList, NULL);
		if (value == NULL) {
			SCTestLog("Failed to copy multiple values from SCDynamicStore. Error: %s", SCErrorString(SCError()));
			CFRelease(keyList);
			return NO;
		}
		CFRelease(value);
	}
	CFRelease(keyList);

	SCTestLog("Successfully completed copyMultiple unit test");
	return YES;
}

static void
myTestCallback(SCDynamicStoreRef store, CFArrayRef changedKeys, void *ctx)
{
#pragma unused(store)
#pragma unused(changedKeys)
	SCTestDynamicStore *test = (__bridge SCTestDynamicStore *)ctx;
	test.counter++;
	if (test.sem != NULL) {
		dispatch_semaphore_signal(test.sem);
	}
}

- (BOOL)unitTestSCDynamicStoreCallbackStress
{
	SCTestDynamicStore *test;
	int iterations = 100;
	NSString *testKey = @"SCTest:/myTestKey";
	BOOL ok;
	dispatch_queue_t callbackQ;
	SCDynamicStoreContext ctx;

	test = [[SCTestDynamicStore alloc] initWithOptions:self.options];
	if (test.store != NULL) {
		CFRelease(test.store);
		test.store = NULL;
	}

	ctx = (SCDynamicStoreContext){0, (__bridge void * _Nullable)(test), CFRetain, CFRelease, NULL};
	test.store = SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("SCTest"), myTestCallback, &ctx);

	ok = SCDynamicStoreSetNotificationKeys(test.store, (__bridge CFArrayRef)@[testKey], NULL);
	if (!ok) {
		SCTestLog("Failed to set notification keys. Error: %s", SCErrorString(SCError()));
		return NO;
	}

	callbackQ = dispatch_queue_create("SCTestDynamicStore callback queue", NULL);
	ok = SCDynamicStoreSetDispatchQueue(test.store, callbackQ);
	if (!ok) {
		SCTestLog("Failed to set dispatch queue. Error: %s", SCErrorString(SCError()));
		return NO;
	}

	for (int i = 0; i < iterations; i++) {
		NSUUID *uuid = [NSUUID UUID];
		ok = SCDynamicStoreSetValue(test.store, (__bridge CFStringRef)testKey, (__bridge CFStringRef)uuid.UUIDString);
		if (!ok) {
			SCTestLog("Failed to set value. Error: %s", SCErrorString(SCError()));
			return NO;
		}
		// Perform a write to Dynamic Store every 10ms.
		[test waitFor:0.01];
	}

	ok = SCDynamicStoreRemoveValue(test.store, (__bridge CFStringRef)testKey);
	if (!ok) {
		SCTestLog("Failed to remove value. Error: %s", SCErrorString(SCError()));
		return NO;
	}

	if (test.counter < iterations) {
		// Not all callbacks were received
		SCTestLog("Not all SCDynamicStore callbacks were received (%d/%d). Something went wrong", test.counter, iterations);
		return NO;
	}

	SCTestLog("Successfully completed SCDynamicStore callbacks unit test");
	return YES;
}

- (BOOL)unitTestSCDynamicStoreSetMultipleStress
{
	SCTestDynamicStore *test;
	int iterations = 100;
	NSMutableArray *testKeyArray;
	NSMutableDictionary *testSetDictionary;
	int expectedCallbackCount = 0;
	BOOL ok;
	dispatch_queue_t callbackQ;
	const uint8_t waitTime = 1; // second
	SCDynamicStoreContext ctx;

	test = [[SCTestDynamicStore alloc] initWithOptions:self.options];
	if (test.store != NULL) {
		CFRelease(test.store);
		test.store = NULL;
	}

	ctx = (SCDynamicStoreContext){0, (__bridge void * _Nullable)(test), CFRetain, CFRelease, NULL};
	test.store = SCDynamicStoreCreate(kCFAllocatorDefault, CFSTR("SCTest"), myTestCallback, &ctx);
	test.sem = dispatch_semaphore_create(0);

	testKeyArray = [[NSMutableArray alloc] init];

	for (int i = 0; i < iterations; i++) {
		NSUUID *uuid = [NSUUID UUID];
		NSString *str = [NSString stringWithFormat:@"SCTest:/%@", uuid];
		[testKeyArray addObject:str];
	}

	testSetDictionary = [[NSMutableDictionary alloc] init];
	for (NSString *key in testKeyArray) {
		NSUUID *uuid = [NSUUID UUID];
		[testSetDictionary setObject:@[uuid.UUIDString] forKey:key];
	}

	callbackQ = dispatch_queue_create("SCTestDynamicStore callback queue", NULL);
	ok = SCDynamicStoreSetNotificationKeys(test.store, (__bridge CFArrayRef)testKeyArray, NULL);
	if (!ok) {
		SCTestLog("Failed to set notification keys. Error: %s", SCErrorString(SCError()));
		return NO;
	}

	ok = SCDynamicStoreSetDispatchQueue(test.store, callbackQ);
	if (!ok) {
		SCTestLog("Failed to set dispatch queue. Error: %s", SCErrorString(SCError()));
		return NO;
	}

	ok = SCDynamicStoreSetMultiple(test.store, (__bridge CFDictionaryRef)testSetDictionary, NULL, NULL);
	if (!ok) {
		SCTestLog("Failed to set multiple keys. Error: %s", SCErrorString(SCError()));
		return NO;
	} else {
		expectedCallbackCount++; // One callback for set multiple
	}

	if(dispatch_semaphore_wait(test.sem, dispatch_time(DISPATCH_TIME_NOW, waitTime * NSEC_PER_SEC))) {
		SCTestLog("Failed to get a callback when multiple keys are set");
		return NO;
	}

	ok = SCDynamicStoreSetMultiple(test.store, NULL, NULL, (__bridge CFArrayRef)testKeyArray);
	if (!ok) {
		SCTestLog("Failed to set multiple keys. Error: %s", SCErrorString(SCError()));
		return NO;
	} else {
		expectedCallbackCount++; // One callback for fake notification
	}

	if(dispatch_semaphore_wait(test.sem, dispatch_time(DISPATCH_TIME_NOW, waitTime * NSEC_PER_SEC))) {
		SCTestLog("Failed to get a callback when multiple keys are notified");
		return NO;
	}

	ok = SCDynamicStoreSetMultiple(test.store, NULL, (__bridge CFArrayRef)testKeyArray, NULL);
	if (!ok) {
		SCTestLog("Failed to set multiple keys. Error: %s", SCErrorString(SCError()));
		return NO;
	} else {
		expectedCallbackCount++; // One callback for key removal
	}

	if(dispatch_semaphore_wait(test.sem, dispatch_time(DISPATCH_TIME_NOW, waitTime * NSEC_PER_SEC))) {
		SCTestLog("Failed to get a callback when multiple keys are removed");
		return NO;
	}

	if (test.counter < expectedCallbackCount) {
		// Not all callbacks were received
		SCTestLog("Not all SCDynamicStore callbacks were received (%d/%d). Something went wrong", test.counter, expectedCallbackCount);
		return NO;
	}

	SCTestLog("Successfully completed SCDynamicStore set multiple unit test");
	return YES;

}

- (BOOL)unitTestSCDynamicStoreAccessControls
{
	CFStringRef		key;
	CFMutableArrayRef	keysToCleanup;
	CFMutableArrayRef	keysToRemove;
	CFMutableDictionaryRef	keysToSet;
	BOOL			ok;
	SCTestDynamicStore	*test;
	CFTypeRef		val;

	test = [[SCTestDynamicStore alloc] initWithOptions:self.options];

	/* read-deny */
	key = SCDTEST_READ_DENY1_KEY;
	ok = SCDynamicStoreSetValue(test.store, key, kCFBooleanTrue);
	if (ok) {
		val = SCDynamicStoreCopyValue(test.store, key);
		if (val != NULL) {
			/* have deny entitlement, should have failed */
			CFRelease(val);
			ok = NO;
		}
	}
	(void) SCDynamicStoreRemoveValue(test.store, key);
	if (!ok) {
		SCTestLog("read-deny 1: access to %@ wasn't blocked",
			  key);
		return NO;
	}

	key = SCDTEST_READ_DENY2_KEY;
	ok = SCDynamicStoreSetValue(test.store, key, kCFBooleanTrue);
	if (ok) {
		val = SCDynamicStoreCopyValue(test.store, key);
		if (val != NULL) {
			CFRelease(val);
		} else {
			/* don't have deny entitlement, should have worked */
			ok = NO;
		}
	}
	(void) SCDynamicStoreRemoveValue(test.store, key);
	if (!ok) {
		SCTestLog("read-deny 2: access to %@ should have been allowed",
			  key);
		return NO;
	}

	/* read-allow */
	key = SCDTEST_READ_ALLOW1_KEY;
	ok = SCDynamicStoreSetValue(test.store, key, kCFBooleanTrue);
	if (ok) {
		val = SCDynamicStoreCopyValue(test.store, key);
		if (val != NULL) {
			/* we don't have entitlement, should have failed */
			CFRelease(val);
			ok = NO;
		}
	}
	(void) SCDynamicStoreRemoveValue(test.store, key);
	if (!ok) {
		SCTestLog("read-allow 1: access to %@ wasn't blocked",
			  key);
		return NO;
	}

	key = SCDTEST_READ_ALLOW2_KEY;
	ok = SCDynamicStoreSetValue(test.store, key, kCFBooleanTrue);
	if (ok) {
		val = SCDynamicStoreCopyValue(test.store, key);
		if (val != NULL) {
			CFRelease(val);
		} else {
			/* we have entitlement, should have worked */
			ok = NO;
		}
	}
	(void) SCDynamicStoreRemoveValue(test.store, key);
	if (!ok) {
		SCTestLog("read-allow 2: access to %@ should have been allowed",
			  key);
		return NO;
	}

	/* write-protect */
	key = SCDTEST_WRITE_PROTECT1_KEY;
	ok = SCDynamicStoreSetValue(test.store, key, kCFBooleanTrue);
	(void) SCDynamicStoreRemoveValue(test.store, key);
	if (ok) {
		SCTestLog("write-protect 1: access to %@ wasn't blocked",
			  key);
		return NO;
	}

	key = SCDTEST_WRITE_PROTECT2_KEY;
	ok = SCDynamicStoreSetValue(test.store, key, kCFBooleanTrue);
	(void) SCDynamicStoreRemoveValue(test.store, key);
	if (!ok) {
		SCTestLog("write-protect 2: access to %@ was blocked",
			  key);
		return NO;
	}

	/* Verify CopyMultiple */
	keysToSet = CFDictionaryCreateMutable(NULL, 0,
					      &kCFTypeDictionaryKeyCallBacks,
					      &kCFTypeDictionaryValueCallBacks);
	keysToCleanup = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	add_to_dict_and_array(keysToSet, keysToCleanup,
			      SCDTEST_READ_UNRESTRICTED_KEY);
	add_to_dict_and_array(keysToSet, keysToCleanup, SCDTEST_READ_DENY1_KEY);
	add_to_dict_and_array(keysToSet, keysToCleanup, SCDTEST_READ_DENY2_KEY);
	add_to_dict_and_array(keysToSet, keysToCleanup, SCDTEST_READ_ALLOW1_KEY);
	add_to_dict_and_array(keysToSet, keysToCleanup, SCDTEST_READ_ALLOW2_KEY);
	keysToRemove = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFArrayAppendValue(keysToRemove, SCDTEST_READ_UNRESTRICTED_NOT_PRESENT_KEY);
	ok = SCDynamicStoreSetMultiple(test.store, keysToSet, keysToRemove,
				       NULL);
	if (!ok) {
		SCTestLog("SCDynamicStoreSetMultiple set %@ remove %@ "
			  " failed, %s",
			  keysToSet, keysToRemove,
			  SCErrorString(SCError()));
	}
	else {
		CFIndex			count;
		CFDictionaryRef		dict;
		CFMutableArrayRef	keys;
		CFMutableArrayRef	patterns;

		keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		CFArrayAppendValue(keys, SCDTEST_READ_UNRESTRICTED_NOT_PRESENT_KEY);
		patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		CFArrayAppendValue(patterns, SCDTEST_READ_PATTERN);
		dict = SCDynamicStoreCopyMultiple(test.store, keys, patterns);
		CFRelease(keys);
		CFRelease(patterns);
		if (dict != NULL) {
			count = CFDictionaryGetCount(dict);
		}
		else {
			SCTestLog("CopyMultiple returned NULL, %s",
				  SCErrorString(SCError()));
			count = 0;
		}
#define N_KEYS	3
		if (count != N_KEYS) {
			SCTestLog("Count is %ld, should be %d", (long)count,
				  N_KEYS);
			ok = NO;
		}
		else {
			/* verify expected keys */
			CFStringRef	keys[N_KEYS] = {
				 SCDTEST_READ_UNRESTRICTED_KEY,
				 SCDTEST_READ_DENY2_KEY,
				 SCDTEST_READ_ALLOW2_KEY,
			};

			for (CFIndex i = 0; i < count; i++) {
				if (!CFDictionaryContainsKey(dict, keys[i])) {
					SCTestLog("Missing %@", keys[i]);
					ok = FALSE;
				}
			}
		}
		if (dict != NULL) {
			CFRelease(dict);
		}
	}
	(void) SCDynamicStoreSetMultiple(test.store, NULL, keysToCleanup, NULL);
	CFRelease(keysToSet);
	CFRelease(keysToRemove);
	CFRelease(keysToCleanup);
	if (!ok) {
		SCTestLog("CopyMultiple: failed");
		return NO;
	}

	SCTestLog("Successfully completed SCDynamicStore access control test");
	return YES;

}

#else // !TARGET_OS_BRIDGE
#import <AssertMacros.h>

#pragma mark -
#pragma mark BridgeOS SCDynamicStore Unit Test

- (BOOL)unitTestSCDynamicStoreCopyValueReturnsNullOnBridgeOS
{
	BOOL			ret = NO;
	CFUUIDRef		uuid = NULL;
	CFStringRef		uuidString = NULL;
	CFStringRef		testKey = NULL;
	CFPropertyListRef	plist = NULL;

	uuid = CFUUIDCreate(kCFAllocatorDefault);
	require_quiet(uuid, exit);
	uuidString = CFUUIDCreateString(kCFAllocatorDefault, uuid);
	require_quiet(uuidString, exit);
	testKey = SCDynamicStoreKeyCreateNetworkServiceEntity(kCFAllocatorDefault, CFSTR("SCTest"), uuidString, kSCEntNetDNS);
	require_quiet(testKey, exit);

	plist = SCDynamicStoreCopyValue(NULL, testKey);
	require_action_quiet(plist == NULL,
			     exit,
			     SCTestLog("FAILURE: %@, failed to get a NULL response from the SCDynamicStore on bridgeOS.",
				       NSStringFromSelector(_cmd)));
	SCTestLog("SUCCESS: %@", NSStringFromSelector(_cmd));
	ret = YES;

exit:
	if (uuid != NULL) CFRelease(uuid);
	if (uuidString != NULL) CFRelease(uuidString);
	if (testKey != NULL) CFRelease(testKey);
	if (plist != NULL) CFRelease(plist);
	return ret;
}

#endif // !TARGET_OS_BRIDGE

@end
