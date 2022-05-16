#import <IOKit/IOReturn.h>

#if TARGET_OS_IPHONE || POWERD_IOS_XCTEST
IOReturn initializeGeneralPreferences(void);
#else
static inline IOReturn initializeGeneralPreferences() { return kIOReturnSuccess; }
#endif
