/*
 * Copyright (c) 1999-2001,2005-2008,2010-2012 Apple Inc. All Rights Reserved.
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

/*
 * sslUtils.c - Misc. OS independant SSL utility functions
 */

/* THIS FILE CONTAINS KERNEL CODE */

#include "sslUtils.h"
#include "tls_types.h"
#include "sslDebug.h"
#include "sslMemory.h"

#include <AssertMacros.h>
#include <stdarg.h>

#if KERNEL
#include <IOKit/IOLib.h>
#else
#include <syslog.h>
#include <pthread.h>
#include <stdlib.h>
#include <strings.h>
#include <xpc/xpc.h>
#include <xpc/private.h>
#endif



#if SSL_DEBUG
void SSLDump(const unsigned char *data, unsigned long len)
{
    unsigned long i;
    for(i=0;i<len;i++)
    {
        if((i&0xf)==0) printf("%04lx :",i);
        printf(" %02x", data[i]);
        if((i&0xf)==0xf) printf("\n");
    }
    printf("\n");
}
#endif

unsigned int
SSLDecodeInt(const uint8_t *p, size_t length)
{
    unsigned int val = 0;
    check(length > 0 && length <= 4); //anything else would be an internal error.
    while (length--)
        val = (val << 8) | *p++;
    return val;
}

uint8_t *
SSLEncodeInt(uint8_t *p, size_t value, size_t length)
{
    unsigned char *retVal = p + length; /* Return pointer to char after int */
    check(length > 0 && length <= 4);  //anything else would be an internal error.
    while (length--)                    /* Assemble backwards */
    {   p[length] = (uint8_t)value;     /* Implicit masking to low byte */
        value >>= 8;
    }
    return retVal;
}

size_t
SSLDecodeSize(const uint8_t *p, size_t length)
{
    unsigned int val = 0;
    check(length > 0 && length <= 4); //anything else would be an internal error.
    while (length--)
        val = (val << 8) | *p++;
    return val;
}

uint8_t *
SSLEncodeSize(uint8_t *p, size_t value, size_t length)
{
    unsigned char *retVal = p + length; /* Return pointer to char after int */
    check(length > 0 && length <= 4);  //anything else would be an internal error.
    while (length--)                    /* Assemble backwards */
    {   p[length] = (uint8_t)value;     /* Implicit masking to low byte */
        value >>= 8;
    }
    return retVal;
}


uint8_t *
SSLEncodeUInt64(uint8_t *p, uint64_t value)
{
    p = SSLEncodeInt(p, (value>>32)&0xffffffff, 4);
    return SSLEncodeInt(p, value&0xffffffff, 4);
}


void
IncrementUInt64(uint64_t *v)
{
    (*v)++;
}

uint64_t
SSLDecodeUInt64(const uint8_t *p, size_t length)
{
    uint64_t val = 0;
    check(length > 0 && length <= 8);
    while (length--)
        val = (val << 8) | *p++;
    return val;
}

bool __ssl_debug_all = false;

#if !KERNEL

static xpc_object_t gDebugScope = NULL;
static struct ssl_logger {
    __ssl_debug_function function;
    void *context;
    struct ssl_logger *next;
} *gDebugLoggers;

static void
__security_debug_init(void)
{
    xpc_object_t settings = NULL;
    void *data = NULL;
    struct stat sb;
    int fd = -1;

    fd = open("/Library/Preferences/com.apple.security.plist", O_RDONLY);
    require_quiet(fd != -1, out);

    require_quiet(fstat(fd, &sb) != -1, out);
    require_quiet(sb.st_size < 100000, out);

    data = malloc((size_t)sb.st_size);
    require_quiet(data != NULL, out);

    require_quiet(sb.st_size == read(fd, data, (size_t)sb.st_size), out);

    settings = xpc_create_from_plist(data, (size_t)sb.st_size);
    require_quiet(settings, out);

    gDebugScope = xpc_dictionary_get_value(settings, "SSLDebugScope");
    require_quiet(gDebugScope != NULL, out);

    if (xpc_get_type(gDebugScope) == XPC_TYPE_DICTIONARY) {
        xpc_retain(gDebugScope);
    } else if (xpc_get_type(gDebugScope) == XPC_TYPE_BOOL) {
        __ssl_debug_all = xpc_bool_get_value(gDebugScope);
        gDebugScope = NULL;
    }

out:
    if (settings)
        xpc_release(settings);
    if (data)
        free(data);
    if (fd != -1)
        close(fd);
}

void
__ssl_add_debug_logger(__ssl_debug_function function, void *ctx)
{
    struct ssl_logger *logger = sslMalloc(sizeof(*logger));
    logger->function = function;
    logger->context = ctx;

    logger->next = gDebugLoggers;
    gDebugLoggers = logger;
}

bool __ssl_debug_enabled(const char *scope)
{
    static pthread_once_t __security_debug_once = PTHREAD_ONCE_INIT;

    pthread_once(&__security_debug_once, __security_debug_init);

    /* Check if scope is enabled. */
    if (!__ssl_debug_all && (gDebugScope == NULL || xpc_dictionary_get_value(gDebugScope, scope) == NULL))
        return false;

    return true;
}
#endif /* !KERNEL */


void __ssl_debug(const char *scope, const char *function,
                 const char *file, int line, const char *format, ...)
{
    va_list args;
#if KERNEL
    if (__ssl_debug_all) {
        IOLog("[%s] %s: ", scope, function);
        va_start(args, format);
        IOLogv(format, args);
        IOLog("\n");
    }
#else /* !KERNEL */


    char *str = NULL;
    va_start(args, format);
    vasprintf(&str, format, args); //FIXME: sslVASPRINTF ?
    if (str) {

        if (__ssl_debug_enabled(scope))
            syslog(LOG_WARNING, "[%s] %s: %s", scope, function, str);

        struct ssl_logger *logger = gDebugLoggers;
        while (logger) {
            logger->function(logger->context, scope, function, str);
            logger = logger->next;
        }
        sslFree(str);
    }
#endif /* !KERNEL */
    va_end(args);
}


#if SSL_DEBUG

const char *protocolVersStr(tls_protocol_version prot)
{
	switch(prot) {
        case tls_protocol_version_Undertermined: return "tls_protocol_version_Undertermined";
        case tls_protocol_version_SSL_3: return "tls_protocol_version_SSL_3";
        case tls_protocol_version_TLS_1_0: return "tls_protocol_version_TLS_1_0";
        case tls_protocol_version_TLS_1_1: return "tls_protocol_version_TLS_1_1";
        case tls_protocol_version_TLS_1_2: return "tls_protocol_version_TLS_1_2";
        default: sslErrorLog("protocolVersStr: bad prot\n"); return "BAD PROTOCOL";
 	}
 	return NULL;	/* NOT REACHED */
}
#endif	/* SSL_DEBUG */



