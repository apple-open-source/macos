/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


#include <openssl/bn.h>
#include <openssl/rand.h>

static int rand(n)
{
    unsigned char x[2];
    RAND_pseudo_bytes(x,2);
    return (x[0] + 2*x[1]);
}

static void bug(char *m, BIGNUM *a, BIGNUM *b)
{
    printf("%s!\na=",m);
    BN_print_fp(stdout, a);
    printf("\nb=");
    BN_print_fp(stdout, b);
    printf("\n");
    fflush(stdout);
}

main()
{
    BIGNUM *a=BN_new(), *b=BN_new(), *c=BN_new(), *d=BN_new(),
	*C=BN_new(), *D=BN_new();
    BN_RECP_CTX *recp=BN_RECP_CTX_new();
    BN_CTX *ctx=BN_CTX_new();

    for(;;) {
	BN_pseudo_rand(a,rand(),0,0);
	BN_pseudo_rand(b,rand(),0,0);
	if (BN_is_zero(b)) continue;

	BN_RECP_CTX_set(recp,b,ctx);
	if (BN_div(C,D,a,b,ctx) != 1)
	    bug("BN_div failed",a,b);
	if (BN_div_recp(c,d,a,recp,ctx) != 1)
	    bug("BN_div_recp failed",a,b);
	else if (BN_cmp(c,C) != 0 || BN_cmp(c,C) != 0)
	    bug("mismatch",a,b);
    }
}
