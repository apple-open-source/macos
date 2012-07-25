/*
 * Copyright (c) 2008-2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2008-2010 Apple Inc. All rights reserved.
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

#include "mit-KerberosLogin.h"
#include <stdio.h>
#include <err.h>

int
main(int argc, char **argv)
{
    KLLoginOptions options;
    KLPrincipal princ;
    KLStatus ret;
    KLBoolean foundV5;
    KLIdleCallback idlecall;
    KLRefCon refcon;

    if (argc != 2)
	errx(1, "argc != 2");

    printf("test NULL argument\n");
    ret = KLCreatePrincipalFromString(NULL, kerberosVersion_V5, &princ);
    if (ret == 0)
	errx(1, "KLCreatePrincipalFromString: %d", ret);

    printf("create principal\n");
    ret = KLCreatePrincipalFromString(argv[1],
				      kerberosVersion_V5, &princ);
    if (ret)
	errx(1, "KLCreatePrincipalFromString: %d", ret);

    printf("acquire cred\n");

    KLCreateLoginOptions(&options);
    KLLoginOptionsSetRenewableLifetime(options, 3600 * 24 * 7);

    ret = KLAcquireInitialTickets(princ, options, NULL, NULL);
    if (ret)
	errx(1, "KLAcquireTicketsWithPassword: %d", ret);

    KLDisposeLoginOptions(options);

    printf("get valid tickets\n");
    ret = KLCacheHasValidTickets(princ, kerberosVersion_V5, &foundV5, NULL, NULL);
    if (ret)
	errx(1, "KLCacheHasValidTickets failed");
    else if (!foundV5)
	errx(1, "found no valid tickets");

    printf("renew tickets\n");
    ret = KLRenewInitialTickets(princ, NULL, NULL, NULL);
    if (ret)
	errx(1, "KLRenewInitialTickets: %d", ret);
    KLDisposePrincipal(princ);

    printf("test callbacks\n");
    ret = KLGetIdleCallback(&idlecall, &refcon);
    if (ret != klNoErr)
	errx(1, "KLGetIdleCallback: %d", ret);

    ret = KLSetIdleCallback(NULL, refcon);
    if (ret != klNoErr)
	errx(1, "KLSetIdleCallback: %d", ret);


    return 0;
}
