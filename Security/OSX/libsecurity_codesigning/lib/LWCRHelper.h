//
//  LWCRHelper.h
//  Security
//

#ifndef LWCRHelper_h
#define LWCRHelper_h

#import <CoreFoundation/CoreFoundation.h>
#import <TargetConditionals.h>
#include "requirement.h"

__BEGIN_DECLS

extern const uint8_t platformReqData[];
extern const size_t platformReqDataLen;

extern const uint8_t testflightReqData[];
extern const size_t testflightReqDataLen;
extern const uint8_t developmentReqData[];
extern const size_t developmentReqDataLen;
extern const uint8_t appStoreReqData[];
extern const size_t appStoreReqDataLen;
extern const uint8_t developerIDReqData[];
extern const size_t developerIDReqDataLen;

CFDictionaryRef copyDefaultDesignatedLWCRMaker(unsigned int validationCategory, const char* signingIdentifier, const char* teamIdentifier, CFArrayRef allCdhashes);

#if !TARGET_OS_SIMULATOR
OSStatus validateLightweightCodeRequirementData(CFDataRef lwcrData);
bool evaluateLightweightCodeRequirement(const Security::CodeSigning::Requirement::Context &ctx, CFDataRef lwcrData);
#endif

__END_DECLS
#endif /* LWCRHelper_h */
