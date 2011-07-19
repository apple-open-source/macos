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

#include <stdio.h>
#include <stdlib.h>

#include <CoreFoundation/CoreFoundation.h>
#include <krb5.h>

#include "test.h"

/* From Krb4DeprecatedAPIs.c */
struct ktext {
    int     length;
    unsigned char dat[MAX_KTXT_LEN];
    unsigned long mbz;
};

int krb_get_cred(char*, char*, char*, void*);
const char * krb_get_err_text(int);
int krb_get_lrealm (char*, int);
int krb_get_tf_fullname (const char*,char*, char*, char*);
int krb_mk_req(struct ktext, char*, char*, char*, UInt32);
char *krb_realmofhost(char*);
const char *tkt_string (void);
void com_err(const char *progname, errcode_t code, const char *format, ...);

typedef unsigned char des_cblock[8];
typedef des_cblock mit_des_cblock;
#define DES_INT32 int
unsigned long des_quad_cksum(const unsigned char *in, unsigned DES_INT32 *out, long length, int out_count, mit_des_cblock *c_seed);

int main(int argc, char **argv)
{
	struct ktext ss;

	VERIFY_DEPRECATED_I(
		"krb_get_cred",
		krb_get_cred(NULL,NULL,NULL,NULL));

	VERIFY_DEPRECATED_S(
		"krb_get_err_text",
		krb_get_err_text(1));

	VERIFY_DEPRECATED_I(
		"krb_get_lrealm",
		krb_get_lrealm(NULL, 1));

	VERIFY_DEPRECATED_I(
		"krb_get_tf_fullname",
		krb_get_tf_fullname(NULL,NULL,NULL,NULL));

	VERIFY_DEPRECATED_I(
		"krb_mk_req",
		krb_mk_req(ss, NULL, NULL, NULL, 0));

	VERIFY_DEPRECATED_S(
		"krb_realmofhost",
		krb_realmofhost(NULL));

	com_err("program", 0, "format");

	/*
	VERIFY_DEPRECATED_S(
		"des_quad_cksum",
		des_quad_chsum(NULL, 0, 0, 0, NULL));
	*/
	VERIFY_DEPRECATED_S(
		"tkt_string",
		tkt_string());

	printf("Test completed.\n");
	return 0;
}
