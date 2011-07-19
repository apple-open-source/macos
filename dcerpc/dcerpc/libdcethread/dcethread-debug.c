/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Portions of this software have been released under the following terms:
 *
 * (c) Copyright 1989-1993 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989-1993 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989-1993 DIGITAL EQUIPMENT CORPORATION
 *
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 * permission to use, copy, modify, and distribute this file for any
 * purpose is hereby granted without fee, provided that the above
 * copyright notices and this notice appears in all source code copies,
 * and that none of the names of Open Software Foundation, Inc., Hewlett-
 * Packard Company or Digital Equipment Corporation be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  Neither Open Software
 * Foundation, Inc., Hewlett-Packard Company nor Digital
 * Equipment Corporation makes any representations about the suitability
 * of this software for any purpose.
 *
 * Copyright (c) 2007, Novell, Inc. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Novell Inc. nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <config.h>
#include "dcethread-debug.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

void
dcethread__default_log_callback (const char* file, unsigned int line,
	int level, const char* str, void* data ATTRIBUTE_UNUSED)
{
    const char* level_name = NULL;

    switch (level)
    {
    case DCETHREAD_DEBUG_ERROR:
        level_name = "ERROR";
        break;
    case DCETHREAD_DEBUG_WARNING:
        level_name = "WARNING";
        break;
    case DCETHREAD_DEBUG_INFO:
        level_name = "INFO";
        break;
    case DCETHREAD_DEBUG_VERBOSE:
        level_name = "VERBOSE";
        break;
    case DCETHREAD_DEBUG_TRACE:
        level_name = "TRACE";
        break;
    default:
        level_name = "UNKNOWN";
        break;
    }

    pthread_mutex_lock(&log_lock);
    fprintf(stderr, "dcethread-%s %s:%i: %s\n", level_name, file, line, str);
    if (level == DCETHREAD_DEBUG_ERROR)
        abort();
    pthread_mutex_unlock(&log_lock);
}

static void (*log_callback) (const char* file, unsigned int line, int level, const char* str, void* data) = NULL;
static void *log_callback_data = NULL;

void
dcethread__debug_set_callback(void (*cb) (const char*, unsigned int, int, const char*, void* data), void* data)
{
    log_callback = cb;
    log_callback_data = data;
}

#ifndef HAVE_VASPRINTF
static char *
my_vasprintf(const char* format, va_list args)
{
    char *smallBuffer;
    unsigned int bufsize;
    int requiredLength;
    int newRequiredLength;
    char* outputString = NULL;
    va_list args2;

    va_copy(args2, args);

    bufsize = 4;
    /* Use a small buffer in case libc does not like NULL */
    do
    {
        smallBuffer = malloc(bufsize);

	if (!smallBuffer)
	{
	    return NULL;
	}

        requiredLength = vsnprintf(smallBuffer, bufsize, format, args);
        if (requiredLength < 0)
        {
            bufsize *= 2;
        }
	free(smallBuffer);
    } while (requiredLength < 0);

    if (requiredLength >= (int)(0xFFFFFFFF - 1))
    {
        return NULL;
    }

    outputString = malloc(requiredLength + 2);

    if (!outputString)
    {
	return NULL;
    }

    newRequiredLength = vsnprintf(outputString, requiredLength + 1, format, args2);
    if (newRequiredLength < 0)
    {
	free(outputString);
	return NULL;
    }

    va_end(args2);

    return outputString;
}
#endif /* HAVE_VASPRINTF */

void
dcethread__debug_printf(const char* file, unsigned int line, int level, const char* fmt, ...)
{
    va_list ap;
    char* str = NULL;

    if (!log_callback)
	return;

    va_start(ap, fmt);

#if HAVE_VASPRINTF
    vasprintf(&str, fmt, ap);
#else
    str = my_vasprintf(fmt, ap);
#endif

    if (str)
    {
	log_callback(file, line, level, str, log_callback_data);
	free(str);
    }

    va_end(ap);
}
