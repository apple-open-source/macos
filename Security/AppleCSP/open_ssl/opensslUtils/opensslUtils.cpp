/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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


/*
 * opensslUtils.h - Support for ssleay-derived crypto modules
 */
 
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <Security/debugging.h>
#include <Security/cssmerr.h>
#include "opensslUtils.h"
#include <AppleCSP/YarrowConnection.h>
#include <AppleCSP/AppleCSPUtils.h>
#include <Security/logging.h>

#define sslUtilsDebug(args...)	debug("sslUtils", ## args)

openSslException::openSslException(
	int irtn, 
	const char *op)
		: mIrtn(irtn)
{ 
	if(op) {
		char buf[300];
		ERR_error_string(irtn, buf);
		sslUtilsDebug("%s: %s\n", op, buf);
	}
}

/* these are replacements for the ones in ssleay */
#define DUMP_RAND_BYTES	0

static int randDex = 1;

int  RAND_bytes(unsigned char *buf,int num)
{
	try {
		cspGetRandomBytes(buf, (unsigned)num);
	}
	catch(...) {
		/* that can only mean Yarrow failure, which we really need to 
		 * cut some slack for */
		Security::Syslog::error("Apple CSP: yarrow failure");
		for(int i=0; i<num; i++) {
			buf[i] = (i*3) + randDex++;
		}
	}
	return 1;
}

int  RAND_pseudo_bytes(unsigned char *buf,int num)
{
	return RAND_bytes(buf, num);
}

void RAND_add(const void *buf,int num,double entropy)
{
	try {
		cspAddEntropy(buf, (unsigned)num);
	}
	catch(...) {
	}
}

/* replacement for mem_dbg.c */
int CRYPTO_mem_ctrl(int mode)
{
	return 0;
}

/*
 * Log error info. Returns the error code we pop off the error queue.
 */
unsigned long logSslErrInfo(const char *op)
{
	unsigned long e = ERR_get_error();
	char outbuf[1024];
	ERR_error_string(e, outbuf);
	if(op) {
		Security::Syslog::error("Apple CSP %s: %s", op, outbuf);
	}
	else {
		Security::Syslog::error("Apple CSP %s", outbuf);
	}
	return e;
}

/*
 * Replacement for same function in openssl's sha.c, which we don't link against. 
 * The only place this is used is in DSA_generate_parameters().
 */
unsigned char *SHA1(const unsigned char *d, unsigned long n,unsigned char *md)
{
	if(md == NULL) {
		sslUtilsDebug("SHA1 with NULL md");
		CssmError::throwMe(CSSMERR_CSP_INTERNAL_ERROR);
	}
	cspGenSha1Hash(d, n, md);
	return md;
}

