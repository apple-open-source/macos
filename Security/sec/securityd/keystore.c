/*
 * Copyright (c) 2010 Apple Inc. All Rights Reserved.
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
 * keystore.c - C API for the AppleKeyStore kext
 */

#include <securityd/keystore.h>

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <CoreFoundation/CFData.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonCryptor.h>
#include <libkern/OSByteOrder.h>
#include <security_utilities/debugging.h>
#include <assert.h>
#include <Security/SecInternal.h>
#include <TargetConditionals.h>
#include <AssertMacros.h>
#include <asl.h>

#if TARGET_OS_EMBEDDED && !TARGET_IPHONE_SIMULATOR
#define USE_KEYSTORE  1
#define USE_DISPATCH  1 /* Shouldn't be here. */
#else /* No AppleKeyStore.kext on this OS. */
#define USE_KEYSTORE  0
#endif /* hardware aes */

#if USE_KEYSTORE
#include <Kernel/IOKit/crypto/AppleKeyStoreDefs.h>

#if USE_DISPATCH
#include <dispatch/dispatch.h>
static dispatch_once_t ks_init_once;
#else /* !USE_DISPATCH use pthreads instead. */
#include <pthread.h>
static pthread_once_t ks_init_once = PTHREAD_ONCE_INIT;
#endif

static io_connect_t ks_connect_handle = MACH_PORT_NULL;

static void ks_service_matching_callback(void *refcon, io_iterator_t iterator)
{
	io_object_t obj = IO_OBJECT_NULL;

	while ((obj = IOIteratorNext(iterator))) {
        kern_return_t ret = IOServiceOpen(obj, mach_task_self(), 0,
            (io_connect_t*)refcon);
        if (ret) {
            asl_log(NULL, NULL, ASL_LEVEL_ERR,
                "IOServiceOpen() failed: %d", ret);
        }
        IOObjectRelease(obj);
	}
}

io_connect_t ks_connect_to_service(const char *className)
{
	kern_return_t kernResult;
    io_connect_t connect = IO_OBJECT_NULL;
	io_iterator_t iterator = IO_OBJECT_NULL;
    IONotificationPortRef notifyport = NULL;
	CFDictionaryRef	classToMatch;

	if ((classToMatch = IOServiceMatching(className)) == NULL) {
		asl_log(NULL, NULL, ASL_LEVEL_ERR,
            "IOServiceMatching failed for '%s'", className);
		return connect;
	}

    /* consumed by IOServiceGetMatchingServices, we need it if that fails. */
    CFRetain(classToMatch);
    kernResult = IOServiceGetMatchingServices(kIOMasterPortDefault,
        classToMatch, &iterator);

    if (kernResult == KERN_SUCCESS) {
        CFRelease(classToMatch);
    } else {
        asl_log(NULL, NULL, ASL_LEVEL_WARNING,
            "IOServiceGetMatchingServices() failed %d", kernResult);

        notifyport = IONotificationPortCreate(kIOMasterPortDefault);
        if (!notifyport) {
            asl_log(NULL, NULL, ASL_LEVEL_ERR,
                "IONotificationPortCreate() failed");
            return connect;
        }

        kernResult = IOServiceAddMatchingNotification(notifyport,
            kIOFirstMatchNotification, classToMatch,
            ks_service_matching_callback, &connect, &iterator);
        if (kernResult) {
            asl_log(NULL, NULL, ASL_LEVEL_ERR,
                "IOServiceAddMatchingNotification() failed: %d", kernResult);
            return connect;
        }
    }

    /* Check whether it was already there before we registered for the
       notification. */
    ks_service_matching_callback(&connect, iterator);

    if (notifyport) {
        /* We'll get set up to wait for it to appear */
        if (connect == IO_OBJECT_NULL) {
            asl_log(NULL, NULL, ASL_LEVEL_ERR,
                "Waiting for %s to show up.", className);
            CFStringRef mode = CFSTR("WaitForCryptoService");
            CFRunLoopAddSource(CFRunLoopGetCurrent(),
                IONotificationPortGetRunLoopSource(notifyport), mode);
            CFRunLoopRunInMode(mode, 30.0, true);
            if (connect == MACH_PORT_NULL)
                asl_log(NULL, NULL, ASL_LEVEL_ERR, "Cannot find %s", className);
        }
        IONotificationPortDestroy(notifyport);
    }

    IOObjectRelease(iterator);

    if (connect != IO_OBJECT_NULL) {
        secdebug("iokit", "obtained connection for '%s'", className);
    }
    return connect;
}

#if USE_DISPATCH
static void ks_crypto_init(void *unused)
#else
static void ks_crypto_init(void)
#endif
{
    ks_connect_handle = ks_connect_to_service(kAppleKeyStoreServiceName);
}

io_connect_t ks_get_connect(void)
{
#if USE_DISPATCH
    dispatch_once_f(&ks_init_once, NULL, ks_crypto_init);
#else
    pthread_once(&ks_init_once, ks_crypto_init);
#endif
    return ks_connect_handle;
}

bool ks_available(void)
{
    ks_get_connect();
	return true;
}


static bool ks_crypt(uint32_t selector, uint64_t keybag,
    uint64_t keyclass, const uint8_t *input, size_t inputLength,
    uint8_t *buffer, size_t *keySize) {
	kern_return_t kernResult;

    if (!ks_connect_handle) {
        secdebug("ks", "AppleKeyStore.kext not found");
        return false;
    }

    uint64_t inputs[] = { keybag, keyclass };
    uint32_t num_inputs = sizeof(inputs)/sizeof(*inputs);
    kernResult = IOConnectCallMethod(ks_connect_handle, selector,
        inputs, num_inputs, input, inputLength, NULL, NULL, buffer,
        keySize);

	if (kernResult != KERN_SUCCESS) {
		asl_log(NULL, NULL, ASL_LEVEL_ERR, "kAppleKeyStore selector(%d): %d",
            selector, kernResult);
		return false;
	}
	return true;
}

void ks_free(ks_object_t object) {
    /* TODO: this might need to be vm_deallocate in some cases. */
    free(object._kso);
}

uint8_t *ks_unwrap(uint64_t keybag, uint64_t keyclass,
    const uint8_t *wrappedKey, size_t wrappedKeySize,
    uint8_t *buffer, size_t bufferSize, size_t *keySize) {
    *keySize = bufferSize;
    if (!ks_crypt(kAppleKeyStoreKeyUnwrap,
        keybag, keyclass, wrappedKey, wrappedKeySize, buffer, keySize))
        return NULL;
    return buffer;
}


uint8_t *ks_wrap(uint64_t keybag, uint64_t keyclass,
    const uint8_t *key, size_t keyByteSize,
    uint8_t *buffer, size_t bufferSize, size_t *wrappedKeySize) {
    *wrappedKeySize = bufferSize;
    if (ks_crypt(kAppleKeyStoreKeyWrap,
        keybag, keyclass, key, keyByteSize,
        buffer, wrappedKeySize))
        return NULL;
    return buffer;
}


#else /* !USE_KEYSTORE */

bool ks_available(void)
{
	return false;
}

#endif /* !USE_KEYSTORE */
