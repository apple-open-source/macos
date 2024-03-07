//
//  LWCRHelper.mm
//  Security
//

#import <Foundation/Foundation.h>
#import <TargetConditionals.h>

#if !TARGET_OS_SIMULATOR
#import <libamfi-interface.h>
#include "TLE.h"
#endif

#undef verify // Macro from Foundation conflicts with Security framework headers and isn't used below.

#import <Security/Security.h>
#import <kern/cs_blobs.h>
#import <security_utilities/debugging.h>
#import <security_utilities/errors.h>
#import <sys/types.h>
#import <sys/sysctl.h>
#import <os/variant_private.h>

#import "LWCRHelper.h"

#if TARGET_OS_SIMULATOR
// lwcr_keys.h doesn't exist for simulator builds yet...
#ifndef kLWCRFact_ValidationCategory
#define kLWCRFact_ValidationCategory "validation-category"
#endif
#ifndef kLWCRFact_SigningIdentifier
#define kLWCRFact_SigningIdentifier "signing-identifier"
#endif
#ifndef kLWCRFact_TeamIdentifier
#define kLWCRFact_TeamIdentifier "team-identifier"
#endif
#ifndef kLWCROperator_Or
#define kLWCROperator_Or "$or"
#endif
#ifndef kLWCROperator_And
#define kLWCROperator_And "$and"
#endif
#ifndef kLWCRFact_CDhash
#define kLWCRFact_CDhash "cdhash"
#endif
#ifndef kLWCROperator_In
#define kLWCROperator_In "$in"
#endif
#else
#import <lwcr_keys.h>
#endif


// anchor apple
const uint8_t platformReqData[] = {
	0xfa, 0xde, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03
};
const size_t platformReqDataLen = sizeof(platformReqData);

// anchor apple generic and certificate leaf[field.1.2.840.113635.100.6.1.25.1] or certificate leaf[1.2.840.113635.100.6.1.25.2]
// 1.2.840.113635.100.6.1.25.1 - Testflight Prod
// 1.2.840.113635.100.6.1.25.2 - Testflight QA
const uint8_t testflightReqData[] = {
	0xfa, 0xde, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06,
	0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b,
	0x2a, 0x86, 0x48, 0x86, 0xf7, 0x63, 0x64, 0x06, 0x01, 0x19, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
};
const size_t testflightReqDataLen = sizeof(testflightReqData);

