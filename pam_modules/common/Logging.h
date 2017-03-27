//
//  Logging.h
//  pam_modules
//

#ifndef Logging_h
#define Logging_h

#include <os/log.h>
#include <os/activity.h>

#define PAM_DEFINE_LOG(category) \
static os_log_t PAM_LOG_ ## category () { \
static dispatch_once_t once; \
static os_log_t log; \
dispatch_once(&once, ^{ log = os_log_create("com.apple.pam", #category); }); \
return log; \
};

#endif /* Logging_h */
