/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 2015 Apple Computer, Inc.  All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef IOKitUser_IOHIDDebugTrace_h
#define IOKitUser_IOHIDDebugTrace_h

#include <sys/kdebug.h>
#include <sys/syscall.h>
#include "IOHIDLibPrivate.h"

// Set to 1 to enable profiling traces
// define in individual files to enable traces in individual files

#ifndef HID_PROFILING
#define HID_PROFILING 0
#endif 

// Set to 1 to enable HID tracing
#ifndef HID_TRACING
#define HID_TRACING 0
#endif

enum {
    kIOHIDEnableProfilingBit = 0,
    
    kIOHIDEnableProfilingMask = 1 << kIOHIDEnableProfilingBit,
};

enum {
    kHIDTrace_TraceBase = 0x2000,
    kHIDTrace_UserDevice_Create,
    kHIDTrace_UserDevice_Release,
    kHIDTrace_UserDevice_Start,
    kHIDTrace_UserDevice_AsyncSupport,
    kHIDTrace_UserDevice_Unschedule,
    kHIDTrace_UserDevice_ScheduleDispatch,
    kHIDTrace_UserDevice_UnscheduleDispatch,
    kHIDTrace_UserDevice_QueueCallback,
    kHIDTrace_UserDevice_HandleReport,
    kHIDTrace_UserDevice_HandleReportCallback,
    kHIDTrace_UserDevice_SetReportCallback,
    kHIDTrace_HIDEventSystem_EventCallback = 0x2010,
    kHIDTrace_HIDEventSystem_FiltersClientsDone,
    kHIDTrace_HIDEventSystem_SystemFilterDone,
    kHIDTrace_HIDEventSystemClient_QueueCallback = 0x2020,
    kHIDTrace_HIDEventSystemConnection_DispatchEvent = 0x2030,
    kHIDTrace_HIDEventSystemService_Callback = 0x2040,
    kHIDTrace_HIDEventSystemService_Create,
    kHIDTrace_HIDEventSystemService_Open,
    kHIDTrace_HIDEventSystemSession_Callback = 0x2050,
    kHIDTrace_HIDEventSystemSession_Dispatch,
};

#define IOHID_DEBUG_CODE(code)          IOKDBG_CODE(DBG_IOHID, code)
#define IOHID_DEBUG_START(code)         IOHID_DEBUG_CODE(code) | DBG_FUNC_START
#define IOHID_DEBUG_END(code)           IOHID_DEBUG_CODE(code) | DBG_FUNC_END

#if HID_TRACING
#define HIDDEBUGTRACE(code, a, b, c, d) do {                      \
    kdebug_trace(IOHID_DEBUG_CODE(code), a, b, c, d);             \
} while (0)

#define HIDFUNCSTART(code, a, b, c, d) do {                       \
    kdebug_trace(IOHID_DEBUG_START(code), a, b, c, d);            \
} while (0)

#define HIDFUNCEND(code, a, b, c, d) do {                         \
    kdebug_trace(IOHID_DEBUG_END(code), a, b, c, d);              \
} while (0)
#else
#define HIDDEBUGTRACE(code, a, b, c, d)     IOHIDLogTrace(#code " 0x%-16llx 0x%-16llx 0x%-16llx 0x%-16llx 0x%-16llx", (unsigned long long)k##code, (unsigned long long)a, (unsigned long long)b, (unsigned long long)c, (unsigned long long)d)
#define HIDFUNCSTART(code, a, b, c, d)      IOHIDLogTrace(#code " 0x%-16llx 0x%-16llx 0x%-16llx 0x%-16llx 0x%-16llx", (unsigned long long)k##code, (unsigned long long)a, (unsigned long long)b, (unsigned long long)c, (unsigned long long)d)
#define HIDFUNCEND(code, a, b, c, d)        IOHIDLogTrace(#code " 0x%-16llx 0x%-16llx 0x%-16llx 0x%-16llx 0x%-16llx", (unsigned long long)k##code, (unsigned long long)a, (unsigned long long)b, (unsigned long long)c, (unsigned long long)d)
#endif

#if HID_PROFILING
#define HIDPROFTRACE(code, a, b, c, d) do {                       \
    HIDDEBUGTRACE(code, a, b, c, d);                              \
} while (0)
#else
#define HIDPROFTRACE(code, a, b, c, d)      IOHIDLogTrace(#code " 0x%-16llx 0x%-16llx 0x%-16llx 0x%-16llx 0x%-16llx", (unsigned long long)k##code, (unsigned long long)a, (unsigned long long)b, (unsigned long long)c, (unsigned long long)d)
#endif

#endif
