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
#include <stdlib.h>
#include <string.h>
#include <err.h>

int
main(int argc, char **argv)
{
    KLLoginOptions options;
    KLPrincipal princ;
    KLStatus ret;
    KLBoolean foundV5;
    char *user;
    char *password;
    char *buffer;

    if (argc != 3)
	errx(1, "argc != 2");

    user = argv[1];
    password = strdup(argv[2]);

    printf("test NULL argument\n");
    ret = KLCreatePrincipalFromString(NULL, kerberosVersion_V5, &princ);
    if (ret == 0)
	errx(1, "KLCreatePrincipalFromString: %d", ret);

    printf("create principal\n");
    ret = KLCreatePrincipalFromString(user,
				      kerberosVersion_V5, &princ);
    if (ret)
	errx(1, "KLCreatePrincipalFromString: %d", ret);

    printf("acquire cred\n");
    ret = KLAcquireNewInitialTicketsWithPassword(princ, NULL, password, NULL);
    if (ret)
	errx(1, "KLAcquireTicketsWithPassword1: %d", ret);

    printf("get valid tickets\n");
    ret = KLCacheHasValidTickets(princ, kerberosVersion_V5, &foundV5, NULL, NULL);
    if (ret)
	errx(1, "KLCacheHasValidTickets failed1");
    else if (!foundV5)
	errx(1, "found no valid tickets");

#if 0
    ret = KLAcquireNewInitialTickets(princ, NULL, NULL, NULL);
    if (ret)
	errx(1, "KLAcquireTickets: %d", ret);

    KLDestroyTickets(princ);

    printf("get valid tickets\n");
    ret = KLCacheHasValidTickets(princ, kerberosVersion_V5, &foundV5, NULL, NULL);
    if (ret)
	errx(1, "KLCacheHasValidTickets failed1 dead");
    else if (foundV5)
	errx(1, "found valid tickets!");
#endif

    KLCreateLoginOptions(&options);
    KLLoginOptionsSetRenewableLifetime(options, 3600 * 24 * 7);

    ret = KLAcquireNewInitialTicketsWithPassword(princ, options, password, NULL);
    if (ret)
	errx(1, "KLAcquireTicketsWithPassword2: %d", ret);

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

    printf("display string from princ\n");
    ret = KLGetDisplayStringFromPrincipal(princ, kerberosVersion_V5, &buffer);
    if (ret)
	errx(1, "KLGetDisplayStringFromPrincipal: %d", ret);
    free(buffer);

    printf("string from princ\n");
    ret = KLGetStringFromPrincipal(princ, kerberosVersion_V5, &buffer);
    if (ret)
	errx(1, "KLGetStringFromPrincipal: %d", ret);
    free(buffer);

    {
    	char *name;
	char *inst;
	char *realm;
        printf("triplet from princ\n");
        ret = KLGetTripletFromPrincipal(princ, &name, &inst, &realm);
        if (ret)
	    errx(1, "KLCancelAllDialogs: %d", ret);
	free(name);
	free(inst);
	free(realm);
    }


    printf("cancel dialogs\n");
    ret = KLCancelAllDialogs();
    if (ret)
	errx(1, "KLCancelAllDialogs: %d", ret);

    printf("dispose string\n");
    ret = KLDisposeString(password);
    if (ret)
	errx(1, "KLDisposeString: %d", ret);

    KLDestroyTickets(princ);
    KLDisposePrincipal(princ);

    return 0;
}
