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

#include "config.h"
#include <ctype.h>
#include <commonp.h>
#include <string.h>
#include <rpcsvc.h>
#include <stdarg.h>

#if HAVE_CRASHREPORTERCLIENT_H

#include <CrashReporterClient.h>

#elif defined(__APPLE__)

/*
 * The following symbol is reference by Crash Reporter symbolicly
 * (instead of through undefined references. To get strip(1) to know
 * this symbol is not to be stripped it needs to have the
 * REFERENCED_DYNAMICALLY bit (0x10) set.  This would have been done
 * automaticly by ld(1) if this symbol were referenced through undefined
 * symbols.
 *
 * NOTE: this is an unsupported interface and the CrashReporter team reserve
 * the right to change it at any time.
 */
char *__crashreporter_info__ = NULL;
asm(".desc ___crashreporter_info__, 0x10");

#define CRSetCrashLogMessage(msg) do { \
    __crashreporter_info__ = (msg); \
} while (0)

#else

/* No CrashReporter support, spit it out to stderr and hope someone is
 * watching.
 */
#define CRSetCrashLogMessage(msg) do { \
    write(STDERR_FILENO, strlen(msg), msg); \
} while (0)

#endif

/*
dce_svc_handle_t rpc_g_svc_handle;
*/
//DCE_SVC_DEFINE_HANDLE(rpc_g_svc_handle, rpc_g_svc_table, "rpc")

//#define RPC_DCE_SVC_PRINTF(args) rpc_dce_svc_printf(args)

void rpc_dce_svc_printf (
                        const char* file,
                        unsigned int line,
                        const char *format,
                        unsigned32 dbg_switch ATTRIBUTE_UNUSED,
                        unsigned32 sev_action_flags,
                        unsigned32 error_code,
                        ... )
{
    char buff[1024];
    char *s = buff;
    size_t remain = sizeof(buff);
    va_list arg_ptr;
    int cs;

    snprintf (s, remain, "[file %s, line %d] ", file, line);
    s = &buff[strlen(buff)];
    remain = sizeof(buff) - (s - buff);

    snprintf (s, remain, "[flags: 0x%x] ", (unsigned int) sev_action_flags);
    s = &buff[strlen(buff)];
    remain = sizeof(buff) - (s - buff);

    snprintf (s, remain, "[error: 0x%x] ", (unsigned int) error_code);
    s = &buff[strlen(buff)];
    remain = sizeof(buff) - (s - buff);

    va_start (arg_ptr, error_code);
    vsnprintf (s, remain, format, arg_ptr);
    va_end (arg_ptr);

    if ( (sev_action_flags & svc_c_action_abort) ||
        (sev_action_flags & svc_c_action_exit_bad) )
    {
        CRSetCrashLogMessage(buff);
        abort();
    }
    else
    {
        cs = dcethread_enableinterrupt_throw(0);
        dcethread_write (2, buff, strlen (buff));
        dcethread_enableinterrupt_throw(cs);
    }
}

#if 0
/*
 * R P C _ _ S V C _ E P R I N T F
 *
 * Format and print arguments as a serviceability
 * debug message.
 */

PRIVATE int rpc__svc_eprintf ( char *fmt, ... )
{
    char	buf[RPC__SVC_DBG_MSG_SZ];
    va_list	arg_ptr;

    va_start (arg_ptr, fmt);
    vsprintf (buf, fmt, arg_ptr);
    va_end (arg_ptr);
    DCE_SVC_DEBUG((RPC__SVC_HANDLE, rpc_svc_general, RPC__SVC_DBG_LEVEL(0), buf));
    return(0);
}


/*
 * R P C _ _ S V C _ I N I T
 *
 * Do initialization required for serviceability
 */

PRIVATE void rpc__svc_init ( void )
{
    error_status_t status;

    /*
     * Currently, all we have to do is return, since
     * everything is statically registered.
     *
     * But someday we might do something like turn
     * on debug levels corresponding to things set
     * in rpc_g_dbg_switches[], or ...
     */

    /*
     * This silliness is a placeholder, so that we
     * remember to do things differently in the kernel
     * if we ever decide to do more than just return
     * out of this routine.
     */
    return;
}

/*
 * R P C _ _ S V C _ F M T _ D B G _ M S G
 *
 * This routine takes the printf "pargs" passed to
 * the RPC_DBG_PRINTF() macro and formats them
 * into a string that can be handed to DCE_SVC_DEBUG.
 *
 * This is necessary because the pargs are passed
 * in as a single, parenthesized argument -- which
 * also requires that the resulting string be passed
 * back as a pointer return value.
 *
 * The returned pointer must be free()'ed by the
 * caller (see comments at malloc() below).  This
 * should be fairly safe, since this routine should
 * only ever be called by RPC_DBG_PRINTF.
 */

PRIVATE char * rpc__svc_fmt_dbg_msg (char *format, ...)
{
    char            *bptr;
    va_list         arg_ptr;

    /*
     * Using malloc here is ugly but necessary.  The formatted
     * string must be passed back as a pointer return value.  The
     * possibility of recursive calls due to evaluation of pargs
     * (where, e.g., one of the pargs is a call to a routine that
     * calls RPC_DBG_PRINTF) preclude an implementation using a
     * mutex to protect a static buffer.  The potential for infinite
     * recursion precludes allocating memory using internal RPC
     * interfaces, since those interfaces call RPC_DBG_PRINTF.
     */

    if( (bptr = malloc(RPC__SVC_DBG_MSG_SZ*sizeof(char))) == NULL )
    {
        /* die horribly */
        abort();
    }

    va_start (arg_ptr, format);
    vsprintf (bptr, format, arg_ptr);
    va_end (arg_ptr);

    return( bptr );
}
#endif	/* DEBUG */
