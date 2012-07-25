/* Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE COMPUTER, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE COMPUTER,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************
 *
 * enc64.h - encode/decode in 64-char IA5 format, per RFC 1421
 *
 * Revision History
 * ----------------
 *  9 Oct 96	Doug Mitchell at NeXT
 *	Created.
 */

#ifndef	_CK_ENC64_H_
#define _CK_ENC64_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Given input buffer inbuf, length inlen, decode from 64-char IA5 format to
 * binary. Result is fmalloced and returned; its length is returned in *outlen.
 * NULL return indicates corrupted input.
 */
unsigned char *enc64(const unsigned char *inbuf,
	unsigned inlen,
	unsigned *outlen);		// RETURNED

/*
 * Enc64, with embedded newlines every lineLen in result. A newline is
 * the Microsoft-style "\r\n".
 */
unsigned char *enc64WithLines(const unsigned char *inbuf,
	unsigned inlen,
	unsigned linelen,
	unsigned *outlen);		// RETURNED

/*
 * Given input buffer inbuf, length inlen, decode from 64-char IA5 format to
 * binary. Result is fmalloced and returned; its length is returned in *outlen.
 * NULL return indicates corrupted input. All whitespace in inbuf is
 * ignored.
 */
unsigned char *dec64(const unsigned char *inbuf,
	unsigned inlen,
	unsigned *outlen);

/*
 * Determine if specified input data is valid enc64 format. Returns 1
 * if valid, 0 if not.
 */
int isValidEnc64(const unsigned char *inbuf,
	unsigned inbufLen);

#ifdef __cplusplus
}
#endif

#endif	/*_CK_ENC64_H_*/
