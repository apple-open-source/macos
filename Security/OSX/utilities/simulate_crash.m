//
//  simulate_crash
//  utilities
//
//  Copyright (c) 2014 Apple Inc. All Rights Reserved.
//

#import <TargetConditionals.h>

#include "debugging.h"

#import <mach/mach.h>
#import <Foundation/Foundation.h>

#if !TARGET_OS_SIMULATOR

#import <OSAnalytics/CrashReporterSupport.h>

#endif // !TARGET_OS_SIMULATOR

static int __simulate_crash_counter = -1;

void __security_simulatecrash(CFStringRef reason, uint32_t code)
{
#if !TARGET_OS_SIMULATOR
    secerror("Simulating crash, reason: %@, code=%08x", reason, code);
    if (__security_simulatecrash_enabled()) {
        SimulateCrash(getpid(), code, (__bridge NSString *)reason);
    } else {
        __simulate_crash_counter++;
    }
#else
    secerror("Simulating crash (not supported on simulator), reason: %@, code=%08x", reason, code);
#endif
}

void __security_stackshotreport(CFStringRef reason, uint32_t code)
{
#if !TARGET_OS_SIMULATOR
    secerror("stackshot report, reason: %@, code=%08x", reason, code);
    if (!__security_simulatecrash_enabled()) {
        return;
    }
    WriteStackshotReport((__bridge NSString *)reason, code);
#else
    secerror("stackshot report (not supported on simulator, reason: %@, code=%08x", reason, code);
#endif
}


int __security_simulatecrash_enable(bool enable)
{
    int count = __simulate_crash_counter;
    __simulate_crash_counter = enable ? -1 : 0;
    return count;
}

bool __security_simulatecrash_enabled(void)
{
    return __simulate_crash_counter == -1;
}

