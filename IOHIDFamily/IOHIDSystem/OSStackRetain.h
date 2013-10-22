/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2013 Apple Computer, Inc.  All Rights Reserved.
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

#ifndef IOHIDFamily_OSStackRetain_h
#define IOHIDFamily_OSStackRetain_h

#include <IOKit/IOTypes.h>

class OSStackRetain {
    OSObject *_retainedObject;
    OSStackRetain() : _retainedObject(NULL) {}
public:
    OSStackRetain(OSObject *object, bool retain = false) : _retainedObject(object) {
        if (_retainedObject && retain) {
            _retainedObject->retain();
        }
    }
    ~OSStackRetain() {
        if (_retainedObject) {
            _retainedObject->release();
        }
        _retainedObject = NULL;
    }
};

#define CONVERT_TO_STACK_RETAIN(X) OSStackRetain  _ ## X ## _retained(X, false)
#define RETAIN_ON_STACK(X) OSStackRetain  _ ## X ## _retained(X, true)

#endif
