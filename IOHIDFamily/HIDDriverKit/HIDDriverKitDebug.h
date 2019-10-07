/*
 * Copyright (c) 2018-2019 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#ifndef HIDDriverKitDebug_h
#define HIDDriverKitDebug_h

#include <stdio.h>
#include <os/log.h>
#include <sys/kdebug.h>

// boot-args for hiddk
uint32_t gHIDDKDebug();

using _cstr = const char * const;

static constexpr _cstr slash_to_end(_cstr str, _cstr last_slash)
{
    return
    *str == '\0' ? last_slash :
    *str == '/'  ? slash_to_end(str + 1, str + 1) : slash_to_end(str + 1, last_slash);
}

static constexpr _cstr slash_to_end(_cstr str)
{
    return slash_to_end (str, str);
}

#define __SHORT_FILENAME__ ({constexpr _cstr _fl {slash_to_end(__FILE__)}; _fl;})

#define HIDLog(fmt, args...) \
    os_log(OS_LOG_DEFAULT, "[%{public}s:%d]" fmt "\n", __SHORT_FILENAME__, __LINE__, ##args);

#if TARGET_OS_DRIVERKIT
#define HIDLogDebug(fmt, args...) HIDLog(fmt, ##args)
#define HIDLogInfo(fmt, args...) HIDLog(fmt, ##args)
#define HIDLogError(fmt, args...) HIDLog(fmt, ##args)
#define HIDLogFault(fmt, args...) HIDLog(fmt, ##args)

#else
#define HIDLogDebug(fmt, args...) \
    os_log_debug(OS_LOG_DEFAULT, "[%{public}s:%d]" fmt "\n", __SHORT_FILENAME__, __LINE__, ##args)

#define HIDLogInfo(fmt, args...) \
    os_log_info(OS_LOG_DEFAULT, "[%{public}s:%d]" fmt "\n", __SHORT_FILENAME__, __LINE__, ##args)

#define HIDLogError(fmt, args...) \
    os_log_error(OS_LOG_DEFAULT, "[%{public}s:%d]" fmt "\n", __SHORT_FILENAME__, __LINE__, ##args)

#define HIDLogFault(fmt, args...) \
    os_log_fault(OS_LOG_DEFAULT, "[%{public}s:%d]" fmt "\n", __SHORT_FILENAME__, __LINE__, ##args)
#endif // TARGET_OS_DRIVERKIT

#define getRegistryID \
    ^(){uint64_t rid = 0; GetRegistryEntryID(&rid); return rid;}


#define HIDServiceLog(fmt, ...)          HIDLog("[0x%llx] " fmt "\n", getRegistryID(), ##__VA_ARGS__)
#define HIDServiceLogInfo(fmt, ...)      HIDLogInfo("[0x%llx] " fmt "\n", getRegistryID(), ##__VA_ARGS__)
#define HIDServiceLogError(fmt, ...)     HIDLogError("[0x%llx] " fmt "\n", getRegistryID(), ##__VA_ARGS__)
#define HIDServiceLogDebug(fmt, ...)     HIDLogDebug("[0x%llx] " fmt "\n", getRegistryID(), ##__VA_ARGS__)
#define HIDServiceLogFault(fmt, ...)     HIDLogFault("[0x%llx] " fmt "\n", getRegistryID(), ##__VA_ARGS__)

enum {
    kHIDDK_TraceBase            = 0x3000,
    kHIDDK_ES_HandleReportCB    = 0x3001, // 0x523c004
    kHIDDK_Dev_InputReport      = 0x3002, // 0x523c008
    kHIDDK_ES_Start             = 0x3003, // 0x523c00c
    kHIDDK_ES_Stop              = 0x3004, // 0x523c010
    kHIDDK_Dev_Start            = 0x3005, // 0x523c014
    kHIDDK_Dev_Stop             = 0x3006, // 0x523c018
};

#define HIDTrace(code, a, b, c, d)          kdebug_trace(IOKDBG_CODE(DBG_IOHID, code), a, b, c, d)
#define HIDTraceStart(code, a, b, c, d)     kdebug_trace(IOKDBG_CODE(DBG_IOHID, code) | DBG_FUNC_START, a, b, c, d)
#define HIDTraceEnd(code, a, b, c, d)       kdebug_trace(IOKDBG_CODE(DBG_IOHID, code) | DBG_FUNC_END, a, b, c, d)

#endif /* HIDDriverKitDebug_h */
