/*
 * Copyright (c) 2007-2019 Apple Inc. All Rights Reserved.
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

#import "SecItemServer+SWC.h"

#if TARGET_OS_IOS && !TARGET_OS_BRIDGE
#import <SharedWebCredentials/SharedWebCredentials.h>

#import "utilities/debugging.h"
#import "utilities/SecCFError.h"

// Methods in this category are here temporarily until I can add SPI in
// _SWCServiceSpecifier for the function below to consume.
@interface NSObject (Workaround61065225)
- (NSString *)domainHost;
@end

SecSWCFlags _SecAppDomainApprovalStatus(CFStringRef appID, CFStringRef fqdn, CFErrorRef *error)
{
	__block SecSWCFlags flags = kSecSWCFlags_None;

	@autoreleasepool {
		secnotice("swc", "Application %@ is requesting approval for %@", appID, fqdn);

		_SWCServiceSpecifier *specifier = [[_SWCServiceSpecifier alloc] initWithServiceType:_SWCServiceTypeWebCredentials
																	  applicationIdentifier:(__bridge NSString *)appID
																					 domain:(__bridge NSString *)fqdn];
		NSError *err = nil;
		NSArray<_SWCServiceDetails *> *allDetails = [_SWCServiceDetails serviceDetailsWithServiceSpecifier:specifier error:&err];
		if (allDetails) {
			_SWCServiceDetails *details = allDetails.firstObject;
			if (details) {
				switch (details.userApprovalState) {
				case SWCServiceApprovalStateApproved:
					flags |= kSecSWCFlag_UserApproved;
					break;

				case SWCServiceApprovalStateDenied:
					flags |= kSecSWCFlag_UserDenied;
					break;

				default:
					break;
				}
				switch (details.siteApprovalState) {
				case SWCServiceApprovalStateApproved:
					flags |= kSecSWCFlag_SiteApproved;
					break;

				case SWCServiceApprovalStateDenied:
					flags |= kSecSWCFlag_SiteDenied;
					break;

				default:
					break;
				}
			}

		} else {
			// An error occurred.
			secerror("+[_SWCServiceDetails serviceDetailsWithServiceSpecifier:error:] failed with %@", err);
		}
	}

	if (error) {
		if (!(flags & kSecSWCFlag_SiteApproved)) {
			SecError(errSecAuthFailed, error, CFSTR("\"%@\" failed to approve \"%@\""), fqdn, appID);
		} else if (flags & kSecSWCFlag_UserDenied) {
			SecError(errSecAuthFailed, error, CFSTR("User denied access to \"%@\" by \"%@\""), fqdn, appID);
		}
	}

	return flags;
}

void _SecSetAppDomainApprovalStatus(CFStringRef appID, CFStringRef fqdn, CFBooleanRef approved)
{
	_SWCServiceSpecifier *specifier = [[_SWCServiceSpecifier alloc] initWithServiceType:_SWCServiceTypeWebCredentials
																  applicationIdentifier:(__bridge NSString *)appID
																				 domain:(__bridge NSString *)fqdn];
	NSError *err = nil;
	NSArray<_SWCServiceDetails *> *allDetails = [_SWCServiceDetails serviceDetailsWithServiceSpecifier:specifier error:&err];
	if (allDetails) {
		_SWCServiceDetails *details = allDetails.firstObject;
		if (details) {
			SWCServiceApprovalState state = SWCServiceApprovalStateUnspecified;
			if (approved == kCFBooleanTrue) {
				state = SWCServiceApprovalStateApproved;
			} else if (approved == kCFBooleanFalse) {
				state = SWCServiceApprovalStateDenied;
			}
			if (![details setUserApprovalState:state error:&err]) {
				secerror("-[_SWCServiceDetails setUserApprovalState:error:] failed with %@", err);
			}
		}

	} else {
		secerror("+[_SWCServiceDetails serviceDetailsWithServiceSpecifier:error:] failed with %@", err);
	}
}

CFTypeRef _SecCopyFQDNObjectFromString(CFStringRef entitlementValue)
{
	CFTypeRef result = NULL;

	@autoreleasepool {
		_SWCServiceSpecifier *serviceSpecifier = [_SWCServiceSpecifier serviceSpecifiersWithEntitlementValue:@[ (__bridge NSString *)entitlementValue ] error:NULL].firstObject;
		if (!serviceSpecifier) {
			serviceSpecifier = [[_SWCServiceSpecifier alloc] initWithServiceType:nil applicationIdentifier:nil domain:(__bridge NSString *)entitlementValue];
		}
		if (serviceSpecifier) {
			// Hiding an ObjC reference in a CFTypeRef since the caller is pure C.
			result = CFBridgingRetain(serviceSpecifier);
		}
	}

	return result;
}

CFStringRef _SecGetFQDNFromFQDNObject(CFTypeRef fqdnObject, SInt32 *outPort)
{
	CFStringRef result = NULL;
	SInt32 port = -1;

	// Extracting an ObjC reference from a CFTypeRef since the caller is pure C.
	_SWCServiceSpecifier *serviceSpecifier = (__bridge _SWCServiceSpecifier *)fqdnObject;
	result = (__bridge CFStringRef)serviceSpecifier.domainHost;
	if (outPort) {
		NSNumber *portNumber = serviceSpecifier.domainPort;
		if (portNumber) {
			port = portNumber.unsignedShortValue;
		}

		*outPort = port;
	}

	return result;
}

#if !TARGET_OS_SIMULATOR
bool _SecEntitlementContainsDomainForService(CFArrayRef domains, CFStringRef domain, SInt32 port)
{
    bool result = false;

	@autoreleasepool {
		NSArray<_SWCServiceSpecifier *> *serviceSpecifiers;
		serviceSpecifiers = [_SWCServiceSpecifier serviceSpecifiersWithEntitlementValue:(__bridge NSArray<NSString *> *)domains
																			serviceType:_SWCServiceTypeWebCredentials
																				  error:NULL];
		for (_SWCServiceSpecifier *serviceSpecifier in serviceSpecifiers) {
			// Check if the hostname matches.
			NSString *specifierDomain = [serviceSpecifier domainHost];
			if (NSOrderedSame == [specifierDomain caseInsensitiveCompare:(__bridge NSString *)domain]) {
				result = true;

				// Also check the port if specified by the caller.
				if (result && port >= 0) {
					NSNumber *specifierPort = serviceSpecifier.domainPort;
					result = [specifierPort isEqualToNumber:@(port)];
				}

				if (result) {
					break;
				}
			}
		}
	}

    return result;
}
#endif /* !TARGET_OS_SIMULATOR */
#endif // TARGET_OS_IOS && !TARGET_OS_BRIDGE
