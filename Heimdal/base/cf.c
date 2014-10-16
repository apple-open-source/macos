/*
 * Copyright (c) 2011 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2011 - 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "baselocl.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <dispatch/dispatch.h>

static void *
_heim_create_cf_instance(CFTypeID typeID, size_t size, char *category)
{
    heim_assert(size >= sizeof(CFRuntimeBase), "cf runtime size too small");
    CFTypeRef type = _CFRuntimeCreateInstance(NULL, typeID, size - sizeof(struct heim_base), (unsigned char *)category);
    if (type)
	memset(((uint8_t *)type) + sizeof(struct heim_base), 0, size - sizeof(struct heim_base));
    return (void *)type;
}

void
heim_base_once_f(heim_base_once_t *once, void *ctx, void (*func)(void *))
{
    dispatch_once_f(once, ctx, func);
}


void *
heim_retain(void *ptr)
{
    if (ptr)
	CFRetain(ptr);
    return ptr;
}

void
heim_release(void *ptr)
{
    if (ptr)
	CFRelease(ptr);
}

heim_tid_t
heim_get_tid(heim_object_t ptr)
{
    return (heim_tid_t)CFGetTypeID(ptr);
}

unsigned long
heim_get_hash(heim_object_t ptr)
{
    return CFHash(ptr);
}

int
heim_cmp(heim_object_t a, heim_object_t b)
{
    if (CFEqual(a, b))
	return 0;

    uintptr_t ai = (uintptr_t)a;
    uintptr_t bi = (uintptr_t)b;

    heim_assert(ai != bi, "pointers are the same ?");

    while (((int)(ai - bi)) == 0) {
	ai = ai >> 8;
	bi = bi >> 8;
    }
    int diff = (int)(ai - bi);
    heim_assert(diff != 0, "pointers are the same ?");

    return diff;

}

heim_array_t
heim_array_create(void)
{
    return (heim_array_t)CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
}

heim_tid_t
heim_array_get_type_id(void)
{
    return (heim_tid_t)CFDictionaryGetTypeID();
}

int
heim_array_append_value(heim_array_t array, heim_object_t object)
{
    CFArrayAppendValue((CFMutableArrayRef)array, object);
    return 0;
}

void
heim_array_iterate_f(heim_array_t array,
		     void *ctx,
		     heim_array_iterator_f_t fn)
{
    CFIndex n, count = CFArrayGetCount((CFArrayRef)array);
    int stop = 0;
    for (n = 0; !stop && n < count; n++)
	fn((heim_array_t)CFArrayGetValueAtIndex((CFArrayRef)array, n), &stop, ctx);
}

#if __BLOCKS__
void
heim_array_iterate(heim_array_t array, heim_array_iterator_t fn)
{
    CFIndex n, count = CFArrayGetCount((CFArrayRef)array);
    int stop = 0;
    for (n = 0; !stop && n < count; n++)
	fn((heim_array_t)CFArrayGetValueAtIndex((CFArrayRef)array, n), &stop);
}
#endif

size_t
heim_array_get_length(heim_array_t array)
{
    return CFArrayGetCount((CFArrayRef)array);
}

heim_object_t
heim_array_copy_value(heim_array_t array, size_t idx)
{
    CFTypeRef value = CFArrayGetValueAtIndex((CFArrayRef)array, idx);
    if (value)
	CFRetain(value);
    return (heim_object_t)value;
}

void
heim_array_delete_value(heim_array_t array, size_t idx)
{
    CFArrayRemoveValueAtIndex((CFMutableArrayRef)array, idx);
}

#if __BLOCKS__
void
heim_array_filter(heim_array_t array, int (^block)(heim_object_t))
{
    CFIndex n = 0;
    
    while (n < CFArrayGetCount((CFArrayRef)array)) {
	if (block((heim_array_t)CFArrayGetValueAtIndex((CFArrayRef)array, n))) {
	    heim_array_delete_value(array, n);
	} else {
	    n++;
	}
    }
}
#endif

int
heim_array_contains_value(heim_array_t array, heim_object_t value)
{
    return CFArrayContainsValue((CFArrayRef)array, CFRangeMake(0, CFArrayGetCount((CFArrayRef)array)), value);
}

heim_dict_t
heim_dict_create(size_t size)
{
    return (heim_dict_t)CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

heim_tid_t
heim_dict_get_type_id(void)
{
    return (heim_tid_t)CFDictionaryGetTypeID();
}

heim_object_t
heim_dict_copy_value(heim_dict_t dict, heim_object_t key)
{
    heim_object_t obj = (heim_object_t)CFDictionaryGetValue((CFDictionaryRef)dict, key);
    if (obj)
	heim_retain(obj);
    return obj;
}

int
heim_dict_set_value(heim_dict_t dict, heim_object_t key, heim_object_t value)
{
    CFDictionarySetValue((CFMutableDictionaryRef)dict, key, value);
    return 0;
}

void
heim_dict_delete_key(heim_dict_t dict, heim_object_t key)
{
    CFDictionaryRemoveValue((CFMutableDictionaryRef)dict, key);
}

struct dict_iter {
    union {
	heim_dict_iterator_f_t func;
	void (^block)(heim_object_t, heim_object_t);
    } u;
    void *arg;
};

static void
dict_iterate_f(const void *key, const void *value, void *context)
{
    struct dict_iter *ctx = context;
    ctx->u.func((heim_object_t)key, (heim_object_t)value, ctx->arg);

}

void
heim_dict_iterate_f(heim_dict_t dict, void *arg, heim_dict_iterator_f_t func)
{
    struct dict_iter ctx = {
	.u.func = func,
	.arg = arg
    };
    CFDictionaryApplyFunction((CFDictionaryRef)dict, dict_iterate_f, &ctx);
}

#ifdef __BLOCKS__

static void
dict_iterate_b(const void *key, const void *value, void *context)
{
    struct dict_iter *ctx = context;
    ctx->u.block((heim_object_t)key, (heim_object_t)value);
    
}

void
heim_dict_iterate(heim_dict_t dict, void (^func)(heim_object_t, heim_object_t))
{
    struct dict_iter ctx = {
	.u.block = func
    };
    CFDictionaryApplyFunction((CFDictionaryRef)dict, dict_iterate_b, &ctx);

}
#endif

heim_string_t
heim_string_create(const char *string)
{
    return (heim_string_t)CFStringCreateWithCString(NULL, string, kCFStringEncodingUTF8);
}

heim_string_t
heim_string_create_with_bytes(const void *data, size_t len)
{
    return (heim_string_t)CFStringCreateWithBytes(NULL, data, len, kCFStringEncodingUTF8, false);
}

heim_tid_t
heim_string_get_type_id(void)
{
    return (heim_tid_t)CFStringGetTypeID();
}

char *
heim_string_copy_utf8(heim_string_t string)
{
    CFIndex len;
    char *str;
    
    str = (char *) CFStringGetCStringPtr((CFStringRef)string, kCFStringEncodingUTF8);
    if (str)
	return strdup(str);

    len = CFStringGetLength((CFStringRef)string);
    len = 1 + CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8);
    str = malloc(len);
    if (str == NULL)
	return NULL;
	
    if (!CFStringGetCString ((CFStringRef)string, str, len, kCFStringEncodingUTF8)) {
	free (str);
	return NULL;
    }
    return str;
}

heim_data_t
heim_data_create(void *indata, size_t len)
{
    return (heim_data_t)CFDataCreate(NULL, indata, len);
}

heim_tid_t
heim_data_get_type_id(void)
{
    return (heim_tid_t)CFDataGetTypeID();
}

const void *
heim_data_get_bytes(heim_data_t data)
{
    return CFDataGetBytePtr((CFDataRef)data);
}

size_t
heim_data_get_length(heim_data_t data)
{
    return CFDataGetLength((CFDataRef)data);
}

static void
heim_alloc_release(CFTypeRef type)
{
    struct heim_base_uniq *mem = (struct heim_base_uniq *)type;
    if (mem->dealloc)
	mem->dealloc(mem);
}

static CFTypeID
uniq_get_type_id(void)
{
    static CFTypeID haid = _kCFRuntimeNotATypeID;
    static dispatch_once_t inited;

    dispatch_once(&inited, ^{
	    static const CFRuntimeClass naclass = {
		0,
		"heim-alloc",
		NULL,
		NULL,
		heim_alloc_release,
		NULL,
		NULL,
		NULL,
		NULL
	    };
	    haid = _CFRuntimeRegisterClass(&naclass);
	});

    return haid;
}

heim_object_t
heim_uniq_alloc(size_t size, const char *name, heim_type_dealloc dealloc)
{
    CFTypeID id = uniq_get_type_id();
    struct heim_base_uniq *mem;

    heim_assert(size >= sizeof(struct heim_base_uniq), "uniq: size too small");

    if (id == _kCFRuntimeNotATypeID)
	return NULL;

    mem = _heim_create_cf_instance(id, size, "base-uniq");
    if (mem) {
	mem->dealloc = dealloc;
	mem->name = name;
    }

    return mem;
}

struct heim_error {
    struct heim_base base;
    int error_code;
    heim_string_t msg;
    struct heim_error *next;
};

static void
heim_error_release(CFTypeRef ptr)
{
    struct heim_error *p = (struct heim_error *)ptr;
    heim_release(p->msg);
    heim_release(p->next);
}


static CFTypeID
heim_error_get_type_id(void)
{
    static CFTypeID haid = _kCFRuntimeNotATypeID;
    static dispatch_once_t inited;
    
    dispatch_once(&inited, ^{
	static const CFRuntimeClass naclass = {
	    0,
	    "heim-error",
	    NULL,
	    NULL,
	    heim_error_release,
	    NULL,
	    NULL,
	    NULL,
	    NULL
	};
	haid = _CFRuntimeRegisterClass(&naclass);
    });
    
    return haid;
}


heim_error_t
heim_error_create(int error_code, const char *fmt, ...)
{
    heim_error_t e;
    va_list ap;
    
    va_start(ap, fmt);
    e = heim_error_createv(error_code, fmt, ap);
    va_end(ap);
    
    return e;
}

heim_error_t
heim_error_createv(int error_code, const char *fmt, va_list ap)
{
    CFTypeID id = heim_error_get_type_id();
    heim_error_t e;
    char *str;
    int len;
    
    if (id == _kCFRuntimeNotATypeID)
	return NULL;
    
    str = malloc(1024);
    if (str == NULL)
        return NULL;
    len = vsnprintf(str, 1024, fmt, ap);
    if (len < 0) {
        free(str);
	return NULL;
    }
    
    e = _heim_create_cf_instance(id, sizeof(struct heim_error), "heim-error");
    if (e) {
	e->msg = heim_string_create(str);
	e->error_code = error_code;
	e->next = NULL;
    }
    free(str);
    
    return e;
}

heim_error_t
heim_error_create_enomem(void)
{
    return heim_error_create(ENOMEM, "out of memory");
}

heim_string_t
heim_error_copy_string(heim_error_t error)
{
    /* XXX concat all strings */
    return heim_retain(error->msg);
}