// anchor apple generic and certificate 1[field.1.2.840.113635.100.6.2.1] and (certificate leaf[field.1.2.840.113635.100.6.1.2] or certificate leaf[field.1.2.840.113635.100.6.1.12])
// 1.2.840.113635.100.6.2.1 - WWDR CA
// 1.2.840.113635.100.6.1.2 - Apple Developer
// 1.2.840.113635.100.6.1.12 - Mac Developer
const uint8_t developmentReqData[] = {
	0xfa, 0xde, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06,
	0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x0a, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x63, 0x64, 0x06, 0x02, 0x01, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x0a, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x63, 0x64, 0x06, 0x01, 0x02, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a,
	0x2a, 0x86, 0x48, 0x86, 0xf7, 0x63, 0x64, 0x06, 0x01, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
const size_t developmentReqDataLen = sizeof(developmentReqData);

// anchor apple generic and (certificate leaf[field.1.2.840.113635.100.6.1.9] or certificate leaf[field.1.2.840.113635.100.6.1.9.1] or certificate leaf[field.1.2.840.113635.100.6.1.3] or certificate leaf[field.1.2.840.113635.100.6.1.3.1] or certificate leaf[field.1.2.840.113635.100.6.1.24] or certificate leaf[field.1.2.840.113635.100.6.1.24.1])
// 1.2.840.113635.100.6.1.9 - Mac App Store Prod
// 1.2.840.113635.100.6.1.9.1 - Mac App Store QA
// 1.2.840.113635.100.6.1.3 - iOS App Store Prod
// 1.2.840.113635.100.6.1.3.1 - iOS App Store QA
// 1.2.840.113635.100.6.1.24 - tvOS App Store Prod
// 1.2.840.113635.100.6.1.24.1 - tvOS App Store QA
const uint8_t appStoreReqData[] = {
	0xfa, 0xde, 0x0c, 0x00, 0x00, 0x00, 0x00, 0xd0, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06,
	0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x07,
	0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x0a, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x63, 0x64, 0x06, 0x01, 0x09, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b,
	0x2a, 0x86, 0x48, 0x86, 0xf7, 0x63, 0x64, 0x06, 0x01, 0x09, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x2a, 0x86, 0x48, 0x86,
	0xf7, 0x63, 0x64, 0x06, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x63, 0x64, 0x06,
	0x01, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x0a, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x63, 0x64, 0x06, 0x01, 0x18, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b,
	0x2a, 0x86, 0x48, 0x86, 0xf7, 0x63, 0x64, 0x06, 0x01, 0x18, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
};
const size_t appStoreReqDataLen = sizeof(appStoreReqData);

// anchor apple generic and certificate 1[field.1.2.840.113635.100.6.2.6] and certificate leaf[field.1.2.840.113635.100.6.1.13]
// 1.2.840.113635.100.6.2.6 - Developer ID CA
// 1.2.840.113635.100.6.1.13 - Developer ID
const uint8_t developerIDReqData[] = {
	0xfa, 0xde, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06,
	0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x0a, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x63, 0x64, 0x06, 0x02, 0x06, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a,
	0x2a, 0x86, 0x48, 0x86, 0xf7, 0x63, 0x64, 0x06, 0x01, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
const size_t developerIDReqDataLen = sizeof(developerIDReqData);

static NSDictionary* defaultPlatformLWCR(const char* signingIdentifier)
{
	if (signingIdentifier == NULL) {
		secerror("%s: signing identifier is NULL, cannot generate a LWCR",__FUNCTION__);
		return nil;
	}
	NSDictionary* lwcr = @{
		@kLWCRFact_ValidationCategory:@(CS_VALIDATION_CATEGORY_PLATFORM),
		@kLWCRFact_SigningIdentifier:@(signingIdentifier)
	};
	return lwcr;
}

static NSDictionary* defaultTestflightLWCR(const char* signingIdentifier)
{
	if (signingIdentifier == NULL) {
		secerror("%s: signing identifier is NULL, cannot generate a LWCR",__FUNCTION__);
		return nil;
	}
	NSDictionary* lwcr = @{
		@kLWCRFact_ValidationCategory:@(CS_VALIDATION_CATEGORY_TESTFLIGHT),
		@kLWCRFact_SigningIdentifier:@(signingIdentifier)
	};
	return lwcr;
}

static NSDictionary* defaultDevelopmentLWCR(const char* signingIdentifier, const char* teamIdentifier)
{
	if (signingIdentifier == NULL) {
		secerror("%s: signing identifier is NULL, cannot generate a LWCR",__FUNCTION__);
		return nil;
	}
	if (teamIdentifier == NULL) {
		secerror("%s: team identifier is NULL, cannot generate a LWCR",__FUNCTION__);
		return nil;
	}
	NSDictionary* lwcr = @{
		@kLWCRFact_ValidationCategory:@(CS_VALIDATION_CATEGORY_DEVELOPMENT),
		@kLWCRFact_SigningIdentifier:@(signingIdentifier),
		@kLWCRFact_TeamIdentifier:@(teamIdentifier),
	};
	return lwcr;
}

static NSDictionary* defaultAppStoreLWCR(const char* signingIdentifier, const char* teamIdentifier)
{
	if (signingIdentifier == NULL) {
		secerror("%s: signing identifier is NULL, cannot generate a LWCR",__FUNCTION__);
		return nil;
	}
	if (teamIdentifier == NULL) {
		secerror("%s: team identifier is NULL, cannot generate a LWCR",__FUNCTION__);
		return nil;
	}
	NSDictionary* lwcr = @{
		@kLWCROperator_Or: @{
			@kLWCRFact_ValidationCategory:@(CS_VALIDATION_CATEGORY_APP_STORE),
			@kLWCROperator_And:@{
				@kLWCRFact_ValidationCategory:@(CS_VALIDATION_CATEGORY_DEVELOPER_ID),
				@kLWCRFact_TeamIdentifier:@(teamIdentifier)
			}
		},
		@kLWCRFact_SigningIdentifier:@(signingIdentifier),
	};
	return lwcr;
}

static NSDictionary* defaultDeveloperIDLWCR(const char* signingIdentifier, const char* teamIdentifier)
{
	if (signingIdentifier == NULL) {
		secerror("%s: signing identifier is NULL, cannot generate a LWCR",__FUNCTION__);
		return nil;
	}
	if (teamIdentifier == NULL) {
		secerror("%s: team identifier is NULL, cannot generate a LWCR",__FUNCTION__);
		return nil;
	}
	NSDictionary* lwcr = @{
		@kLWCRFact_ValidationCategory:@(CS_VALIDATION_CATEGORY_DEVELOPER_ID),
		@kLWCRFact_SigningIdentifier:@(signingIdentifier),
		@kLWCRFact_TeamIdentifier:@(teamIdentifier),
	};
	return lwcr;
}

static NSDictionary* defaultAdhocLWCR(NSArray* allCdhashes)
{
	if (allCdhashes == nil || allCdhashes.count == 0) {
		secerror("%s: no cdhashes for code, cannot generate a LWCR", __FUNCTION__);
		return nil;
	}
	NSDictionary* lwcr = @{
		@kLWCRFact_CDhash : @{
			@kLWCROperator_In: allCdhashes
		},
	};
	return lwcr;
}

CFDictionaryRef copyDefaultDesignatedLWCRMaker(unsigned int validationCategory,
											   const char* signingIdentifier,
											   const char* teamIdentifier,
											   CFArrayRef allCdhashes)
{
	NSDictionary* lwcr = nil;
	switch (validationCategory) {
	case CS_VALIDATION_CATEGORY_PLATFORM:
		lwcr = defaultPlatformLWCR(signingIdentifier);
		break;
	case CS_VALIDATION_CATEGORY_TESTFLIGHT:
		lwcr = defaultTestflightLWCR(signingIdentifier);
		break;
	case CS_VALIDATION_CATEGORY_DEVELOPMENT:
		lwcr = defaultDevelopmentLWCR(signingIdentifier, teamIdentifier);
		break;
	case CS_VALIDATION_CATEGORY_APP_STORE:
		lwcr = defaultAppStoreLWCR(signingIdentifier, teamIdentifier);
		break;
	case CS_VALIDATION_CATEGORY_DEVELOPER_ID:
		lwcr = defaultDeveloperIDLWCR(signingIdentifier, teamIdentifier);
		break;
	default:
		lwcr = defaultAdhocLWCR((__bridge NSArray*)allCdhashes);
		break;
	}
	return (__bridge_retained CFDictionaryRef)lwcr;
}

#if !TARGET_OS_SIMULATOR
static sec_LWCR* makeLightweightCodeRequirement(CFDataRef lwcrData)
{
	NSError* error = nil;
	sec_LWCR* lwcr = [sec_LWCR withData:(__bridge NSData*)lwcrData withError:&error];
	if (error) {
		secerror ("%s: failed to parse LightweightCodeRequirement", __FUNCTION__);
	}
	return lwcr;
}

OSStatus validateLightweightCodeRequirementData(CFDataRef lwcrData)
{
	sec_LWCR* lwcr = makeLightweightCodeRequirement(lwcrData);
	if (lwcr == nil) {
		Security::MacOSError::throwMe(errSecBadReq);
	}
	return errSecSuccess;
}

static NSString* getOSEnvironment()
{
	size_t len = 0;
	int ret = sysctlbyname("hw.osenvironment", NULL, &len, NULL, 0);
	if (ret) {
		secerror("%s: failed to query hw.osenvironment sysctl (error: %s)", __FUNCTION__, strerror(errno));
		return nil;
	}
	if (len == 0) {
		return [NSString new];
	}
	char* buf = (char*)malloc(len);
	
	NSString* osenv = nil;
	ret = sysctlbyname("hw.osenvironment", buf, &len, NULL, 0);
	if (ret) {
		secerror("%s: failed to retrieve hw.osenvironment (error: %s)", __FUNCTION__, strerror(errno));
		// Fall through to clean up and return nil.
	} else {
		osenv = [NSString stringWithCString:buf encoding:NSUTF8StringEncoding];
	}
	free(buf);
	return osenv;
}

static void bindAndAdd(NSMutableDictionary<NSString*, sec_LWCRFact*>* facts, const char* key, sec_LWCRFact* fact)
{
	[fact bindName:key withLength:strlen(key)];
	facts[@(key)] = fact;
}

static void bindAndAddIntegerFact(NSMutableDictionary<NSString*, sec_LWCRFact*>* facts, const char* key, int64_t val)
{
	sec_LWCRFact* fact = [sec_LWCRFact integerFact:[NSNumber numberWithLongLong:val]];
	bindAndAdd(facts, key, fact);
}

static void bindAndAddStringFact(NSMutableDictionary<NSString*, sec_LWCRFact*>* facts, const char* key, const char* val)
{
	sec_LWCRFact* fact = [sec_LWCRFact stringFact:@(val)];
	bindAndAdd(facts, key, fact);
}

static void bindAndAddBoolFact(NSMutableDictionary<NSString*, sec_LWCRFact*>* facts, const char* key, bool val)
{
	sec_LWCRFact* fact = [sec_LWCRFact boolFact:val];
	bindAndAdd(facts, key, fact);
}

static NSDictionary<NSString*, sec_LWCRFact*>* collectFacts(const Security::CodeSigning::Requirement::Context &ctx)
{
	NSMutableDictionary<NSString*, sec_LWCRFact*>* facts = [NSMutableDictionary dictionary];
	//kLWCRFact_AppleInternal
	bool appleInternal = os_variant_allows_internal_security_policies("com.apple.security.codesigning");
	bindAndAddBoolFact(facts,kLWCRFact_AppleInternal, appleInternal);
	//kLWCRFact_CDhash
	sec_LWCRFact* cdhash = [sec_LWCRFact dataFact:(__bridge_transfer NSData*)ctx.directory->cdhash()];
	bindAndAdd(facts, kLWCRFact_CDhash, cdhash);
	//kLWCRFact_CodeSigningFlags
	uint32_t csflags_raw = ctx.directory->flags;
	bindAndAddIntegerFact(facts, kLWCRFact_CodeSigningFlags, csflags_raw);
	//kLWCRFact_DeveloperModeEnabled
	bool developer_mode_status = amfi_developer_mode_status();
	bindAndAddBoolFact(facts, kLWCRFact_DeveloperModeEnabled, developer_mode_status);
	//kLWCRFact_Entitlements
	sec_LWCRFact* entitlements = [sec_LWCRFact entitlementsFact:(__bridge NSDictionary*)ctx.entitlements];
	bindAndAdd(facts, kLWCRFact_Entitlements, entitlements);
	//kLWCRFact_InfoPlistHash
	uint8_t hashSize = ctx.directory->hashSize;
	const uint8_t* infoPlistHashData_raw = ctx.directory->getSlot(-Security::CodeSigning::cdInfoSlot, false);
	NSData* infoPlistHashData = [NSData dataWithBytes:(void*)infoPlistHashData_raw length:hashSize];
	sec_LWCRFact* infoPlistHash = [sec_LWCRFact dataFact:infoPlistHashData];
	bindAndAdd(facts, kLWCRFact_InfoPlistHash, infoPlistHash);
	//kLWCRFact_InTrustcacheWithConstraintCategory is unsupported for now. Need a new kernel interface.
	//kLWCRFact_IsMainBinary
	bindAndAddBoolFact(facts, kLWCRFact_IsMainBinary, (ctx.directory->execSegFlags & CS_EXECSEG_MAIN_BINARY) ? true : false);
	//kLWCRFact_IsSIPProtected
	bindAndAddBoolFact(facts, kLWCRFact_IsSIPProtected, ctx.isSIPProtected);
	//kLWCRFact_OnAuthorizedAuthAPFSVolume
	bindAndAddBoolFact(facts, kLWCRFact_OnAuthorizedAuthAPFSVolume, ctx.onAuthorizedAuthAPFSVolume);
	//kLWCRFact_OnSystemVolume
	bindAndAddBoolFact(facts, kLWCRFact_OnSystemVolume, ctx.onSystemVolume);
	//kLWCRFact_OSEnvironment
	NSString* osenvironment = getOSEnvironment();
	if (osenvironment) {
		bindAndAddStringFact(facts, kLWCRFact_OSEnvironment, osenvironment.UTF8String);
	} else {
		bindAndAddStringFact(facts, kLWCRFact_OSEnvironment, "");
	}
	//kLWCRFact_Platform
	bindAndAddIntegerFact(facts, kLWCRFact_Platform, ctx.platformType);
	//kLWCRFact_PlatformIdentifier
	uint8_t platformIdentifier_raw = ctx.directory->platform;
	bindAndAddIntegerFact(facts, kLWCRFact_PlatformIdentifier, platformIdentifier_raw);
	//kLWCRFact_SigningIdentifier
	bindAndAddStringFact(facts, kLWCRFact_SigningIdentifier, ctx.identifier.c_str());
	//kLWCRFact_TeamIdentifier
	if (ctx.teamIdentifier != NULL) {
		bindAndAddStringFact(facts, kLWCRFact_TeamIdentifier, ctx.teamIdentifier);
	}
	//kLWCRFact_ValidationCategory
	bindAndAddIntegerFact(facts, kLWCRFact_ValidationCategory, ctx.validationCategory);
	return facts;
}

bool evaluateLightweightCodeRequirement(const Security::CodeSigning::Requirement::Context &ctx, CFDataRef lwcrData)
{
	@autoreleasepool {
		NSDictionary<NSString*, sec_LWCRFact*>* facts = collectFacts(ctx);
		sec_LWCR* lwcr = makeLightweightCodeRequirement(lwcrData);
		if (lwcr == nil) {
			Security::MacOSError::throwMe(errSecBadReq);
		}
		
		BOOL matches = [[sec_LWCRExecutor executor] evaluateRequirements:lwcr withFacts:facts];
		return matches == YES ? true : false;
	}
}
#endif
