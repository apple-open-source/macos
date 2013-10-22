/*-
 * Copyright (c) 2013 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#import <CoreFoundation/CoreFoundation.h>
#import <CoreFoundation/CFRuntime.h>
#import <xpc/xpc.h>

#import "heimcred.h"

#define CFRELEASE_NULL(x) do { if (x) { CFRelease(x); x = NULL; } } while(0)

struct HeimCred_s {
    CFRuntimeBase runtime;
    CFUUIDRef uuid;
    CFDictionaryRef attributes;
};

typedef struct {
    dispatch_queue_t queue;
    CFMutableDictionaryRef items;
    CFTypeID haid;
#if HEIMCRED_SERVER
    CFSetRef connections;
#else
    xpc_connection_t conn;
#endif
} HeimCredContext;

extern HeimCredContext HeimCredCTX;

void
_HeimCredInitCommon(void);

HeimCredRef
HeimCredCreateItem(CFUUIDRef uuid);

CFUUIDRef
HeimCredCopyUUID(xpc_object_t object, const char *key);

CFDictionaryRef
HeimCredMessageCopyAttributes(xpc_object_t object, const char *key);

void
HeimCredMessageSetAttributes(xpc_object_t object, const char *key, CFTypeRef attrs);

void
HeimCredSetUUID(xpc_object_t object, const char *key, CFUUIDRef uuid);


#define HEIMCRED_CONST(_t,_c) extern const char * _c##xpc;
#include "heimcred-const.h"
#undef HEIMCRED_CONST
