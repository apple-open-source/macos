/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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

#import <FactoryOTAEAPClient/FactoryOTAEAPClient.h>
#import <Security/SecIdentityPriv.h>
#import "FactoryOTAConfigurationAccess.h"
#import "myCFUtil.h"
#import "EAPLog.h"

static FactoryOTAEAPClient *
GetFactoryOTAEAPClient(void)
{
    static dispatch_once_t		once;
    static FactoryOTAEAPClient 		*factoryOTAClient = nil;

    dispatch_once(&once, ^{
	factoryOTAClient = [[FactoryOTAEAPClient alloc] init];
    });
    return (factoryOTAClient);
}

static Boolean
CopyFOTAEAPConfiguration(FactoryOTAEAPConfiguration *configuration, FOTAEAPConfigurationRef eapConfig) {
    if (configuration) {
	if (CFGetTypeID((CFTypeRef)[configuration.tlsCertificateChain firstObject]) != SecCertificateGetTypeID()) {
	    EAPLOG_FL(LOG_ERR, "received invalid client certificate from Factory OTA Client");
	    return FALSE;
	}
	if (CFGetTypeID(configuration.tlsKey) != SecKeyGetTypeID()) {
	    EAPLOG_FL(LOG_ERR, "received invalid client private key from Factory OTA Client");
	    return FALSE;
	}
	bzero(eapConfig, sizeof(*eapConfig));
	SecCertificateRef leaf = (__bridge SecCertificateRef)[configuration.tlsCertificateChain firstObject];
	eapConfig->tlsClientIdentity = SecIdentityCreate(kCFAllocatorDefault, leaf, (SecKeyRef)configuration.tlsKey);
	if (configuration.tlsCertificateChain.count > 1) {
	    CFMutableArrayRef certs = CFArrayCreateMutableCopy(NULL, configuration.tlsCertificateChain.count, (__bridge CFArrayRef)configuration.tlsCertificateChain);
	    CFArrayRemoveValueAtIndex(certs, 0);
	    eapConfig->tlsClientCertificateChain = certs;
	}
    }
    return TRUE;
}

void
FactoryOTAConfigurationAccessFetchEAPConfiguration(FactoryOTAConfigurationAccessCallback callback, void *context) {
    @autoreleasepool {
	FactoryOTAEAPClient *factoryOTAClient = GetFactoryOTAEAPClient();
	[factoryOTAClient eapConfigurationWithCompletion:^(FactoryOTAEAPConfiguration *configuration, NSError *error) {
	    FOTAEAPConfiguration eapConfig;
	    Boolean success = FALSE;
	    if (error) {
		EAPLOG_FL(LOG_ERR, "failed to fetch EAP configuration from Factory OTA Client, error: %@", error);
	    } else {
		EAPLOG_FL(LOG_NOTICE, "received EAP configuration from Factory OTA Client");
		success = CopyFOTAEAPConfiguration(configuration, &eapConfig);
	    }
	    if (callback) {
		FOTAEAPConfigurationRef eapConfigRef = success ? &eapConfig : NULL;
		callback(context, eapConfigRef);
		if (eapConfigRef != NULL) {
		    my_CFRelease(&eapConfigRef->tlsClientIdentity);
		    my_CFRelease(&eapConfigRef->tlsClientCertificateChain);
		}
	    }
	}];
    }
}

bool
FactoryOTAConfigurationAccessIsInFactoryMode(void) {
    @autoreleasepool {
	FactoryOTAEAPClient *factoryOTAClient = GetFactoryOTAEAPClient();
	return ([factoryOTAClient isInFactoryMode] == YES);
    }
}

#endif /* TARGET_OS_IOS && !TARGET_OS_VISION */