int
heim_error_get_code(heim_error_t error)
{
    return error->error_code;
}

heim_error_t
heim_error_append(heim_error_t top, heim_error_t append)
{
    if (top->next)
	heim_release(top->next);
    top->next = heim_retain(append);
    return top;
}

heim_number_t
heim_number_create(int number)
{
    return (heim_number_t)CFNumberCreate(NULL, kCFNumberIntType, &number);
}


heim_tid_t
heim_number_get_type_id(void)
{
    return (heim_tid_t)CFNumberGetTypeID();
}

int
heim_number_get_int(heim_number_t number)
{
    int num = 0;
    CFNumberGetValue((CFNumberRef)number, kCFNumberIntType, &num);
    return num;
}

heim_queue_t
heim_queue_create(const char *name, heim_queue_attr_t attr)
{
    return (heim_queue_t)dispatch_queue_create(name, NULL);
}

void
heim_async_f(heim_queue_t queue, void *ctx, void (*callback)(void *data))
{
    dispatch_async_f((dispatch_queue_t)queue, ctx, callback);
}

heim_sema_t
heim_sema_create(long count)
{
    return (heim_sema_t)dispatch_semaphore_create(count);
}

void
heim_sema_signal(heim_sema_t sema)
{
    dispatch_semaphore_signal((dispatch_semaphore_t)sema);
}

void
heim_sema_wait(heim_sema_t sema, time_t t)
{
    dispatch_semaphore_wait((dispatch_semaphore_t)sema, 
			    dispatch_time(DISPATCH_TIME_NOW, ((long long)t) * NSEC_PER_SEC));
}
