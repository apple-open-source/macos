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


/*
 * opensslUtils.h - Support for ssleay-derived crypto modules
 */
 
#include <openssl/rand.h>
#include <openssl/crypto_legacy.h>
#include <openssl/opensslerr.h>
#include <openssl/sha.h>
#include <openssl/rsa_legacy.h>
#include <openssl/dsa_legacy.h>
#include <openssl/dh_legacy.h>
#include <openssl/opensslerr.h>
#include <security_utilities/debugging.h>
#include <Security/cssmerr.h>
#include "opensslUtils.h"
#include <YarrowConnection.h>
#include <AppleCSPUtils.h>
#include <security_utilities/logging.h>

#define sslUtilsDebug(args...)	secinfo("sslUtils", ## args)

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

/* Clear openssl error stack. */
void clearOpensslErrors()
{
	while(ERR_get_error()) 
		;
}

/*
 * Log error info. Returns the error code we pop off the error queue.
 */
unsigned long logSslErrInfo(const char *op)
{
	unsigned long e = ERR_get_error();
	
	/* flush out subsequent errors; we only want the first one */
	clearOpensslErrors();
	
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

void throwRsaDsa(
	const char *op)
{
	unsigned long e = logSslErrInfo(op);
	CSSM_RETURN cerr = CSSM_OK;
	
	/* try to parse into something meaningful */
	int reason = ERR_GET_REASON(e);
	int lib = ERR_GET_LIB(e);
	
	/* first try the global ones */
	switch(reason) {
		case ERR_R_MALLOC_FAILURE:
			cerr = CSSMERR_CSP_MEMORY_ERROR; break;
		case ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED:
			/* internal */ break;
		case ERR_R_PASSED_NULL_PARAMETER:
			cerr = CSSMERR_CSP_INVALID_POINTER; break;
		case ERR_R_NESTED_ASN1_ERROR:
		case ERR_R_BAD_ASN1_OBJECT_HEADER:
		case ERR_R_BAD_GET_ASN1_OBJECT_CALL:
		case ERR_R_EXPECTING_AN_ASN1_SEQUENCE:
		case ERR_R_ASN1_LENGTH_MISMATCH:
		case ERR_R_MISSING_ASN1_EOS:
			/* ASN - shouldn't happen, right? */
			cerr = CSSMERR_CSP_INTERNAL_ERROR; break;
		default:
			break;
	}
	if(cerr != CSSM_OK) {
		CssmError::throwMe(cerr);
	}
	
	/* now the lib-specific ones */
	switch(lib) {
		case ERR_R_BN_LIB:
			/* all indicate serious internal error...right? */
			cerr = CSSMERR_CSP_INTERNAL_ERROR; break;
		case ERR_R_RSA_LIB:
			switch(reason) {
				case RSA_R_ALGORITHM_MISMATCH:
					cerr = CSSMERR_CSP_ALGID_MISMATCH; break;
				case RSA_R_BAD_SIGNATURE:
					cerr = CSSMERR_CSP_VERIFY_FAILED; break;
				case RSA_R_DATA_TOO_LARGE:
				case RSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE:
				case RSA_R_DATA_TOO_SMALL:
				case RSA_R_DATA_TOO_SMALL_FOR_KEY_SIZE:
				case RSA_R_DIGEST_TOO_BIG_FOR_RSA_KEY:
					cerr = CSSMERR_CSP_INPUT_LENGTH_ERROR; break;
				case RSA_R_KEY_SIZE_TOO_SMALL:
					cerr = CSSMERR_CSP_INVALID_ATTR_KEY_LENGTH; break;
				case RSA_R_PADDING_CHECK_FAILED:
				case RSA_R_BLOCK_TYPE_IS_NOT_01:
				case RSA_R_BLOCK_TYPE_IS_NOT_02:
				case RSA_R_DATA_GREATER_THAN_MOD_LEN:
				case RSA_R_BAD_PAD_BYTE_COUNT:
					cerr = CSSMERR_CSP_INVALID_DATA; break;
				case RSA_R_RSA_OPERATIONS_NOT_SUPPORTED:
					cerr = CSSMERR_CSP_FUNCTION_NOT_IMPLEMENTED; break;
				case RSA_R_UNKNOWN_ALGORITHM_TYPE:
					cerr = CSSMERR_CSP_INVALID_ALGORITHM; break;
				case RSA_R_WRONG_SIGNATURE_LENGTH:
					cerr = CSSMERR_CSP_VERIFY_FAILED; break;
				case RSA_R_SSLV3_ROLLBACK_ATTACK:
					cerr = CSSMERR_CSP_APPLE_SSLv2_ROLLBACK; break;
				default:
					cerr = CSSMERR_CSP_INTERNAL_ERROR; break;
			}
			break;
		case ERR_R_DSA_LIB:
			switch(reason) {
				case DSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE:
					cerr = CSSMERR_CSP_INPUT_LENGTH_ERROR; break;
				default:
					cerr = CSSMERR_CSP_INTERNAL_ERROR; break;
			}
			break;
		case ERR_R_DH_LIB:
			/* actually none of the DH errors make sense at the CDSA level */
			cerr = CSSMERR_CSP_INTERNAL_ERROR; 
			break;
		default:
			cerr = CSSMERR_CSP_INTERNAL_ERROR; break;
	}
	CssmError::throwMe(cerr);
}

/*
 * given an openssl-style error, throw appropriate CssmError.
 */
void throwOpensslErr(int irtn)
{
	/* FIXME */
	CssmError::throwMe(CSSMERR_CSP_INTERNAL_ERROR);
}

