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
#include <syslog.h>

/*
 *
 */

#ifdef __APPLE_PRIVATE__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wlanguage-extension-token"

const char *__crashreporter_info__ = NULL;
asm(".desc ___crashreporter_info__, 0x10");

#pragma clang diagnostic pop
#endif


/**
 * Abort and log the failure (using syslog)
 */

void
heim_abort(const char *fmt, ...)
    HEIMDAL_NORETURN_ATTRIBUTE
    HEIMDAL_PRINTF_ATTRIBUTE((printf, 1, 2))
{
    va_list ap;
    va_start(ap, fmt);
    heim_abortv(fmt, ap);
    va_end(ap);
}

/**
 * Abort and log the failure (using syslog)
 */

void
heim_abortv(const char *fmt, va_list ap)
    HEIMDAL_NORETURN_ATTRIBUTE
    HEIMDAL_PRINTF_ATTRIBUTE((printf, 1, 0))
{
    char *str = NULL;
    int ret;
    
    ret = vasprintf(&str, fmt, ap);
    if (ret > 0 && str) {
	syslog(LOG_ERR, "heim_abort: %s", str);
	
#ifdef __APPLE_PRIVATE__
	__crashreporter_info__ = str;
#endif
    }
    abort();
}
