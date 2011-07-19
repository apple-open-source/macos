/*
 * Copyright (c) 1999-2008 Apple Computer, Inc.  All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
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

#include <IOKit/IOKitLib.h>
#include <IOKit/IOReturn.h>
#include <stdarg.h>
#include <asl.h>
#include "IOHIDLibPrivate.h"
#include "IOHIDBase.h"

void _IOObjectCFRelease(        CFAllocatorRef          allocator  __unused, 
                                const void *            value)
{
    IOObjectRelease((io_object_t)(uintptr_t) value);
}

const void * _IOObjectCFRetain( CFAllocatorRef          allocator  __unused, 
                                const void *            value)
{
    if (kIOReturnSuccess != IOObjectRetain((io_object_t)(uintptr_t)value))
        return NULL;
    
    return value;
}

void _IOHIDCallbackApplier(const void *callback, 
                           const void *callbackContext, 
                           void *applierContext)
{
    IOHIDCallbackApplierContext *context = (IOHIDCallbackApplierContext*)applierContext;
    if (callback && context)
        ((IOHIDCallback)callback)((void *)callbackContext, context->result, context->sender);
}

void _IOHIDLog(int level, const char *format, ...)
{
    aslmsg msg = NULL;
    
    if (1) {
        msg = asl_new(ASL_TYPE_MSG);
        asl_set(msg, ASL_KEY_FACILITY, "com.apple.iokit.IOHID");
    }
    va_list ap;
    va_start(ap, format);
    asl_vlog(NULL, msg, level, format, ap);
    va_end(ap);
    if (msg) {
        asl_free(msg);
    }
}
