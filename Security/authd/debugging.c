/* Copyright (c) 2012 Apple Inc. All rights reserved. */

#include "debugging.h"
#include "authd_private.h"
#include "authutilities.h"
#include <stdarg.h>
#include <syslog.h>
#include <dispatch/dispatch.h>
#include <CoreFoundation/CoreFoundation.h>

// sudo defaults write /Library/Preferences/com.apple.security.coderequirements Entitlements -string always

static bool
security_auth_verbose(void)
{
    static dispatch_once_t onceToken;
    static bool verbose_enabled = false;
    
    //sudo defaults write /Library/Preferences/com.apple.authd verbose -bool true
    dispatch_once(&onceToken, ^{
		CFTypeRef verbose = (CFNumberRef)CFPreferencesCopyValue(CFSTR("verbose"), CFSTR(SECURITY_AUTH_NAME), kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
        
        if (verbose && CFGetTypeID(verbose) == CFBooleanGetTypeID()) {
            verbose_enabled = CFBooleanGetValue((CFBooleanRef)verbose);
        }
#if DEBUG
        syslog(LOG_NOTICE, "verbose: %s", verbose_enabled ? "enabled" : "disabled");
#endif
        CFReleaseSafe(verbose);
    });
    
    return verbose_enabled;
}

void
security_auth_log(int type,const char * format,...)
{
    va_list ap;
    va_start(ap, format);
    switch (type) {
        case AUTH_LOG_NORMAL:
            vsyslog(LOG_NOTICE, format, ap);
            break;
        case AUTH_LOG_VERBOSE:
            if (security_auth_verbose()) {
                vsyslog(LOG_NOTICE, format, ap);
            }
            break;
        case AUTH_LOG_ERROR:
            vsyslog(LOG_ERR, format, ap);
            break;
        default:
            break;
    }
    va_end(ap);
}

void _show_cf(CFTypeRef value)
{
    CFStringRef string = NULL;
    char * tmp = NULL;
    require(value != NULL, done);
    
    if (security_auth_verbose()) {
        string = CFCopyDescription(value);
        tmp = _copy_cf_string(string, NULL);
        
        syslog(LOG_NOTICE, "%s", tmp);
    }
    
done:
    CFReleaseSafe(string);
    free_safe(tmp);
    return;
}
