/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#include <CoreFoundation/CFRuntime.h>
#include "IOHIDTransactionElement.h"

static IOHIDTransactionElementRef   __IOHIDTransactionElementCreatePrivate(CFAllocatorRef allocator, CFAllocatorContext * context __unused);
static void                         __IOHIDTransactionElementRelease( CFTypeRef object );

typedef struct __IOHIDTransactionElement
{
    CFRuntimeBase               cfBase;   // base CFType information
    
    IOHIDElementRef             element;
    IOHIDValueRef               value;
    IOHIDValueRef               defaultValue;
    uint8_t                     state;

} __IOHIDTransactionElement, *__IOHIDTransactionElementRef;

static const CFRuntimeClass __IOHIDTransactionElementClass = {
    0,                      // version
    "IOHIDElement",         // className
    NULL,                   // init
    NULL,                   // copy
    __IOHIDTransactionElementRelease,  // finalize
    NULL,                   // equal
    NULL,                   // hash
    NULL,                   // copyFormattingDesc
    NULL,                   // copyDebugDesc
    NULL,                   // reclaim
    NULL,                   // refcount
};

static CFTypeID __kIOHIDTransactionElementTypeID = _kCFRuntimeNotATypeID;

IOHIDTransactionElementRef __IOHIDTransactionElementCreatePrivate(CFAllocatorRef allocator, CFAllocatorContext * context __unused)
{
    IOHIDTransactionElementRef     element = NULL;
    void *              offset  = NULL;
    uint32_t            size;
    
    /* allocate session */
    size  = sizeof(__IOHIDTransactionElement) - sizeof(CFRuntimeBase);
    element = (IOHIDTransactionElementRef)_CFRuntimeCreateInstance(allocator, IOHIDTransactionElementGetTypeID(), size, NULL);
    
    if (!element)
        return NULL;

    offset = element;
    bzero(offset + sizeof(CFRuntimeBase), size);
    
    return element;
}

void __IOHIDTransactionElementRelease( CFTypeRef object )
{
    IOHIDTransactionElementRef element = ( IOHIDTransactionElementRef ) object;

    if (element->element) CFRelease(element->element);
    if (element->value) CFRelease(element->value);
    if (element->defaultValue) CFRelease(element->defaultValue);
}

CFTypeID IOHIDTransactionElementGetTypeID(void)
{
    if ( __kIOHIDTransactionElementTypeID == _kCFRuntimeNotATypeID )
        __kIOHIDTransactionElementTypeID = _CFRuntimeRegisterClass(&__IOHIDTransactionElementClass);

    return __kIOHIDTransactionElementTypeID;
}

IOHIDTransactionElementRef IOHIDTransactionElementCreate(CFAllocatorRef allocator, IOHIDElementRef element, IOOptionBits options __unused)
{
    if ( !element )
        return NULL;

    IOHIDTransactionElementRef transElement = __IOHIDTransactionElementCreatePrivate(allocator, NULL);
    
    if ( transElement )
        transElement->element = (IOHIDElementRef)CFRetain(element);
        
    return transElement;
}

IOHIDElementRef IOHIDTransactionElementGetElement(IOHIDTransactionElementRef element)
{
    return element->element;
}

void IOHIDTransactionElementSetDefaultValue(IOHIDTransactionElementRef element, IOHIDValueRef value)
{
    if (element->defaultValue) CFRelease(element->defaultValue);
        
    element->defaultValue = value ? (IOHIDValueRef)CFRetain(value) : NULL;
}

IOHIDValueRef IOHIDTransactionElementGetDefaultValue(IOHIDTransactionElementRef element)
{
    return element->defaultValue;
}

void IOHIDTransactionElementSetValue(IOHIDTransactionElementRef element, IOHIDValueRef value)
{
    if (element->value) CFRelease(element->value);
        
    element->value = value ? (IOHIDValueRef)CFRetain(value) : NULL;
}

IOHIDValueRef IOHIDTransactionElementGetValue(IOHIDTransactionElementRef element)
{
    return element->value;
}
