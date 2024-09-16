/*
 * Copyright (c) 2011-2024 Apple Inc. All rights reserved.
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

#ifndef ObjectWrapperInternal_h
#define ObjectWrapperInternal_h

/*
 * Type: ObjectWrapperRef
 *
 * Purpose:
 *  Provides a level of indirection between an object
 *  (e.g. IPConfigurationServiceRef) and any other object(s) that might need
 *  to reference it (e.g. SCDynamicStoreRef).
 *
 *  For an object that is invalidated by calling CFRelease
 *  (e.g. IPConfigurationServiceRef), that means there is normally only
 *  a single reference to that object. If there's an outstanding block that
 *  was scheduled (but not run) while calling CFRelease() on that object,
 *  when the block does eventually run, it can't validly reference
 *  that object anymore, it's been deallocated.
 *
 *  The ObjectWrapperRef is a simple, reference-counted structure that just
 *  stores an object pointer. In the *Deallocate function of the object,
 *  it synchronously calls ObjectWrapperClearObject(). When the block
 *  referencing the wrapper runs, it calls ObjectWrapperGetObject(), and if
 *  it is NULL, does not continue.
 */
typedef struct ObjectWrapper * ObjectWrapperRef;

ObjectWrapperRef
ObjectWrapperAlloc(const void * obj);

const void *
ObjectWrapperRetain(const void * info);

void
ObjectWrapperRelease(const void * info);

const void *
ObjectWrapperGetObject(ObjectWrapperRef wrapper);

void
ObjectWrapperClearObject(ObjectWrapperRef wrapper);

#endif /* ObjectWrapperInternal_h */
