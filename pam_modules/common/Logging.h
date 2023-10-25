//
//  Logging.h
//  pam_modules
//

#ifndef Logging_h
#define Logging_h

// to turn on os_logs, uncomment following line
// #define PAM_USE_OS_LOG

#ifdef PAM_USE_OS_LOG
#include <os/log.h>
#include <os/activity.h>

#define PAM_DEFINE_LOG(category) \
static os_log_t PAM_LOG_ ## category () { \
static dispatch_once_t once; \
static os_log_t log; \
dispatch_once(&once, ^{ log = os_log_create("com.apple.pam", #category); }); \
return log; \
};

#define _LOG_DEBUG(...) os_log_debug(PAM_LOG, __VA_ARGS__)
#define _LOG_VERBOSE(...) os_log_debug(PAM_LOG, __VA_ARGS__)
#define _LOG_INFO(...) os_log_info(PAM_LOG, __VA_ARGS__)
#define _LOG_DEFAULT(...) os_log(PAM_LOG, __VA_ARGS__)
#define _LOG_ERROR(...) os_log_error(PAM_LOG, __VA_ARGS__)

#else

#define _LOG_DEBUG(...) openpam_log(PAM_LOG_DEBUG, __VA_ARGS__)
#define _LOG_VERBOSE(...) openpam_log(PAM_LOG_VERBOSE, __VA_ARGS__)
#define _LOG_INFO(...) openpam_log(PAM_LOG_VERBOSE, __VA_ARGS__)
#define _LOG_DEFAULT(...) openpam_log(PAM_LOG_NOTICE, __VA_ARGS__)
#define _LOG_ERROR(...) openpam_log(PAM_LOG_ERROR, __VA_ARGS__)
#endif

#endif /* Logging_h */
