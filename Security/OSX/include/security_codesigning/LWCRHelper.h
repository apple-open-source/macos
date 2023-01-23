//
//  LWCRHelper.h
//  Security
//

#ifndef LWCRHelper_h
#define LWCRHelper_h

#import <CoreFoundation/CoreFoundation.h>

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
#endif /* LWCRHelper_h */
