/*
 * Copyright (c) 2022, 2024 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>

#if TARGET_OS_IOS && !TARGET_OS_VISION

#import <MobileInBoxUpdate/MobileInBoxUpdate.h>
#import <MobileInBoxUpdate/MIBUClient.h>
#import <Security/SecIdentityPriv.h>
#import "MIBConfigurationAccess.h"
#import "myCFUtil.h"
#import "EAPLog.h"

static MIBUClient *
GetMIBUClient(void)
{
    static dispatch_once_t	once;
    static MIBUClient 		*mibuClient = nil;

    dispatch_once(&once, ^{
	mibuClient = [[MIBUClient alloc] init];
    });
    return (mibuClient);
}

static MIBEAPConfigurationRef
MIBConfigurationConvertToMIBEAPConfiguration(MIBUEAPConfiguartion *configuration) {
    MIBEAPConfigurationRef eapConfig = NULL;
    if (configuration) {
	if (CFGetTypeID((CFTypeRef)[configuration.tlsCertificateChain firstObject]) != SecCertificateGetTypeID()) {
	    EAPLOG_FL(LOG_ERR, "received invalid client certificate from MIB");
	    return NULL;
	}
	if (CFGetTypeID(configuration.tlsKey) != SecKeyGetTypeID()) {
	    EAPLOG_FL(LOG_ERR, "received invalid client private key from MIB");
	    return NULL;
	}
	eapConfig = (MIBEAPConfigurationRef)malloc(sizeof(*eapConfig));
	bzero(eapConfig, sizeof(*eapConfig));
	SecCertificateRef leaf = (__bridge SecCertificateRef)[configuration.tlsCertificateChain firstObject];
	eapConfig->tlsClientIdentity = SecIdentityCreate(kCFAllocatorDefault, leaf, (SecKeyRef)configuration.tlsKey);
	if (configuration.tlsCertificateChain.count > 1) {
	    CFMutableArrayRef certs = CFArrayCreateMutableCopy(NULL, configuration.tlsCertificateChain.count, (__bridge CFArrayRef)configuration.tlsCertificateChain);
	    CFArrayRemoveValueAtIndex(certs, 0);
	    eapConfig->tlsClientCertificateChain = certs;
	}
    }
    return eapConfig;
}

void
MIBConfigurationAccessFetchEAPConfiguration(MIBConfigurationAccessCallback callback, void *context) {
    @autoreleasepool {
	MIBUClient *mibuClient = GetMIBUClient();
	[mibuClient eapConfigurationWithCompletion:^(MIBUEAPConfiguartion *configuration, NSError *error) {
	    MIBEAPConfigurationRef eapConfig = NULL;
	    if (error) {
		EAPLOG_FL(LOG_ERR, "failed to fetch EAP configuration from MIB, error: %@", error);
	    } else {
		eapConfig = MIBConfigurationConvertToMIBEAPConfiguration(configuration);
	    }
	    dispatch_async(dispatch_get_main_queue(), ^{
		if (callback) {
		    callback(context, eapConfig);
		}
	    });
	}];
    }
}

bool
MIBConfigurationAccessIsInBoxUpdateMode(void) {
    @autoreleasepool {
	MIBUClient *mibuClient = GetMIBUClient();
	return ([mibuClient isInBoxUpdateMode:nil] == YES);
    }
}

#endif /* TARGET_OS_IOS && !TARGET_OS_VISION */
