//
//  ForkSafeLogging.h
//  pam_modules
//
//  Fork detection to disable framework logging after fork
//

#ifndef ForkSafeLogging_h
#define ForkSafeLogging_h

#include <pthread.h>
#include <stdbool.h>
#import <os/trace_private.h>

static bool _pam_is_forked = false;

// Fork handlers using pthread_atfork pattern from Heimdal
static void _pam_fork_child_handler(void) {
    _pam_is_forked = true;
    // Disable os_trace to prevent OpenDirectory and other framework crashes
    os_trace_set_mode(OS_TRACE_MODE_DISABLE);
}

// Library constructor to register fork handlers
__attribute__((constructor))
static void _pam_init_fork_detection(void) {
    pthread_atfork(NULL, NULL, _pam_fork_child_handler);
}

// Check if we're running after fork
static inline bool pam_is_after_fork(void) {
    return _pam_is_forked;
}

#endif /* ForkSafeLogging_h */
